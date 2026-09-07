#include "internal/ethercat_probe_recorder.hpp"

#include "internal/motor_shared_state.hpp"
#include "probe/single_shot_probe.h"

#include "hhros2_log/log.h"

namespace
{
bool trace_targets_master(const single_shot_probe::TraceData *trace,
                          int master_index)
{
    return trace != nullptr && single_shot_probe::ready(trace) &&
           trace->target_master_index == master_index;
}

bool valid_target_motor(int motor_index)
{
    return motor_index >= 0 && motor_index < motor_number;
}

bool feedback_changed_from_send_baseline(
    const single_shot_probe::TraceData &trace,
    const Motor_msg_single &motor)
{
    return motor.position_actual != trace.ec_send_position_actual ||
           motor.velocity_actual != trace.ec_send_velocity_actual ||
           motor.current_actual != trace.ec_send_current_actual;
}
} // namespace

void record_ethercat_send_probe(int master_index)
{
    auto *trace = single_shot_probe::map_trace();
    if (!trace_targets_master(trace, master_index) || trace->ec_send_ns != 0U)
    {
        return;
    }

    const int target_motor = trace->target_motor_index;
    if (!valid_target_motor(target_motor))
    {
        return;
    }

    Motor_msg_single motor{};
    {
        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        motor = motor_msg.master_id[master_index].motor_id[target_motor];
    }
    if (motor.position_cmd == 0 && motor.velocity_cmd == 0 && motor.Tor_cmd == 0)
    {
        return;
    }

    trace->ec_send_ns = single_shot_probe::now_ns();
    trace->ec_send_position_actual = motor.position_actual;
    trace->ec_send_velocity_actual = motor.velocity_actual;
    trace->ec_send_current_actual = motor.current_actual;
    LOG_INFO(LogType::MOTORLOG,
        "[Probe][EtherCAT] send_callback master=%d motor=%d ec_send_ns=%llu "
        "position_cmd=%f velocity_cmd=%f torque_cmd=%f "
        "position_actual=%f velocity_actual=%f current_actual=%f",
        master_index,
        target_motor,
        (unsigned long long)trace->ec_send_ns,
        motor.position_cmd,
        motor.velocity_cmd,
        motor.Tor_cmd,
        motor.position_actual,
        motor.velocity_actual,
        motor.current_actual);
}

void record_ethercat_receive_probe(int master_index)
{
    auto *trace = single_shot_probe::map_trace();
    if (!trace_targets_master(trace, master_index) || trace->ec_send_ns == 0U)
    {
        return;
    }

    const int target_motor = trace->target_motor_index;
    if (!valid_target_motor(target_motor))
    {
        return;
    }

    Motor_msg_single motor{};
    {
        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        motor = motor_msg.master_id[master_index].motor_id[target_motor];
    }
    if (trace->ec_receive_ns == 0U)
    {
        trace->ec_receive_ns = single_shot_probe::now_ns();
        LOG_INFO(LogType::MOTORLOG,
            "[Probe][EtherCAT] first receive_callback master=%d motor=%d "
            "ec_receive_ns=%llu position_actual=%f velocity_actual=%f "
            "current_actual=%f",
            master_index,
            target_motor,
            (unsigned long long)trace->ec_receive_ns,
            motor.position_actual,
            motor.velocity_actual,
            motor.current_actual);
    }

    if (trace->ec_feedback_change_ns != 0U ||
        !feedback_changed_from_send_baseline(*trace, motor))
    {
        return;
    }

    trace->ec_feedback_change_ns = single_shot_probe::now_ns();
    trace->ec_feedback_position_actual = motor.position_actual;
    trace->ec_feedback_velocity_actual = motor.velocity_actual;
    trace->ec_feedback_current_actual = motor.current_actual;
    LOG_INFO(LogType::MOTORLOG,
        "[Probe][EtherCAT] feedback changed master=%d motor=%d "
        "ec_feedback_change_ns=%llu position_actual=%f velocity_actual=%f "
        "current_actual=%f",
        master_index,
        target_motor,
        (unsigned long long)trace->ec_feedback_change_ns,
        motor.position_actual,
        motor.velocity_actual,
        motor.current_actual);
}
