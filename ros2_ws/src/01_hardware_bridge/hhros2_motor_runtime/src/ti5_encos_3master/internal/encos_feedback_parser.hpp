#pragma once

#include "internal/io.hpp"
#include "internal/motor_axis_data.hpp"

namespace encos
{

void parse_angle_feedback(const io_data &io, int slave_pos, EncosAxisData *axis);
void parse_velocity_feedback(const io_data &io, int slave_pos, EncosAxisData *axis);
void parse_current_feedback(const io_data &io, int slave_pos, EncosAxisData *axis);

} // namespace encos
