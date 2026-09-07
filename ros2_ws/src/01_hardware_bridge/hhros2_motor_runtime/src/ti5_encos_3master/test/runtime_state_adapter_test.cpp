#include "internal/runtime_state_adapter.hpp"

#include <cstdlib>
#include <iostream>

namespace
{

#define EXPECT_TRUE(condition)                                                \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            std::cerr << __FILE__ << ":" << __LINE__                         \
                      << ": expected " #condition "\n";                      \
            std::exit(1);                                                     \
        }                                                                     \
    } while (false)

void test_default_commands()
{
    Motor_master shared_state{};
    ti5_encos::initialize_default_motor_commands(&shared_state);

    EXPECT_TRUE(shared_state.master_id[0].motor_id[0].kp_cmd == 50000);
    EXPECT_TRUE(shared_state.master_id[0].motor_id[0].kd_cmd == 3000);
    EXPECT_TRUE(shared_state.master_id[1].motor_id[1].kp_cmd == 300000);
    EXPECT_TRUE(shared_state.master_id[1].motor_id[1].kd_cmd == 20000);
    EXPECT_TRUE(shared_state.master_id[2].motor_id[0].kp_cmd == 300000);
    EXPECT_TRUE(shared_state.master_id[1].motor_id[0].kp_cmd == 0);
    EXPECT_TRUE(shared_state.master_id[1].motor_id[0].kd_cmd == 0);
}

void test_command_and_state_round_trip()
{
    Motor_master shared_state{};
    const ti5_encos::MotorCommand command{
        1,
        2,
        3,
        4,
        5,
    };

    ti5_encos::set_motor_command_in_shared_state(
        &shared_state,
        2,
        3,
        command);

    auto &motor = shared_state.master_id[2].motor_id[3];
    motor.position_actual = 10;
    motor.velocity_actual = 20;
    motor.current_actual = 30;

    ti5_encos::MotorState state{};
    EXPECT_TRUE(ti5_encos::get_motor_state_from_shared_state(
        shared_state,
        2,
        3,
        &state));
    EXPECT_TRUE(state.command.kp == 1);
    EXPECT_TRUE(state.command.kd == 2);
    EXPECT_TRUE(state.command.torque == 3);
    EXPECT_TRUE(state.command.position == 4);
    EXPECT_TRUE(state.command.velocity == 5);
    EXPECT_TRUE(state.position == 10);
    EXPECT_TRUE(state.velocity == 20);
    EXPECT_TRUE(state.current == 30);

    EXPECT_TRUE(!ti5_encos::get_motor_state_from_shared_state(
        shared_state,
        -1,
        3,
        &state));
}

void test_diag_copy()
{
    Ethercat_comm_diag source{};
    source.version = 11;
    source.update_seq = 12;
    for (int master_index = 0; master_index < master_number; ++master_index)
    {
        auto &diag = source.master_diag[master_index];
        const int base = master_index * 10;

        diag.wc_state = base + 1;
        diag.lost_frame_count = base + 2;
        diag.lost_frame_delta = base + 3;
        diag.slaves_responding = base + 4;
        diag.master_al_state = base + 5;
        diag.latency_flag = base + 6;
        diag.latency_max_us = base + 7;
        diag.latency_min_us = base + 8;
        diag.latency_avg_us = base + 9;
        diag.cycle_counter = base + 10;
    }

    ti5_encos::RuntimeDiag target{};
    ti5_encos::copy_diag_to_runtime_snapshot(source, &target);

    EXPECT_TRUE(target.version == 11);
    EXPECT_TRUE(target.update_seq == 12);
    EXPECT_TRUE(target.masters[2].wc_state == 21);
    EXPECT_TRUE(target.masters[2].lost_frame_count == 22);
    EXPECT_TRUE(target.masters[2].lost_frame_delta == 23);
    EXPECT_TRUE(target.masters[2].cycle_counter == 30);
}
} // namespace

int main()
{
    test_default_commands();
    test_command_and_state_round_trip();
    test_diag_copy();

    return 0;
}
