#include "hhros2_motor_runtime/hhros2_ecat_l0_backend.hpp"
#include "probe/single_shot_probe.h"
#include <cmath>
#include <iostream>
#include "hhros2_log/log.h"
#include <Eigen/Dense>

namespace hhros2_l0 {

using Eigen::Vector2d;
using Eigen::Matrix2d;
using hhros2_mechanisms::FootSide;

EcatL0Backend::EcatL0Backend()
{
    // 初始化踝关节缓存
    std::fill(std::begin(motor_ankle_cmd_), std::end(motor_ankle_cmd_), hhros2::shm::JointCommand{});
    std::fill(std::begin(joint_q_), std::end(joint_q_), 0.0);
    std::fill(std::begin(joint_dq_), std::end(joint_dq_), 0.0);
    std::fill(std::begin(joint_tau_), std::end(joint_tau_), 0.0);
    std::fill(std::begin(motor_q_), std::end(motor_q_), 0.0);
    std::fill(std::begin(motor_dq_), std::end(motor_dq_), 0.0);
    std::fill(std::begin(motor_tau_), std::end(motor_tau_), 0.0);
}

bool EcatL0Backend::init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_.load()) {
        return true;
    }

    try {
        LOG_INFO(LogType::MOTORLOG,"[EcatL0Backend] Initializing with %d masters", 
                     ti5_encos::kMasterNumber);

        for (int i = 1; i < ti5_encos::kMasterNumber; i++)
        {
            config_.masters[i].eni_file = ti5_encos::bundled_eni_file_for_master(i);
        }

        // 创建runtime，使用默认配置，EcatL0Backend 只负责使用配置，不负责配置内容
        runtime_ = std::make_unique<ti5_encos::ThreeMasterTi5EncosRuntime>(config_);
        
        // 初始化
        std::string error;
        if (!runtime_->initialize(&error)) {
            LOG_ERROR(LogType::MOTORLOG,"[EcatL0Backend] Initialize failed: %s", error.c_str());
            return false;
        }

        // 启动
        if (!runtime_->start(&error)) {
            LOG_ERROR(LogType::MOTORLOG,"[EcatL0Backend] Start failed: %s", error.c_str());
            return false;
        }

        // 默认不上使能，在hhros2_l0_runtime_main.cpp中循环控制
        runtime_->set_motor_enable(false);
        
        initialized_.store(true);
        LOG_INFO(LogType::MOTORLOG,"[EcatL0Backend] Initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(LogType::MOTORLOG,"[EcatL0Backend] Exception in init: %s", e.what());
        return false;
    }
}

// 后端应用程序，发送指令给电机
void EcatL0Backend::apply(const hhros2::shm::JointCommand* cmd, int count, bool enabled)
{
    if (!initialized_.load() || !runtime_) {
        return;
    }

    
    /***debug log start***/
    // 记录 L0 读取共享内存的时刻
    auto *trace = single_shot_probe::map_trace();
    if (trace && single_shot_probe::ready(trace) &&
        trace->l0_read_shm_ns == 0U) {
        trace->l0_read_shm_ns = single_shot_probe::now_ns();
    }
    /***debug log end***/

    // 更新使能状态
    if (enabled_ != enabled) {
        enabled_ = enabled;
        runtime_->set_motor_enable(enabled);
        LOG_DEBUG(LogType::MOTORLOG, "[EcatL0Backend] Motor enable set to %d", enabled);
    }

    const int max_count = std::min(count, hhros2::shm::kJointCount);

    if (max_count > kRightAnkleRoll) {
        left_ankle_cmd_.pitch_pos = cmd[kLeftAnklePitch].position;
        left_ankle_cmd_.pitch_vel = cmd[kLeftAnklePitch].velocity;
        left_ankle_cmd_.pitch_eff = cmd[kLeftAnklePitch].effort;
        left_ankle_cmd_.pitch_kp  = cmd[kLeftAnklePitch].kp;
        left_ankle_cmd_.pitch_kd  = cmd[kLeftAnklePitch].kd;
        left_ankle_cmd_.roll_pos  = cmd[kLeftAnkleRoll].position;
        left_ankle_cmd_.roll_vel  = cmd[kLeftAnkleRoll].velocity;
        left_ankle_cmd_.roll_eff  = cmd[kLeftAnkleRoll].effort;
        left_ankle_cmd_.roll_kp   = cmd[kLeftAnkleRoll].kp;
        left_ankle_cmd_.roll_kd   = cmd[kLeftAnkleRoll].kd;

        right_ankle_cmd_.pitch_pos = cmd[kRightAnklePitch].position;
        right_ankle_cmd_.pitch_vel = cmd[kRightAnklePitch].velocity;
        right_ankle_cmd_.pitch_eff = cmd[kRightAnklePitch].effort;
        right_ankle_cmd_.pitch_kp  = cmd[kRightAnklePitch].kp;
        right_ankle_cmd_.pitch_kd  = cmd[kRightAnklePitch].kd;
        right_ankle_cmd_.roll_pos  = cmd[kRightAnkleRoll].position;
        right_ankle_cmd_.roll_vel  = cmd[kRightAnkleRoll].velocity;
        right_ankle_cmd_.roll_eff  = cmd[kRightAnkleRoll].effort;
        right_ankle_cmd_.roll_kp   = cmd[kRightAnkleRoll].kp;
        right_ankle_cmd_.roll_kd   = cmd[kRightAnkleRoll].kd;
        sync_motor_commands_from_joint_commands();
    }

    for (const auto & wiring : joint_wiring::kPhysicalMotorMap) {
        const std::size_t logical = wiring.logical_joint_index;
        if (static_cast<int>(logical) >= max_count) continue;
        ti5_encos::MotorCommand motor_cmd;

        int ankle_index = -1;
        if (logical == joint_wiring::kLeftAnklePitch) ankle_index = 0;
        else if (logical == joint_wiring::kLeftAnkleRoll) ankle_index = 1;
        else if (logical == joint_wiring::kRightAnklePitch) ankle_index = 2;
        else if (logical == joint_wiring::kRightAnkleRoll) ankle_index = 3;

        if (ankle_index >= 0 && max_count > kRightAnkleRoll) {
            motor_cmd.kp = motor_ankle_cmd_[ankle_index].kp;
            motor_cmd.kd = motor_ankle_cmd_[ankle_index].kd;
            motor_cmd.torque = motor_ankle_cmd_[ankle_index].effort;
            motor_cmd.position = motor_ankle_cmd_[ankle_index].position;
            motor_cmd.velocity = motor_ankle_cmd_[ankle_index].velocity;
        } else {
            motor_cmd.kp = cmd[logical].kp;
            motor_cmd.kd = cmd[logical].kd;
            motor_cmd.torque = cmd[logical].effort;
            motor_cmd.position = cmd[logical].position;
            motor_cmd.velocity = cmd[logical].velocity;
        }

        runtime_->set_motor_command(
            wiring.master_id, wiring.motor_index, motor_cmd);
    }
    // static int unm_cont = 0;
    // unm_cont++;
    //  if(unm_cont>100)
    // {
    //     std::cout << "motor_cmd kLeftAnklePitch:" << motor_ankle_cmd_[0].position << std::endl;
    //     std::cout << "motor_cmd kLeftAnkleRoll:" << motor_ankle_cmd_[1].position << std::endl;
    //     std::cout << "motor_cmd kRightAnklePitch:" << motor_ankle_cmd_[2].position << std::endl;
    //     std::cout << "motor_cmd kRightAnkleRoll:" << motor_ankle_cmd_[3].position << std::endl;
    //     unm_cont = 0;
    // }
}

void EcatL0Backend::step(double dt)
{
    // ThreeMasterTi5EncosRuntime在后台运行
    // 这里不需要额外操作
    (void)dt;
}

void EcatL0Backend::sample(hhros2::shm::JointFeedback* feedback, int count, 
                           hhros2::shm::ImuSample& imu)
{
    if (!initialized_.load() || !runtime_) {
        return;
    }
    
    auto snapshot = runtime_->snapshot();
    int max_count = std::min(count, hhros2::shm::kJointCount);
    
    for (const auto & wiring : joint_wiring::kPhysicalMotorMap) {
        const std::size_t logical = wiring.logical_joint_index;
        if (static_cast<int>(logical) >= max_count) continue;
        const auto & motor =
            snapshot.motors[wiring.master_id][wiring.motor_index];
        const auto calibration =
            joint_wiring::calibration_for_model(wiring.motor_model);

        if (calibration.protocol == joint_wiring::MotorProtocol::Encos) {
            feedback[logical].position = encos_pulse_to_rad(motor.position);
            feedback[logical].velocity =
                encos_pulse_to_angular_velocity(motor.velocity);
            feedback[logical].effort = encos_pulse_to_effort(
                motor.current, wiring.motor_index);
        } else {
            feedback[logical].position = motor.position;
            feedback[logical].velocity = motor.velocity;
            feedback[logical].effort = ti5_current_to_effort(
                wiring.master_id, wiring.motor_index, motor.current);
        }

        int ankle_index = -1;
        if (logical == joint_wiring::kLeftAnklePitch) ankle_index = 0;
        else if (logical == joint_wiring::kLeftAnkleRoll) ankle_index = 1;
        else if (logical == joint_wiring::kRightAnklePitch) ankle_index = 2;
        else if (logical == joint_wiring::kRightAnkleRoll) ankle_index = 3;
        if (ankle_index >= 0) {
            motor_q_[ankle_index] = feedback[logical].position;
            motor_dq_[ankle_index] = feedback[logical].velocity;
            motor_tau_[ankle_index] = feedback[logical].effort;
        }
    }
    
    // ========== 执行正解（仅更新内部 joint_q_ 等） ==========
    sync_joint_feedback_from_motor_feedback();

    // 将正解后的踝关节结果覆盖回 feedback
    const int ankle_idxs[4] = {
        kLeftAnklePitch, kLeftAnkleRoll,
        kRightAnklePitch, kRightAnkleRoll};
    for (int i = 0; i < 4; ++i) {
        if (ankle_idxs[i] >= max_count) continue;
        feedback[ankle_idxs[i]].position = joint_q_[i];
        feedback[ankle_idxs[i]].velocity = joint_dq_[i];
        feedback[ankle_idxs[i]].effort = joint_tau_[i];
    }

    // IMU数据处理
    std::memset(&imu, 0, sizeof(hhros2::shm::ImuSample));
}

void EcatL0Backend::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (runtime_) {
        LOG_INFO(LogType::MOTORLOG, "[EcatL0Backend] Shutting down...");
        
        // 安全关闭流程
        runtime_->set_motor_enable(false);  // 先禁用电机
        runtime_->request_stop();            // 请求停止
        runtime_->wait();                    // 等待停止完成
        runtime_->release();                 // 释放资源
        
        runtime_.reset();
        LOG_INFO(LogType::MOTORLOG, "[EcatL0Backend] Shutdown complete");
    }
    
    initialized_.store(false);
    enabled_.store(false);
}

// ========== 逆解映射：关节 -> 电机（命令下发） ==========
void EcatL0Backend::map_parallel_ankle_command(
    int pitch_index, int roll_index, FootSide side)
{
    if (!valid_pair(pitch_index, roll_index)) return;

    // 从 left_ankle_cmd_ / right_ankle_cmd_ 读取
    const AnkleCommand* cmd = (pitch_index == kLeftAnklePitch) ? &left_ankle_cmd_ : &right_ankle_cmd_;
    Vector2d q(cmd->pitch_pos, cmd->roll_pos);
    Vector2d vel(cmd->pitch_vel, cmd->roll_vel);
    Vector2d joint_effort(cmd->pitch_eff, cmd->roll_eff);

    const Matrix2d joint_to_motor = ankle_kinematics_.JointToMotorJacobianRad(q[0], q[1], side);
    Vector2d motor_effort = joint_to_motor.transpose().fullPivLu().solve(joint_effort);
    if (!motor_effort.allFinite()) motor_effort.setZero();

    ankle_kinematics_.JointToMotor(q, vel, side);

    // 存入 motor_ankle_cmd_
    int idx = (pitch_index == kLeftAnklePitch) ? 0 :
              (pitch_index == kRightAnklePitch) ? 2 : -1;
    if (idx >= 0) {
        motor_ankle_cmd_[idx].position = q[0];
        motor_ankle_cmd_[idx].velocity = vel[0];
        motor_ankle_cmd_[idx].effort = motor_effort[0];
        motor_ankle_cmd_[idx].kp = cmd->pitch_kp;
        motor_ankle_cmd_[idx].kd = cmd->pitch_kd;
        motor_ankle_cmd_[idx+1].position = q[1];
        motor_ankle_cmd_[idx+1].velocity = vel[1];
        motor_ankle_cmd_[idx+1].effort = motor_effort[1];
        motor_ankle_cmd_[idx+1].kp = cmd->roll_kp;
        motor_ankle_cmd_[idx+1].kd = cmd->roll_kd;
    }
}

// ========== 批量命令映射 ==========
void EcatL0Backend::sync_motor_commands_from_joint_commands()
{
    // 左踝逆解
    map_parallel_ankle_command(kLeftAnklePitch, kLeftAnkleRoll, FootSide::kLeft);
    // 右踝逆解
    map_parallel_ankle_command(kRightAnklePitch, kRightAnkleRoll, FootSide::kRight);
}

// ========== 正解映射：电机 -> 关节（反馈处理） ==========
void EcatL0Backend::map_parallel_ankle_feedback(
    int pitch_index, int roll_index, FootSide side)
{
    if (!valid_pair(pitch_index, roll_index)) return;

    int idx0 = (pitch_index == kLeftAnklePitch) ? 0 : 2;
    int idx1 = idx0 + 1;
    Vector2d q(motor_q_[idx0], motor_q_[idx1]);
    Vector2d vel(motor_dq_[idx0], motor_dq_[idx1]);
    Vector2d initial_deg(joint_q_[idx0] * 180.0 / kPi, joint_q_[idx1] * 180.0 / kPi);

    // A periodic parallel mechanism has multiple mathematical roots. Do not
    // overwrite the last valid joint feedback when the solver leaves the
    // physical ankle range or fails to converge.
    if (!ankle_kinematics_.MotorToJoint(q, vel, side, initial_deg)) {
        return;
    }

    joint_q_[idx0] = q[0];
    joint_q_[idx1] = q[1];
    joint_dq_[idx0] = vel[0];
    joint_dq_[idx1] = vel[1];

    const Matrix2d joint_to_motor = ankle_kinematics_.JointToMotorJacobianRad(q[0], q[1], side);
    Vector2d motor_tau(motor_tau_[idx0], motor_tau_[idx1]);
    Vector2d joint_tau = joint_to_motor.transpose() * motor_tau;
    joint_tau_[idx0] = joint_tau[0];
    joint_tau_[idx1] = joint_tau[1];
}

// ========== 批量反馈映射（正解） ==========
void EcatL0Backend::sync_joint_feedback_from_motor_feedback()
{
    map_parallel_ankle_feedback(kLeftAnklePitch, kLeftAnkleRoll, FootSide::kLeft);
    map_parallel_ankle_feedback(kRightAnklePitch, kRightAnkleRoll, FootSide::kRight);
}

// ========== 辅助函数 ==========
bool EcatL0Backend::valid_pair(int first, int second) const
{
    return first >= 0 && second >= 0 &&
           first < hhros2::shm::kJointCount && second < hhros2::shm::kJointCount;
}

} // namespace hhros2_l0
