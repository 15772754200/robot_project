#include "internal/ipc_motor_message_codec.hpp"

#include <arpa/inet.h>
#include <cstring>

void decode_motor_command_message(
    const Motor_master &network_message,
    Motor_master *host_command)
{
    if (host_command == nullptr)
    {
        return;
    }

    std::memset(host_command, 0, sizeof(Motor_master));
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const auto &network_motor =
                network_message.master_id[master_idx].motor_id[motor_idx];
            auto &host_motor =
                host_command->master_id[master_idx].motor_id[motor_idx];

            host_motor.kp_cmd = ntohl(network_motor.kp_cmd);
            host_motor.kd_cmd = ntohl(network_motor.kd_cmd);
            host_motor.Tor_cmd = ntohl(network_motor.Tor_cmd);
            host_motor.position_cmd = ntohl(network_motor.position_cmd);
            host_motor.velocity_cmd = ntohl(network_motor.velocity_cmd);
        }
    }
}

void encode_motor_feedback_message(
    const Motor_master &host_feedback,
    Motor_master *network_message)
{
    if (network_message == nullptr)
    {
        return;
    }

    std::memset(network_message, 0, sizeof(Motor_master));
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const auto &host_motor =
                host_feedback.master_id[master_idx].motor_id[motor_idx];
            auto &network_motor =
                network_message->master_id[master_idx].motor_id[motor_idx];

            network_motor.position_actual = htonl(host_motor.position_actual);
            network_motor.velocity_actual = htonl(host_motor.velocity_actual);
            network_motor.current_actual = htonl(host_motor.current_actual);
        }
    }

    network_message->ec_diag.version = htonl(host_feedback.ec_diag.version);
    network_message->ec_diag.update_seq =
        htonl(host_feedback.ec_diag.update_seq);
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        const auto &host_diag = host_feedback.ec_diag.master_diag[master_idx];
        auto &network_diag =
            network_message->ec_diag.master_diag[master_idx];

        network_diag.wc_state = htonl(host_diag.wc_state);
        network_diag.lost_frame_count = htonl(host_diag.lost_frame_count);
        network_diag.lost_frame_delta = htonl(host_diag.lost_frame_delta);
        network_diag.slaves_responding = htonl(host_diag.slaves_responding);
        network_diag.master_al_state = htonl(host_diag.master_al_state);
        network_diag.latency_flag = htonl(host_diag.latency_flag);
        network_diag.latency_max_us = htonl(host_diag.latency_max_us);
        network_diag.latency_min_us = htonl(host_diag.latency_min_us);
        network_diag.latency_avg_us = htonl(host_diag.latency_avg_us);
        network_diag.cycle_counter = htonl(host_diag.cycle_counter);
    }
}

void apply_motor_command_to_shared_state(
    const Motor_master &command,
    Motor_master *shared_state)
{
    if (shared_state == nullptr)
    {
        return;
    }

    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const auto &command_motor =
                command.master_id[master_idx].motor_id[motor_idx];
            auto &shared_motor =
                shared_state->master_id[master_idx].motor_id[motor_idx];

            shared_motor.kp_cmd = command_motor.kp_cmd;
            shared_motor.kd_cmd = command_motor.kd_cmd;
            shared_motor.Tor_cmd = command_motor.Tor_cmd;
            shared_motor.position_cmd = command_motor.position_cmd;
            shared_motor.velocity_cmd = command_motor.velocity_cmd;
        }
    }
}

void copy_motor_feedback_snapshot(
    const Motor_master &shared_state,
    const Ethercat_comm_diag &diag,
    Motor_master *feedback)
{
    if (feedback == nullptr)
    {
        return;
    }

    std::memset(feedback, 0, sizeof(Motor_master));
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const auto &shared_motor =
                shared_state.master_id[master_idx].motor_id[motor_idx];
            auto &feedback_motor =
                feedback->master_id[master_idx].motor_id[motor_idx];

            feedback_motor.position_actual = shared_motor.position_actual;
            feedback_motor.velocity_actual = shared_motor.velocity_actual;
            feedback_motor.current_actual = shared_motor.current_actual;
        }
    }
    feedback->ec_diag = diag;
}
