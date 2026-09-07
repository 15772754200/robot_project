#pragma once

#include "internal/motor_shared_state.hpp"

#include "ecat/niic_api.hpp"

void ipc_motor_diag_register_tasks(
    ecat::task *master0,
    ecat::task *master1,
    ecat::task *master2);

void ipc_motor_diag_update_cycle_status(int master_index, int wc_state);

void ipc_motor_diag_update_server_sample();

void ipc_motor_diag_sample_direct(Ethercat_comm_diag *diag);

Ethercat_comm_diag ipc_motor_diag_snapshot();
