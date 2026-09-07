#pragma once

#include <cstdint>

#include "hhros2_motor_protocol/ipc_motor_protocol.hpp"

using ti5_encos::MotorControlMode;

struct MotorAxisData
{
    std::uint16_t axis_id = 0;
    std::uint16_t slave_pos = 0;
    std::uint16_t master_id = 0;
    std::uint16_t old_status_word = 0;

    volatile std::uint16_t *control_word = nullptr;
    volatile std::int32_t *target_position = nullptr;
    volatile std::int16_t *target_torque = nullptr;
    volatile std::int32_t *target_velocity = nullptr;
    volatile std::int8_t *mode_of_operation = nullptr;

    volatile const std::uint16_t *error_code = nullptr;
    volatile const std::uint16_t *status_word = nullptr;
    volatile const std::int32_t *position_actual_value = nullptr;
    volatile const std::int8_t *mode_of_operation_display = nullptr;
    volatile const std::int16_t *torque_actual_value = nullptr;
    volatile const std::int32_t *velocity_actual_value = nullptr;

    volatile float *kp = nullptr;
    volatile float *kd = nullptr;
    volatile float *target_pos = nullptr;
    volatile float *target_vel = nullptr;
    volatile float *target_tor = nullptr;
    volatile float *actual_pos = nullptr;
    volatile float *actual_vel = nullptr;
    volatile float *actual_cur = nullptr;
};

constexpr int kEncosMotorCount = 12;

struct EncosAxisData
{
    volatile double encos_angle_actual_value[kEncosMotorCount]{};               // ecos 角度返回值
    volatile double encos_angular_velocity_actual_value[kEncosMotorCount]{};    // encos速度返回值
    volatile const std::int16_t *encos_torque_actual_value = nullptr;           // 力矩返回值
    volatile double encos_current_actual_value[kEncosMotorCount]{};             // 电流返回值
};
