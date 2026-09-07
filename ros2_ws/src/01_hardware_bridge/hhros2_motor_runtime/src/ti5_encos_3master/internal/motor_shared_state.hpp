#pragma once

#include "hhros2_motor_protocol/ipc_motor_protocol.hpp"

#include <mutex>

extern Motor_master motor_msg;

inline std::mutex &motor_shared_state_mutex()
{
    static std::mutex mutex;
    return mutex;
}
