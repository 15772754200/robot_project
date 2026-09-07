#pragma once

#include "internal/motor_shared_state.hpp"

void decode_motor_command_message(
    const Motor_master &network_message,
    Motor_master *host_command);

void encode_motor_feedback_message(
    const Motor_master &host_feedback,
    Motor_master *network_message);

void apply_motor_command_to_shared_state(
    const Motor_master &command,
    Motor_master *shared_state);

void copy_motor_feedback_snapshot(
    const Motor_master &shared_state,
    const Ethercat_comm_diag &diag,
    Motor_master *feedback);
