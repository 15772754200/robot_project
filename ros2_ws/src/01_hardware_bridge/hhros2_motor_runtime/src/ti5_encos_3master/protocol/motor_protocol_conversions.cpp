// 本文件存放电机协议与下发指令转换涉及到的函数和电机映射表，统一放到该文件下实现

#include "internal/motor_protocol_conversions.hpp"

#include <cmath>
#include <cstring>


namespace
{

struct FloatBytes
{
    float as_float = 0.0f;
    std::uint32_t as_uint = 0;
    std::uint8_t bytes[4]{};
};


FloatBytes split_float(float value)
{
    FloatBytes result;
    result.as_float = value;
    std::memcpy(&result.as_uint, &value, sizeof(value));
    std::memcpy(result.bytes, &value, sizeof(value));
    return result;
}

float join_float(std::uint32_t value)
{
    float result = 0.0f;
    std::memcpy(&result, &value, sizeof(value));
    return result;
}

} // namespace

float my_fmaxf(float x, float y)
{
    return (((x) > (y)) ? (x) : (y));
}

float my_fminf(float x, float y)
{
    return (((x) < (y)) ? (x) : (y));
}

float fmaxf3(float x, float y, float z)
{
    return (x > y ? (x > z ? x : z) : (y > z ? y : z));
}

float fminf3(float x, float y, float z)
{
    return (x < y ? (x < z ? x : z) : (y < z ? y : z));
}

void limit_norm(float *x, float *y, float limit)
{
    const float norm = std::sqrt(*x * *x + *y * *y);
    if (norm > limit) {
        *x = *x * limit / norm;
        *y = *y * limit / norm;
    }
}

int float_to_uint(float x, float x_min, float x_max, int bits)
{
    const float span = x_max - x_min;
    const float offset = x_min;
    return static_cast<int>((x - offset) * static_cast<float>((1 << bits) - 1) / span);
}

float uint_to_float(int x_int, float x_min, float x_max, int bits)
{
    const float span = x_max - x_min;
    const float offset = x_min;
    return static_cast<float>(x_int) * span /
           static_cast<float>((1 << bits) - 1) + offset;
}

void float32_to_float16(float *float32, std::uint16_t *float16)
{
    const auto f32 = split_float(*float32);
    //	*float16 = ((f32.v_int & 0x7fffffff) >> 13) - (0x38000000 >> 13);
    //  *float16 |= ((f32.v_int & 0x80000000) >> 16);
    std::uint16_t temp =
        (f32.bytes[3] & 0x7F) << 1 | ((f32.bytes[2] & 0x80) >> 7);
    temp -= 112;
    *float16 = temp << 10 | (f32.bytes[2] & 0x7F) << 3 | f32.bytes[1] >> 5;
    *float16 |= ((f32.as_uint & 0x80000000) >> 16);
}

void float16_to_float32(std::uint16_t *float16, float *float32)
{
    //	f32.v_int=*float16;
    //	f32.v_int = ((f32.v_int & 0x7fff) << 13) + 0x7f000000;
    //  f32.v_int |= ((*float16 & 0x8000) << 16);
    //	*float32=f32.v_float;
    const std::uint16_t temp2 = (((*float16 & 0x7C00) >> 10) + 112);
    std::uint8_t bytes[4]{};
    bytes[3] = temp2 >> 1;
    bytes[2] = ((temp2 & 0x01) << 7) | (*float16 & 0x03FC) >> 3;
    bytes[1] = (*float16 & 0x03) << 6;

    std::uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(bytes));
    value |= ((*float16 & 0x8000) << 16);
    *float32 = join_float(value);
}



float ti5_current_to_effort(uint8_t master_id,uint8_t motor_idx,double current_actual)
{
    const auto * calibration =
        joint_wiring::calibration_for_physical_motor(master_id, motor_idx);
    if (calibration == nullptr ||
        calibration->protocol != joint_wiring::MotorProtocol::Ti5)
    {
        return 0.0f;
    }
    return static_cast<float>(
        current_actual * calibration->torque_constant *
        calibration->gear_ratio);
}

double encos_pulse_to_rad(double position_actual)
{
    return (position_actual / 65536.0) * 25.0 - 12.5;
}
    
double encos_pulse_to_angular_velocity(double velocity_actual)
{
    return -18.0 + velocity_actual * 36.0 / 4095.0;
}

double encos_pulse_to_effort(double current_actual,int32_t encos_motor_idx)
{
    if (encos_motor_idx < 0)
    {
        return 0.0;
    }
    const auto * calibration = joint_wiring::calibration_for_physical_motor(
        0, static_cast<std::uint8_t>(encos_motor_idx));
    if (calibration == nullptr ||
        calibration->protocol != joint_wiring::MotorProtocol::Encos)
    {
        return 0.0;
    }
    const double range = calibration->feedback_current_range;
    return (current_actual * range * 2.0 / 4095.0 - range) *
        calibration->torque_constant;
}
