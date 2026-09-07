#include "internal/encos_command_builder.hpp"
#include "internal/joint_wiring_map.hpp"

#include <array>
#include <cstring>

namespace encos
{
namespace
{
constexpr int kMotorsPerSlave = 6;
constexpr std::uint8_t kMaxAckStatus = 3;
constexpr std::uint16_t kMaxPacked12Bit = 0x0FFF;

bool is_valid_passage(std::uint8_t passage)
{
    return passage >= 1 && passage <= kMotorsPerSlave;
}

int float_to_uint(float value, float min, float max, int bits)
{
    const float span = max - min;
    return static_cast<int>(
        (value - min) * static_cast<float>((1 << bits) - 1) / span);
}

std::uint16_t clamp_u16(std::uint16_t value, std::uint16_t max)
{
    if (value > max)
    {
        return max;
    }
    return value;
}

std::array<std::uint8_t, 4> float_to_bytes(float value)
{
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    return bytes;
}

Motor_Msg *select_motor(EtherCAT_Msg *message, std::uint8_t passage)
{
    if (message == nullptr || !is_valid_passage(passage))
    {
        return nullptr;
    }
    return &message->motor[passage - 1];
}

float clamp_value(float value, float min, float max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

std::int16_t clamp_value(std::int16_t value, std::int16_t min, std::int16_t max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

struct TorqueRange
{
    float min = 0.0f;
    float max = 0.0f;
};

TorqueRange torque_range(std::uint16_t motor_id)
{
    const auto calibration = joint_wiring::calibration_for_model(
        joint_wiring::encos_model_for_device_id(motor_id));
    return {
        calibration.command_effort_min,
        calibration.command_effort_max,
    };
}

int torque_to_uint(float torque, TorqueRange range)
{
    return float_to_uint(torque, range.min, range.max, 12);
}

} // namespace

/*
 * 构建力位混合指令 
 */
bool build_hybrid_force_position_command(EtherCAT_Msg *message,
                                         std::uint8_t passage,
                                         std::uint16_t motor_id,
                                         float kp,
                                         float kd,
                                         float position,
                                         float speed,
                                         float torque)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr)
    {
        return false;
    }

    message->can_ide = 0;
    motor->rtr = 0;
    motor->id = motor_id;
    motor->dlc = 8;

    kp = clamp_value(kp, kEncosKpMin, kEncosKpMax);
    const float kd_max = joint_wiring::calibration_for_model(
        joint_wiring::encos_model_for_device_id(motor_id)).kd_max;

    kd = clamp_value(kd, kEncosKdMin, kd_max);
    position = clamp_value(position, kEncosPositionMin, kEncosPositionMax);
    speed = clamp_value(speed, kEncosSpeedMin, kEncosSpeedMax);
    const TorqueRange torque_limits = torque_range(motor_id);
    torque = clamp_value(torque, torque_limits.min, torque_limits.max);

    const int kp_int = float_to_uint(kp, kEncosKpMin, kEncosKpMax, 12);
    const int kd_int = float_to_uint(kd, kEncosKdMin, kd_max, 9);
    const int position_int =
        float_to_uint(position, kEncosPositionMin, kEncosPositionMax, 16);
    const int speed_int = float_to_uint(speed, kEncosSpeedMin, kEncosSpeedMax, 12);
    const int torque_int = torque_to_uint(torque, torque_limits);

    motor->data[0] = 0x00 | (kp_int >> 7);
    motor->data[1] = ((kp_int & 0x7F) << 1) | ((kd_int & 0x100) >> 8);
    motor->data[2] = kd_int & 0xFF;
    motor->data[3] = position_int >> 8;
    motor->data[4] = position_int & 0xFF;
    motor->data[5] = speed_int >> 4;
    motor->data[6] = (speed_int & 0x0F) << 4 | (torque_int >> 8);
    motor->data[7] = torque_int & 0xFF;
    return true;
}

bool build_position_command(EtherCAT_Msg *message,
                            std::uint8_t passage,
                            std::uint16_t motor_id,
                            float position,
                            std::uint16_t speed,
                            std::uint16_t current,
                            std::uint8_t ack_status)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr || ack_status > kMaxAckStatus)
    {
        return false;
    }

    speed = clamp_u16(speed, kMaxPacked12Bit);
    current = clamp_u16(current, kMaxPacked12Bit);

    message->can_ide = 0;
    motor->rtr = 0;
    motor->id = motor_id;
    motor->dlc = 8;

    const auto position_bytes = float_to_bytes(position);
    motor->data[0] = 0x20 | (position_bytes[3] >> 3);
    motor->data[1] = (position_bytes[3] << 5) | (position_bytes[2] >> 3);
    motor->data[2] = (position_bytes[2] << 5) | (position_bytes[1] >> 3);
    motor->data[3] = (position_bytes[1] << 5) | (position_bytes[0] >> 3);
    motor->data[4] = (position_bytes[0] << 5) | (speed >> 10);
    motor->data[5] = (speed & 0x3FC) >> 2;
    motor->data[6] = (speed & 0x03) << 6 | (current >> 6);
    motor->data[7] = (current & 0x3F) << 2 | ack_status;
    return true;
}

bool build_current_torque_command(EtherCAT_Msg *message,
                                  std::uint8_t passage,
                                  std::uint16_t motor_id,
                                  std::int16_t current_or_torque,
                                  std::uint8_t control_status,
                                  std::uint8_t ack_status)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr || ack_status > kMaxAckStatus || control_status > 7)
    {
        return false;
    }

    message->can_ide = 0;
    motor->rtr = 0;
    motor->id = motor_id;
    motor->dlc = 3;

    const auto min = static_cast<std::int16_t>(control_status ? -3000 : -2000);
    const auto max = static_cast<std::int16_t>(control_status ? 3000 : 2000);
    current_or_torque = clamp_value(current_or_torque, min, max);

    motor->data[0] = 0x60 | control_status << 2 | ack_status;
    const auto encoded_value = static_cast<std::uint16_t>(current_or_torque);
    motor->data[1] = encoded_value >> 8;
    motor->data[2] = encoded_value & 0xFF;
    return true;
}

bool build_motor_speed_command(EtherCAT_Msg *message,
                               std::uint8_t passage,
                               std::uint16_t motor_id,
                               float speed,
                               std::uint16_t current,
                               std::uint8_t ack_status)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr || ack_status > kMaxAckStatus)
    {
        return false;
    }

    current = clamp_u16(current, kMaxPacked12Bit);

    message->can_ide = 0;
    motor->rtr = 0;
    motor->id = motor_id;
    motor->dlc = 7;

    const auto speed_bytes = float_to_bytes(speed);
    motor->data[0] = 0x40 | ack_status;
    motor->data[1] = speed_bytes[3];
    motor->data[2] = speed_bytes[2];
    motor->data[3] = speed_bytes[1];
    motor->data[4] = speed_bytes[0];
    motor->data[5] = current >> 8;
    motor->data[6] = current & 0xFF;
    return true;
}

bool build_motor_id_reading_command(EtherCAT_Msg *message, std::uint8_t passage)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr)
    {
        return false;
    }

    message->can_ide = 0;
    motor->rtr = 0;
    motor->id = 0x7FF;
    motor->dlc = 4;
    motor->data[0] = 0xFF;
    motor->data[1] = 0xFF;
    motor->data[2] = 0x00;
    motor->data[3] = 0x82;
    return true;
}

bool build_motor_id_setting_command(EtherCAT_Msg *message,
                                    std::uint8_t passage,
                                    std::uint16_t motor_id,
                                    std::uint16_t new_motor_id)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr)
    {
        return false;
    }

    message->can_ide = 0;
    motor->id = 0x7FF;
    motor->dlc = 6;
    motor->rtr = 0;
    motor->data[0] = motor_id >> 8;
    motor->data[1] = motor_id & 0xFF;
    motor->data[2] = 0x00;
    motor->data[3] = 0x04;
    motor->data[4] = new_motor_id >> 8;
    motor->data[5] = new_motor_id & 0xFF;
    return true;
}

bool build_motor_zero_setting_command(EtherCAT_Msg *message,
                                      std::uint8_t passage,
                                      std::uint16_t motor_id)
{
    auto *motor = select_motor(message, passage);
    if (motor == nullptr)
    {
        return false;
    }

    message->can_ide = 0;
    motor->id = 0x7FF;
    motor->dlc = 4;
    motor->rtr = 0;
    motor->data[0] = motor_id >> 8;
    motor->data[1] = motor_id & 0xFF;
    motor->data[2] = 0x00;
    motor->data[3] = 0x03;
    return true;
}

} // namespace encos
