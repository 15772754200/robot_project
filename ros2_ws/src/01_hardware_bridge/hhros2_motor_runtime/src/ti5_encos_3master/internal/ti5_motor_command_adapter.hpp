#pragma once

#include "internal/motor_axis_data.hpp"

struct Ti5MotorCommandAdapter
{
    MotorAxisData *axis = nullptr;
    bool execute = false;
    bool valid = false;
    bool error = false;
    int errorid = 0;
    bool executing_in_process = false;
    uint64_t count = 0;
    uint32_t vel = 0;
    int32_t pos = 0;

    void on_cycle();
    float get_check_data(float data, float max_data, float min_data);
};
