#pragma once

#include "hhros2_motor_runtime/ti5_encos_3master/three_master_ti5_encos_runtime.hpp"

#include "internal/motor_shared_state.hpp"

namespace ti5_encos
{

bool valid_motor_index(int master_index, int motor_index);

void initialize_default_motor_commands(Motor_master *shared_state);

void set_motor_command_in_shared_state(Motor_master *shared_state,
                                       int master_index,
                                       int motor_index,
                                       const MotorCommand &command);

bool get_motor_state_from_shared_state(const Motor_master &shared_state,
                                       int master_index,
                                       int motor_index,
                                       MotorState *state);

void copy_diag_to_runtime_snapshot(const Ethercat_comm_diag &source,
                                   RuntimeDiag *target);

} // namespace ti5_encos
