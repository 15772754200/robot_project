#include "internal/encos_command_builder.hpp"
#include "internal/encos_motor_route.hpp"

#include <cstdlib>
#include <iostream>
#include <cstdint>

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

void test_rejects_invalid_passage()
{
    EtherCAT_Msg message{};

    EXPECT_TRUE(!encos::build_motor_id_reading_command(&message, 0));
    EXPECT_TRUE(!encos::build_motor_id_reading_command(&message, 7));
    EXPECT_TRUE(!encos::build_motor_id_reading_command(nullptr, 1));
}

void test_default_motor_routes_are_explicit()
{
    EXPECT_TRUE(encos::kDefaultMotorRoutes.size() ==
                encos::kMotorsPerEncosSlave);

    for (int motor_offset = 0;
         motor_offset < static_cast<int>(encos::kMotorsPerEncosSlave);
         ++motor_offset)
    {
        const auto *route = encos::motor_route_for_offset(motor_offset);
        EXPECT_TRUE(route != nullptr);
        EXPECT_TRUE(route->passage == motor_offset + 1);
        EXPECT_TRUE(route->motor_id == motor_offset + 1);
    }

    EXPECT_TRUE(encos::motor_route_for_offset(-1) == nullptr);
    EXPECT_TRUE(encos::motor_route_for_offset(
                    static_cast<int>(encos::kMotorsPerEncosSlave)) ==
                nullptr);
}

void test_rejects_invalid_ack_status()
{
    EtherCAT_Msg message{};

    EXPECT_TRUE(!encos::build_position_command(&message, 1, 1, 0.0F, 0, 0, 4));
    EXPECT_TRUE(!encos::build_motor_speed_command(&message, 1, 1, 0.0F, 0, 4));
    EXPECT_TRUE(!encos::build_current_torque_command(&message, 1, 1, 0, 0, 4));
}

void test_position_command_clamps_12_bit_fields()
{
    EtherCAT_Msg message{};

    EXPECT_TRUE(encos::build_position_command(
        &message,
        1,
        2,
        1.0F,
        0xFFFF,
        0xFFFF,
        3));

    const auto &motor = message.motor[0];
    EXPECT_TRUE(message.can_ide == 0);
    EXPECT_TRUE(motor.id == 2);
    EXPECT_TRUE(motor.dlc == 8);

    const std::uint16_t encoded_speed =
        static_cast<std::uint16_t>(((motor.data[4] & 0x1F) << 10) |
                                   (motor.data[5] << 2) |
                                   (motor.data[6] >> 6));
    const std::uint16_t encoded_current =
        static_cast<std::uint16_t>(((motor.data[6] & 0x3F) << 6) |
                                   (motor.data[7] >> 2));

    EXPECT_TRUE(encoded_speed == 0x0FFF);
    EXPECT_TRUE(encoded_current == 0x0FFF);
    EXPECT_TRUE((motor.data[7] & 0x03) == 3);
}

void test_current_torque_command_encodes_negative_value()
{
    EtherCAT_Msg message{};

    EXPECT_TRUE(encos::build_current_torque_command(
        &message,
        1,
        3,
        -2000,
        0,
        2));

    const auto &motor = message.motor[0];
    EXPECT_TRUE(motor.dlc == 3);
    EXPECT_TRUE(motor.data[0] == 0x62);
    EXPECT_TRUE(motor.data[1] == 0xF8);
    EXPECT_TRUE(motor.data[2] == 0x30);
}

int encoded_hybrid_kd(const EtherCAT_Msg &message)
{
    const auto &motor = message.motor[0];
    return ((motor.data[1] & 0x01) << 8) | motor.data[2];
}

int encoded_hybrid_torque(const EtherCAT_Msg &message)
{
    const auto &motor = message.motor[0];
    return ((motor.data[6] & 0x0F) << 8) | motor.data[7];
}

void test_hybrid_command_uses_forward_10020_kd_encoding()
{
    EtherCAT_Msg message{};

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        1,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F));

    EXPECT_TRUE(encoded_hybrid_kd(message) == 0);
    EXPECT_TRUE(message.can_ide == 0);

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        1,
        0.0F,
        50.0F,
        0.0F,
        0.0F,
        0.0F));

    EXPECT_TRUE(encoded_hybrid_kd(message) == 511);
}

void test_hybrid_command_uses_motor_specific_torque_limits()
{
    EtherCAT_Msg message{};

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        1,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        150.0F));

    EXPECT_TRUE(encoded_hybrid_torque(message) == 4095);

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        2,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        150.0F));

    EXPECT_TRUE(encoded_hybrid_torque(message) == 4095);

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        3,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        60.0F));

    EXPECT_TRUE(encoded_hybrid_torque(message) == 4095);

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        4,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        150.0F));

    EXPECT_TRUE(encoded_hybrid_torque(message) == 4095);

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        5,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        70.0F));

    EXPECT_TRUE(encoded_hybrid_torque(message) == 4095);

    EXPECT_TRUE(encos::build_hybrid_force_position_command(
        &message,
        1,
        6,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        70.0F));

    EXPECT_TRUE(encoded_hybrid_torque(message) == 4095);
}
} // namespace

int main()
{
    test_rejects_invalid_passage();
    test_default_motor_routes_are_explicit();
    test_rejects_invalid_ack_status();
    test_position_command_clamps_12_bit_fields();
    test_current_torque_command_encodes_negative_value();
    test_hybrid_command_uses_forward_10020_kd_encoding();
    test_hybrid_command_uses_motor_specific_torque_limits();

    return 0;
}
