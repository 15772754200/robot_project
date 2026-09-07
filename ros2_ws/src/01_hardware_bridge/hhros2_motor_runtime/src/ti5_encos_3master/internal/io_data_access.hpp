#pragma once

#include "internal/io.hpp"

#include <cstdint>
#include <string>

io_data_type parse_io_data_type(const std::string &data_type_name);

void log_io_value(const io_data &io);

void write_io_value(io_data *io, std::int64_t integer_value, float real_value);
