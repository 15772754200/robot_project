#include "internal/encos_feedback_parser.hpp"

#include <array>
#include <cstdint>

namespace encos
{
namespace
{
constexpr int kMotorsPerSlave = 6;
constexpr int kEncosAxisCount = 12;
constexpr std::uint16_t kCurrentMask = 0x0FFF;

struct FeedbackField
{
    std::uint8_t high_sub_index;
    std::uint8_t low_sub_index;
};

constexpr std::array<FeedbackField, kMotorsPerSlave> kAngleFields = {
    {{7, 8}, {24, 25}, {41, 48}, {64, 65}, {81, 82}, {98, 99}}};

constexpr std::array<FeedbackField, kMotorsPerSlave> kVelocityFields = {
    {{9, 16}, {32, 33}, {49, 50}, {66, 67}, {83, 84}, {100, 101}}};

constexpr std::array<FeedbackField, kMotorsPerSlave> kCurrentFields = {
    {{16, 17}, {33, 34}, {50, 51}, {67, 68}, {84, 85}, {101, 102}}};

std::uint8_t read_u8(const io_data &io)
{
    return *reinterpret_cast<volatile std::uint8_t *>(io.io_address);
}

int axis_offset(int slave_pos)
{
    return slave_pos * kMotorsPerSlave;
}

template <typename OnValue>
void parse_u16_feedback(const io_data &io,
                        int slave_pos,
                        const std::array<FeedbackField, kMotorsPerSlave> &fields,
                        std::array<std::uint16_t, kEncosAxisCount> *scratch,
                        OnValue on_value)
{
    if (slave_pos < 0 || slave_pos > 1)
    {
        return;
    }

    const int base_axis = axis_offset(slave_pos);
    for (int motor = 0; motor < kMotorsPerSlave; ++motor)
    {
        const auto &field = fields[motor];
        const int axis_index = base_axis + motor;

        if (io.io_subIdx == field.high_sub_index)
        {
            (*scratch)[axis_index] = static_cast<std::uint16_t>(read_u8(io) << 8);
            return;
        }

        if (io.io_subIdx == field.low_sub_index)
        {
            (*scratch)[axis_index] |= read_u8(io);
            on_value(axis_index, (*scratch)[axis_index]);
            return;
        }
    }
}

} // namespace

void parse_angle_feedback(const io_data &io, int slave_pos, EncosAxisData *axis)
{
    static std::array<std::uint16_t, kEncosAxisCount> position{};
    if (axis == nullptr)
    {
        return;
    }

    parse_u16_feedback(
        io,
        slave_pos,
        kAngleFields,
        &position,
        [axis](int axis_index, std::uint16_t value) {
            axis->encos_angle_actual_value[axis_index] = value;
        });
}

void parse_velocity_feedback(const io_data &io, int slave_pos, EncosAxisData *axis)
{
    static std::array<std::uint16_t, kEncosAxisCount> velocity{};
    if (axis == nullptr)
    {
        return;
    }

    parse_u16_feedback(
        io,
        slave_pos,
        kVelocityFields,
        &velocity,
        [axis](int axis_index, std::uint16_t value) {
            axis->encos_angular_velocity_actual_value[axis_index] = value >> 4;
        });
}

void parse_current_feedback(const io_data &io, int slave_pos, EncosAxisData *axis)
{
    static std::array<std::uint16_t, kEncosAxisCount> current{};
    if (axis == nullptr)
    {
        return;
    }

    parse_u16_feedback(
        io,
        slave_pos,
        kCurrentFields,
        &current,
        [axis](int axis_index, std::uint16_t value) {
            axis->encos_current_actual_value[axis_index] = value & kCurrentMask;
        });
}

} // namespace encos
