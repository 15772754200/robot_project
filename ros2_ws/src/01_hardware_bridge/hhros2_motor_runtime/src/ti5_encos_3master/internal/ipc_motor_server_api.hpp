#pragma once

#include "internal/motor_shared_state.hpp"

namespace ecat
{
class task;
}

void ipc_motor_server_register_tasks(ecat::task *master0,
                                     ecat::task *master1,
                                     ecat::task *master2);
void update_master_cycle_status(int master_index, int wc_state);
void ipc_motor_server_sample_diag(Ethercat_comm_diag *diag);
void configure_ipc_motor_server(int cpu_affinity, int priority);
void start_ipc_motor_server();
void stop_ipc_motor_server();
