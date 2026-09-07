#include "internal/runtime_state_adapter.hpp"

namespace ti5_encos
{

bool valid_motor_index(int master_index, int motor_index)
{
    return master_index >= 0 && master_index < kMasterNumber &&
           motor_index >= 0 && motor_index < kMotorNumber;
}

void initialize_default_motor_commands(Motor_master *shared_state)
{
    if (shared_state == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
    for (int motor_index = 0; motor_index < kMotorNumber; ++motor_index)
    {
        shared_state->master_id[0].motor_id[motor_index].kp_cmd = 0;
        shared_state->master_id[0].motor_id[motor_index].kd_cmd = 0;
        shared_state->master_id[0].motor_id[motor_index].Tor_cmd = 0;
        shared_state->master_id[0].motor_id[motor_index].position_cmd = 0;
        shared_state->master_id[0].motor_id[motor_index].velocity_cmd = 0;

        shared_state->master_id[1].motor_id[motor_index].kp_cmd = 0;
        shared_state->master_id[1].motor_id[motor_index].kd_cmd = 0;
        shared_state->master_id[1].motor_id[motor_index].Tor_cmd = 0;
        shared_state->master_id[1].motor_id[motor_index].position_cmd = 0;
        shared_state->master_id[1].motor_id[motor_index].velocity_cmd = 0;

        shared_state->master_id[2].motor_id[motor_index].kp_cmd = 0;
        shared_state->master_id[2].motor_id[motor_index].kd_cmd = 0;
        shared_state->master_id[2].motor_id[motor_index].Tor_cmd = 0;
        shared_state->master_id[2].motor_id[motor_index].position_cmd = 0;
        shared_state->master_id[2].motor_id[motor_index].velocity_cmd = 0;
    }

}

void set_motor_command_in_shared_state(Motor_master *shared_state,
                                       int master_index,
                                       int motor_index,
                                       const MotorCommand &command)
{
    if (shared_state == nullptr ||
        !valid_motor_index(master_index, motor_index))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
    auto &motor = shared_state->master_id[master_index].motor_id[motor_index];
    motor.kp_cmd = command.kp;
    motor.kd_cmd = command.kd;
    motor.Tor_cmd = command.torque;
    motor.position_cmd = command.position;
    motor.velocity_cmd = command.velocity;
}

bool get_motor_state_from_shared_state(const Motor_master &shared_state,
                                       int master_index,
                                       int motor_index,
                                       MotorState *state)
{
    if (state == nullptr || !valid_motor_index(master_index, motor_index))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
    const auto &motor = shared_state.master_id[master_index].motor_id[motor_index];
    state->command.kp = motor.kp_cmd;
    state->command.kd = motor.kd_cmd;
    state->command.torque = motor.Tor_cmd;
    state->command.position = motor.position_cmd;
    state->command.velocity = motor.velocity_cmd;
    state->position = motor.position_actual;
    state->velocity = motor.velocity_actual;
    state->current = motor.current_actual;
    return true;
}

void copy_diag_to_runtime_snapshot(const Ethercat_comm_diag &source,
                                   RuntimeDiag *target)
{
    if (target == nullptr)
    {
        return;
    }

    target->version = source.version;
    target->update_seq = source.update_seq;
    for (int master_index = 0; master_index < kMasterNumber; ++master_index)
    {
        const auto &source_master = source.master_diag[master_index];
        auto &target_master = target->masters[master_index];

        target_master.wc_state = source_master.wc_state;
        target_master.lost_frame_count = source_master.lost_frame_count;
        target_master.lost_frame_delta = source_master.lost_frame_delta;
        target_master.slaves_responding = source_master.slaves_responding;
        target_master.master_al_state = source_master.master_al_state;
        target_master.latency_flag = source_master.latency_flag;
        target_master.latency_max_us = source_master.latency_max_us;
        target_master.latency_min_us = source_master.latency_min_us;
        target_master.latency_avg_us = source_master.latency_avg_us;
        target_master.cycle_counter = source_master.cycle_counter;
    }
}

} // namespace ti5_encos
