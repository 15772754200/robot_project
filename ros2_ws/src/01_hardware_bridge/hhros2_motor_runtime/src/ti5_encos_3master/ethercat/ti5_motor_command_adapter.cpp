#include "internal/ti5_motor_command_adapter.hpp"

#include "internal/cia402_power_controller.hpp"
#include "internal/joint_wiring_map.hpp"
#include "internal/motor_shared_state.hpp"

namespace
{
constexpr float kKpMax = 500.0f;
constexpr float kKpMin = 0.0f;
constexpr float kKdMax = 5.0f;
constexpr float kKdMin = 0.0f;
constexpr float kMaxVelocity = 18.0f;
constexpr float kMinVelocity = -18.0f;
constexpr float kMaxPosition = 12.5f;
constexpr float kMinPosition = -12.5f;
constexpr float kCommandScale = 10000.0f;

bool is_ti5_master(std::uint16_t master_id)
{
    return master_id == 1 || master_id == 2;
}

float torque_limit_max(std::uint16_t master_id, std::uint16_t axis_id)
{
    const auto * calibration = joint_wiring::calibration_for_physical_motor(
        static_cast<std::uint8_t>(master_id),
        static_cast<std::uint8_t>(axis_id));
    return calibration != nullptr &&
        calibration->protocol == joint_wiring::MotorProtocol::Ti5 ?
        calibration->command_effort_max : 0.0f;
}

float torque_limit_min(std::uint16_t master_id, std::uint16_t axis_id)
{
    const auto * calibration = joint_wiring::calibration_for_physical_motor(
        static_cast<std::uint8_t>(master_id),
        static_cast<std::uint8_t>(axis_id));
    return calibration != nullptr &&
        calibration->protocol == joint_wiring::MotorProtocol::Ti5 ?
        calibration->command_effort_min : 0.0f;
}

void update_actual_feedback(MotorAxisData *axis)
{
    if (axis == nullptr || !is_ti5_master(axis->master_id))
    {
        return;
    }

    if (ti5_motor_control_mode == MotorControlMode::PositionMode)
    {
        if (axis->position_actual_value == nullptr &&
            axis->velocity_actual_value == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        auto &motor =
            motor_msg.master_id[axis->master_id].motor_id[axis->axis_id];
        if (axis->position_actual_value != nullptr)
        {
            motor.position_actual = *axis->position_actual_value;
        }
        if (axis->velocity_actual_value != nullptr)
        {
            motor.velocity_actual = *axis->velocity_actual_value;
        }
        return;
    }

    if (ti5_motor_control_mode == MotorControlMode::HybridForcePositionMode)
    {
        if (axis->actual_pos == nullptr &&
            axis->actual_vel == nullptr &&
            axis->actual_cur == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        auto &motor =
            motor_msg.master_id[axis->master_id].motor_id[axis->axis_id];
        if (axis->actual_pos != nullptr)
        {
            motor.position_actual = *axis->actual_pos;
        }
        if (axis->actual_vel != nullptr)
        {
            motor.velocity_actual = *axis->actual_vel;
        }
        if (axis->actual_cur != nullptr)
        {
            motor.current_actual = *axis->actual_cur;
        }
    }
}

} // namespace

void Ti5MotorCommandAdapter::on_cycle()
{
    if (axis == nullptr)
    {
        error = true;
        errorid = 1;
        return;
    }

    if (!is_ti5_master(axis->master_id))
    {
        error = false;
        errorid = 0;
        return;
    }

    update_actual_feedback(axis);

    if (!execute)
    {
        valid = false;
        return;
    }

    if (!executing_in_process)
    {
        if (ti5_motor_control_mode == MotorControlMode::PositionMode &&
            axis->position_actual_value != nullptr)
        {
            // pos = *axis->position_actual_value;
        }
        else if (ti5_motor_control_mode == MotorControlMode::HybridForcePositionMode &&
                 axis->actual_pos != nullptr)
        {
            pos = static_cast<int32_t>(*axis->actual_pos);
        }
    }
    executing_in_process = execute;

    Motor_msg_single command_snapshot{};
    {
        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        command_snapshot =
            motor_msg.master_id[axis->master_id].motor_id[axis->axis_id];
    }

    if (ti5_motor_control_mode == MotorControlMode::PositionMode)
    {
        // if (axis->target_position != nullptr)
        // {
        //     *axis->target_position = command_snapshot.position_cmd;
        // }
        // if (axis->position_actual_value != nullptr ||
        //     axis->velocity_actual_value != nullptr)
        // {
        //     std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        //     auto &motor =
        //         motor_msg.master_id[axis->master_id].motor_id[axis->axis_id];
        //     if (axis->position_actual_value != nullptr)
        //     {
        //         motor.position_actual = *axis->position_actual_value;
        //     }
        //     if (axis->velocity_actual_value != nullptr)
        //     {
        //         motor.velocity_actual = *axis->velocity_actual_value;
        //     }
        // }
    }
    else if (ti5_motor_control_mode == MotorControlMode::HybridForcePositionMode)
    {
        float target_kp = command_snapshot.kp_cmd;
        float target_kd = command_snapshot.kd_cmd;
        float target_position = command_snapshot.position_cmd;
        float target_velocity = command_snapshot.velocity_cmd;
        float target_torque = command_snapshot.Tor_cmd;

        target_kp = get_check_data(target_kp, kKpMax, kKpMin);
        target_kd = get_check_data(target_kd, kKdMax, kKdMin);
        target_position = get_check_data(target_position, kMaxPosition, kMinPosition);
        target_velocity = get_check_data(target_velocity, kMaxVelocity, kMinVelocity);
        target_torque = get_check_data(
            target_torque,
            torque_limit_max(axis->master_id, axis->axis_id),
            torque_limit_min(axis->master_id, axis->axis_id));

        if (axis->kp != nullptr)
        {
            *axis->kp = target_kp;
        }
        if (axis->kd != nullptr)
        {
            *axis->kd = target_kd;
        }
        if (axis->target_pos != nullptr)
        {
            *axis->target_pos = target_position;
        }
        if (axis->target_vel != nullptr)
        {
            *axis->target_vel = target_velocity;
        }
        if (axis->target_tor != nullptr)
        {
            *axis->target_tor = target_torque;
        }

        update_actual_feedback(axis);

    }

    error = false;
    errorid = 0;
}

float Ti5MotorCommandAdapter::get_check_data(float data,
                                             float max_data,
                                             float min_data)
{
    if (data > max_data)
    {
        return max_data;
    }
    if (data < min_data)
    {
        return min_data;
    }
    return data;
}
