#pragma once

/**
 * @file hhros2_l0_backend.hpp
 * @brief Hardware backend abstraction for the L0 runtime loop (NO ROS).
 *
 * The L0 main loop is hardware-agnostic: it pulls commands from shared memory,
 * hands them to a backend, steps it, and publishes the sampled feedback. The
 * real deployment plugs in an EtherCAT backend that wraps the existing
 * ThreeMasterTi5EncosRuntime; SimL0Backend lets the same binary run on a dev
 * host to validate the shared-memory contract and the safe-damping fallback.
 */

#include <cmath>
#include <cstdint>
#include <vector>

#include <Eigen/Dense>

#include "hhros2_mechanisms/parallel_ankle_kinematics.hpp"
#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"

namespace hhros2_l0
{

class L0HardwareBackend
{
public:
    virtual ~L0HardwareBackend() = default;

    /// Bring the bus / actuators up. Return false to abort startup.
    virtual bool init() = 0;
    virtual void shutdown() = 0;

    /**
     * @brief Latch the latest command.
     * @param enabled  When false the backend must hold a safe passive/damping
     *                 state regardless of the command contents (Safe Reflex).
     */
    virtual void apply(
        const hhros2::shm::JointCommand * cmd, int count, bool enabled) = 0;

    /// Advance the hardware/physics by one cycle of duration dt seconds.
    virtual void step(double dt) = 0;

    /// Sample the measured joint state and IMU.
    virtual void sample(
        hhros2::shm::JointFeedback * fb,
        int count,
        hhros2::shm::ImuSample & imu) = 0;
};

/**
 * @brief Minimal integrator backend for bring-up without hardware.
 *
 * Closes a simple PD around the hybrid command so the shared-memory loop and
 * the controller chain can be exercised end-to-end on a laptop. This is NOT a
 * physics simulator (use hhros2_sim/MuJoCo for fidelity) - it only honors the
 * command contract and the enable/damping semantics.
 */
class SimL0Backend : public L0HardwareBackend
{
public:
    explicit SimL0Backend(int joint_count = hhros2::shm::kJointCount)
    : joint_count_(joint_count),
      joint_cmd_(joint_count),
      joint_q_(joint_count, 0.0),
      joint_dq_(joint_count, 0.0),
      joint_tau_(joint_count, 0.0),
      motor_cmd_(joint_count),
      motor_q_(joint_count, 0.0),
      motor_dq_(joint_count, 0.0),
      motor_tau_(joint_count, 0.0),
      enabled_(false)
    {
    }

    bool init() override { return true; }
    void shutdown() override {}

    void apply(
        const hhros2::shm::JointCommand * cmd, int count, bool enabled) override
    {
        enabled_ = enabled;
        const int n = std::min<int>(count, static_cast<int>(joint_cmd_.size()));
        for (int i = 0; i < n; ++i)
        {
            joint_cmd_[i] = cmd[i];
        }
        sync_motor_commands_from_joint_commands();
    }

    void step(double dt) override
    {
        constexpr double kInertia = 0.5;  // lumped joint inertia
        for (std::size_t i = 0; i < motor_q_.size(); ++i)
        {
            if (enabled_)
            {
                motor_tau_[i] = motor_cmd_[i].effort +
                                motor_cmd_[i].kp *
                                    (motor_cmd_[i].position - motor_q_[i]) +
                                motor_cmd_[i].kd *
                                    (motor_cmd_[i].velocity - motor_dq_[i]);
            }
            else
            {
                motor_tau_[i] = -1.0 * motor_dq_[i];
            }
            const double ddq = motor_tau_[i] / kInertia;
            motor_dq_[i] += ddq * dt;
            motor_q_[i] += motor_dq_[i] * dt;
        }
        sync_joint_feedback_from_motor_feedback();
    }

    void sample(
        hhros2::shm::JointFeedback * fb,
        int count,
        hhros2::shm::ImuSample & imu) override
    {
        const int n = std::min<int>(count, static_cast<int>(joint_q_.size()));
        for (int i = 0; i < n; ++i)
        {
            fb[i].position = joint_q_[i];
            fb[i].velocity = joint_dq_[i];
            fb[i].effort = joint_tau_[i];
            fb[i].temperature = 25.0;
            fb[i].error_flags = 0;
        }
        imu.orientation[0] = 0.0;
        imu.orientation[1] = 0.0;
        imu.orientation[2] = 0.0;
        imu.orientation[3] = 1.0;
        imu.linear_acceleration[2] = 9.81;
    }

private:
    static constexpr int kLeftAnklePitch = 4;
    static constexpr int kLeftAnkleRoll = 5;
    static constexpr int kRightAnklePitch = 10;
    static constexpr int kRightAnkleRoll = 11;

    void sync_motor_commands_from_joint_commands()
    {
        motor_cmd_ = joint_cmd_;
        map_parallel_ankle_command(
            kLeftAnklePitch, kLeftAnkleRoll, hhros2_mechanisms::FootSide::kLeft);
        map_parallel_ankle_command(
            kRightAnklePitch, kRightAnkleRoll, hhros2_mechanisms::FootSide::kRight);
    }

    void map_parallel_ankle_command(
        int pitch_index, int roll_index, hhros2_mechanisms::FootSide side)
    {
        if (!valid_pair(pitch_index, roll_index))
        {
            return;
        }

        Eigen::Vector2d q(
            joint_cmd_[static_cast<std::size_t>(pitch_index)].position,
            joint_cmd_[static_cast<std::size_t>(roll_index)].position);
        Eigen::Vector2d velocity(
            joint_cmd_[static_cast<std::size_t>(pitch_index)].velocity,
            joint_cmd_[static_cast<std::size_t>(roll_index)].velocity);
        const Eigen::Vector2d joint_effort(
            joint_cmd_[static_cast<std::size_t>(pitch_index)].effort,
            joint_cmd_[static_cast<std::size_t>(roll_index)].effort);
        const Eigen::Matrix2d joint_to_motor =
            ankle_kinematics_.JointToMotorJacobianRad(q[0], q[1], side);
        Eigen::Vector2d motor_effort =
            joint_to_motor.transpose().fullPivLu().solve(joint_effort);
        if (!motor_effort.allFinite())
        {
            motor_effort.setZero();
        }
        ankle_kinematics_.JointToMotor(q, velocity, side);

        const auto motor1 = static_cast<std::size_t>(pitch_index);
        const auto motor2 = static_cast<std::size_t>(roll_index);
        motor_cmd_[motor1].position = q[0];
        motor_cmd_[motor1].velocity = velocity[0];
        motor_cmd_[motor1].effort = motor_effort[0];
        motor_cmd_[motor2].position = q[1];
        motor_cmd_[motor2].velocity = velocity[1];
        motor_cmd_[motor2].effort = motor_effort[1];
    }

    void sync_joint_feedback_from_motor_feedback()
    {
        joint_q_ = motor_q_;
        joint_dq_ = motor_dq_;
        joint_tau_ = motor_tau_;
        map_parallel_ankle_feedback(
            kLeftAnklePitch, kLeftAnkleRoll, hhros2_mechanisms::FootSide::kLeft);
        map_parallel_ankle_feedback(
            kRightAnklePitch, kRightAnkleRoll, hhros2_mechanisms::FootSide::kRight);
    }

    void map_parallel_ankle_feedback(
        int pitch_index, int roll_index, hhros2_mechanisms::FootSide side)
    {
        if (!valid_pair(pitch_index, roll_index))
        {
            return;
        }

        Eigen::Vector2d q(
            motor_q_[static_cast<std::size_t>(pitch_index)],
            motor_q_[static_cast<std::size_t>(roll_index)]);
        Eigen::Vector2d velocity(
            motor_dq_[static_cast<std::size_t>(pitch_index)],
            motor_dq_[static_cast<std::size_t>(roll_index)]);
        const Eigen::Vector2d initial_deg(
            joint_q_[static_cast<std::size_t>(pitch_index)] * 180.0 / kPi,
            joint_q_[static_cast<std::size_t>(roll_index)] * 180.0 / kPi);
        ankle_kinematics_.MotorToJoint(q, velocity, side, initial_deg);

        const auto pitch = static_cast<std::size_t>(pitch_index);
        const auto roll = static_cast<std::size_t>(roll_index);
        joint_q_[pitch] = q[0];
        joint_q_[roll] = q[1];
        joint_dq_[pitch] = velocity[0];
        joint_dq_[roll] = velocity[1];

        const Eigen::Matrix2d joint_to_motor =
            ankle_kinematics_.JointToMotorJacobianRad(q[0], q[1], side);
        const Eigen::Vector2d motor_tau(motor_tau_[pitch], motor_tau_[roll]);
        const Eigen::Vector2d joint_tau = joint_to_motor.transpose() * motor_tau;
        joint_tau_[pitch] = joint_tau[0];
        joint_tau_[roll] = joint_tau[1];
    }

    bool valid_pair(int first, int second) const
    {
        return first >= 0 && second >= 0 &&
               first < joint_count_ && second < joint_count_;
    }

    static constexpr double kPi = 3.14159265358979323846;

    int joint_count_;
    hhros2_mechanisms::ParallelAnkleKinematics ankle_kinematics_;
    std::vector<hhros2::shm::JointCommand> joint_cmd_;
    std::vector<double> joint_q_;
    std::vector<double> joint_dq_;
    std::vector<double> joint_tau_;
    std::vector<hhros2::shm::JointCommand> motor_cmd_;
    std::vector<double> motor_q_;
    std::vector<double> motor_dq_;
    std::vector<double> motor_tau_;
    bool enabled_;
};

}  // namespace hhros2_l0
