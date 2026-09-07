#pragma once

#include <cstdint>
#include "internal/joint_wiring_map.hpp"

float ti5_current_to_effort(uint8_t master_id,uint8_t motor_idx,double current_actual);
double encos_pulse_to_rad(double position_actual);
double encos_pulse_to_angular_velocity(double velocity_actual);
double encos_pulse_to_effort(double current_actual,int32_t encos_motor_idx);


float my_fmaxf(float x, float y);
float my_fminf(float x, float y);
float fmaxf3(float x, float y, float z);
float fminf3(float x, float y, float z);

void limit_norm(float *x, float *y, float limit);

int float_to_uint(float x, float x_min, float x_max, int bits);
float uint_to_float(int x_int, float x_min, float x_max, int bits);

void float32_to_float16(float *float32, std::uint16_t *float16);
void float16_to_float32(std::uint16_t *float16, float *float32);
