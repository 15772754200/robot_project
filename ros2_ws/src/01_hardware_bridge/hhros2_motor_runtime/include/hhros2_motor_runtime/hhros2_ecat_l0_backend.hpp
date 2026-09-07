#pragma once

#include <memory>
#include <mutex>
#include <atomic>

#include "hhros2_motor_runtime/hhros2_l0_backend.hpp"
#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"
#include "hhros2_motor_runtime/ti5_encos_3master/three_master_ti5_encos_runtime.hpp"
#include "internal/motor_protocol_conversions.hpp"
#include "hhros2_mechanisms/parallel_ankle_kinematics.hpp"

namespace hhros2_l0 {

/**
 * @brief EtherCAT硬件后端 - 基于ThreeMasterTi5EncosRuntime
 * 
 * 参考 ti5_encos_3master_demo 的实现模式
 */
class EcatL0Backend : public L0HardwareBackend
{
public:
    EcatL0Backend();
    ~EcatL0Backend() override = default;

    bool init() override;
    void apply(const hhros2::shm::JointCommand* cmd, int count, bool enabled) override;
    void step(double dt) override;
    void sample(hhros2::shm::JointFeedback* fb, int count, hhros2::shm::ImuSample& imu) override;
    void shutdown() override;
    
private:
    // 配置
    ti5_encos::RuntimeConfig config_;
    std::unique_ptr<ti5_encos::ThreeMasterTi5EncosRuntime> runtime_;
    
    // 状态
    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{false};
    std::mutex mutex_;

    // ========== 并联踝关节映射 ==========
    static constexpr int kLeftAnklePitch =
        static_cast<int>(joint_wiring::kLeftAnklePitch);
    static constexpr int kLeftAnkleRoll =
        static_cast<int>(joint_wiring::kLeftAnkleRoll);
    static constexpr int kRightAnklePitch =
        static_cast<int>(joint_wiring::kRightAnklePitch);
    static constexpr int kRightAnkleRoll =
        static_cast<int>(joint_wiring::kRightAnkleRoll);
    static constexpr double kPi = 3.14159265358979323846;

    // 运动学求解器
    hhros2_mechanisms::ParallelAnkleKinematics ankle_kinematics_;

    // ===== 缓存：仅用于踝关节（索引 4,5,10,11） =====
    // 命令缓存：存储从 joint_cmd_ 提取的踝关节命令
    struct AnkleCommand {
        double pitch_pos, pitch_vel, pitch_eff, pitch_kp, pitch_kd;
        double roll_pos,  roll_vel,  roll_eff,  roll_kp,  roll_kd;
    };
    AnkleCommand left_ankle_cmd_;
    AnkleCommand right_ankle_cmd_;

    // 电机命令缓存（逆解结果）
    hhros2::shm::JointCommand motor_ankle_cmd_[4]; // 按索引 4,5,10,11 的顺序

    // 反馈缓存（正解结果）
    double joint_q_[4];    // pitch, roll 顺序
    double joint_dq_[4];
    double joint_tau_[4];
    double motor_q_[4];    // 原始电机角度
    double motor_dq_[4];
    double motor_tau_[4];

    // 命令映射（逆解）：将 joint 空间命令映射到 motor 空间命令（力位混控）
    void map_parallel_ankle_command(int pitch_index, int roll_index, hhros2_mechanisms::FootSide side);
    void sync_motor_commands_from_joint_commands();

    // 反馈映射（正解）：将 motor 反馈映射到 joint 反馈（仅内部缓存，不修改共享内存）
    void map_parallel_ankle_feedback(int pitch_index, int roll_index, hhros2_mechanisms::FootSide side);
    void sync_joint_feedback_from_motor_feedback();

    // 辅助：检查索引是否有效
    bool valid_pair(int first, int second) const;
};

} // namespace hhros2_l0
