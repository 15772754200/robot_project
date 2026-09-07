#include "internal/ipc_motor_message_codec.hpp"

#include <arpa/inet.h>
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

void fill_network_command(Motor_master *message)
{
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const int base = master_idx * 100 + motor_idx;
            auto &motor = message->master_id[master_idx].motor_id[motor_idx];

            motor.kp_cmd = htonl(base + 1);
            motor.kd_cmd = htonl(base + 2);
            motor.Tor_cmd = htonl(base + 3);
            motor.position_cmd = htonl(base + 4);
            motor.velocity_cmd = htonl(base + 5);
        }
    }
}

void expect_decoded_command(const Motor_master &message)
{
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const int base = master_idx * 100 + motor_idx;
            const auto &motor =
                message.master_id[master_idx].motor_id[motor_idx];

            EXPECT_TRUE(motor.kp_cmd == base + 1);
            EXPECT_TRUE(motor.kd_cmd == base + 2);
            EXPECT_TRUE(motor.Tor_cmd == base + 3);
            EXPECT_TRUE(motor.position_cmd == base + 4);
            EXPECT_TRUE(motor.velocity_cmd == base + 5);
            EXPECT_TRUE(motor.position_actual == 0);
            EXPECT_TRUE(motor.velocity_actual == 0);
            EXPECT_TRUE(motor.current_actual == 0);
        }
    }
}

void fill_shared_feedback(Motor_master *message)
{
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const int base = master_idx * 1000 + motor_idx;
            auto &motor = message->master_id[master_idx].motor_id[motor_idx];

            motor.position_actual = base + 10;
            motor.velocity_actual = base + 20;
            motor.current_actual = base + 30;
        }
    }
}

Ethercat_comm_diag make_diag()
{
    Ethercat_comm_diag diag{};
    diag.version = 7;
    diag.update_seq = 8;
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        auto &master_diag = diag.master_diag[master_idx];
        const int base = master_idx * 10;

        master_diag.wc_state = base + 1;
        master_diag.lost_frame_count = base + 2;
        master_diag.lost_frame_delta = base + 3;
        master_diag.slaves_responding = base + 4;
        master_diag.master_al_state = base + 5;
        master_diag.latency_flag = base + 6;
        master_diag.latency_max_us = base + 7;
        master_diag.latency_min_us = base + 8;
        master_diag.latency_avg_us = base + 9;
        master_diag.cycle_counter = static_cast<uint32_t>(base + 10);
    }
    return diag;
}

void expect_network_feedback(const Motor_master &message)
{
    EXPECT_TRUE(ntohl(message.ec_diag.version) == 7);
    EXPECT_TRUE(ntohl(message.ec_diag.update_seq) == 8);

    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        const auto &network_diag = message.ec_diag.master_diag[master_idx];
        const int base = master_idx * 10;

        EXPECT_TRUE(ntohl(network_diag.wc_state) == static_cast<uint32_t>(base + 1));
        EXPECT_TRUE(ntohl(network_diag.lost_frame_count) ==
               static_cast<uint32_t>(base + 2));
        EXPECT_TRUE(ntohl(network_diag.lost_frame_delta) ==
               static_cast<uint32_t>(base + 3));
        EXPECT_TRUE(ntohl(network_diag.slaves_responding) ==
               static_cast<uint32_t>(base + 4));
        EXPECT_TRUE(ntohl(network_diag.master_al_state) ==
               static_cast<uint32_t>(base + 5));
        EXPECT_TRUE(ntohl(network_diag.latency_flag) ==
               static_cast<uint32_t>(base + 6));
        EXPECT_TRUE(ntohl(network_diag.latency_max_us) ==
               static_cast<uint32_t>(base + 7));
        EXPECT_TRUE(ntohl(network_diag.latency_min_us) ==
               static_cast<uint32_t>(base + 8));
        EXPECT_TRUE(ntohl(network_diag.latency_avg_us) ==
               static_cast<uint32_t>(base + 9));
        EXPECT_TRUE(ntohl(network_diag.cycle_counter) ==
               static_cast<uint32_t>(base + 10));

        for (int motor_idx = 0; motor_idx < motor_number; ++motor_idx)
        {
            const int motor_base = master_idx * 1000 + motor_idx;
            const auto &motor =
                message.master_id[master_idx].motor_id[motor_idx];

            EXPECT_TRUE(ntohl(motor.position_actual) ==
                   static_cast<uint32_t>(motor_base + 10));
            EXPECT_TRUE(ntohl(motor.velocity_actual) ==
                   static_cast<uint32_t>(motor_base + 20));
            EXPECT_TRUE(ntohl(motor.current_actual) ==
                   static_cast<uint32_t>(motor_base + 30));
            EXPECT_TRUE(motor.position_cmd == 0);
            EXPECT_TRUE(motor.velocity_cmd == 0);
        }
    }
}
} // namespace

int main()
{
    Motor_master network_command{};
    Motor_master host_command{};
    Motor_master shared_state{};
    Motor_master feedback{};
    Motor_master network_feedback{};

    fill_network_command(&network_command);
    decode_motor_command_message(network_command, &host_command);
    expect_decoded_command(host_command);

    apply_motor_command_to_shared_state(host_command, &shared_state);
    expect_decoded_command(shared_state);

    fill_shared_feedback(&shared_state);
    copy_motor_feedback_snapshot(shared_state, make_diag(), &feedback);
    encode_motor_feedback_message(feedback, &network_feedback);
    expect_network_feedback(network_feedback);

    return 0;
}
