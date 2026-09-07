#pragma once

#include <cstdint>

constexpr float kEncosKpMin = 0.0f;
constexpr float kEncosKpMax = 500.0f;
constexpr float kEncosKdMin = 0.0f;
constexpr float kEncosPositionMin = -12.5f;
constexpr float kEncosPositionMax = 12.5f;
constexpr float kEncosSpeedMin = -18.0f;
constexpr float kEncosSpeedMax = 18.0f;

constexpr std::uint8_t kEncosCommandAck = 0x00;
constexpr std::uint8_t kEncosCommandAuto = 0x01;

struct Motor_Msg
{
    std::uint32_t id = 0;
    std::uint8_t rtr = 0;
    std::uint8_t dlc = 0;
    std::uint8_t data[8]{};
};

struct EtherCAT_Msg
{
    std::uint8_t motor_num = 0;
    std::uint8_t can_ide = 0;
    Motor_Msg motor[6]{};
};

struct OD_Motor_Msg
{
    std::uint16_t angle_actual_int = 0;
    std::uint16_t angle_desired_int = 0;
    std::int16_t speed_actual_int = 0;
    std::int16_t speed_desired_int = 0;
    std::int16_t current_actual_int = 0;
    std::int16_t current_desired_int = 0;
    float speed_actual_rad = 0.0f;
    float speed_desired_rad = 0.0f;
    float angle_actual_rad = 0.0f;
    float angle_desired_rad = 0.0f;
    std::uint16_t motor_id = 0;
    std::uint8_t temperature = 0;
    std::uint8_t error = 0;
    float angle_actual_float = 0.0f;
    float speed_actual_float = 0.0f;
    float current_actual_float = 0.0f;
    float angle_desired_float = 0.0f;
    float speed_desired_float = 0.0f;
    float current_desired_float = 0.0f;
    float power = 0.0f;
    std::uint16_t acceleration = 0;
    std::uint16_t linkage_KP = 0;
    std::uint16_t speed_KI = 0;
    std::uint16_t feedback_KP = 0;
    std::uint16_t feedback_KD = 0;
};
