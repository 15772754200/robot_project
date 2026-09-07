#include "internal/encos_feedback_parser.hpp"

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

io_data make_io(std::uint8_t sub_index, volatile std::uint16_t *storage)
{
    io_data io{};
    io.io_subIdx = sub_index;
    io.io_address = storage;
    return io;
}

void parse_byte(void (*parser)(const io_data &, int, EncosAxisData *),
                std::uint8_t sub_index,
                std::uint8_t value,
                int slave_pos,
                EncosAxisData *axis)
{
    volatile std::uint16_t storage = value;
    const auto io = make_io(sub_index, &storage);
    parser(io, slave_pos, axis);
}

void expect_angle_feedback()
{
    EncosAxisData axis{};

    parse_byte(encos::parse_angle_feedback, 7, 0x12, 0, &axis);
    parse_byte(encos::parse_angle_feedback, 8, 0x34, 0, &axis);
    EXPECT_TRUE(axis.encos_angle_actual_value[0] == 0x1234);

    parse_byte(encos::parse_angle_feedback, 98, 0xAB, 1, &axis);
    parse_byte(encos::parse_angle_feedback, 99, 0xCD, 1, &axis);
    EXPECT_TRUE(axis.encos_angle_actual_value[11] == 0xABCD);
}

void expect_velocity_feedback()
{
    EncosAxisData axis{};

    parse_byte(encos::parse_velocity_feedback, 9, 0x12, 0, &axis);
    parse_byte(encos::parse_velocity_feedback, 16, 0x30, 0, &axis);
    EXPECT_TRUE(axis.encos_angular_velocity_actual_value[0] == 0x0123);

    parse_byte(encos::parse_velocity_feedback, 100, 0xF0, 1, &axis);
    parse_byte(encos::parse_velocity_feedback, 101, 0x00, 1, &axis);
    EXPECT_TRUE(axis.encos_angular_velocity_actual_value[11] == 0x0F00);
}

void expect_current_feedback()
{
    EncosAxisData axis{};

    parse_byte(encos::parse_current_feedback, 16, 0xF2, 0, &axis);
    parse_byte(encos::parse_current_feedback, 17, 0x34, 0, &axis);
    EXPECT_TRUE(axis.encos_current_actual_value[0] == 0x0234);

    parse_byte(encos::parse_current_feedback, 101, 0xA5, 1, &axis);
    parse_byte(encos::parse_current_feedback, 102, 0x67, 1, &axis);
    EXPECT_TRUE(axis.encos_current_actual_value[11] == 0x0567);
}

void expect_invalid_input_is_ignored()
{
    EncosAxisData axis{};

    parse_byte(encos::parse_angle_feedback, 7, 0xFF, -1, &axis);
    parse_byte(encos::parse_angle_feedback, 8, 0xFF, -1, &axis);
    EXPECT_TRUE(axis.encos_angle_actual_value[0] == 0.0);

    parse_byte(encos::parse_current_feedback, 1, 0xFF, 0, &axis);
    EXPECT_TRUE(axis.encos_current_actual_value[0] == 0.0);
}

} // namespace

int main()
{
    expect_angle_feedback();
    expect_velocity_feedback();
    expect_current_feedback();
    expect_invalid_input_is_ignored();
    return 0;
}
