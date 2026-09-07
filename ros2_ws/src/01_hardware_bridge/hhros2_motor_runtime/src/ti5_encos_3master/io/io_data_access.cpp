#include "internal/io_data_access.hpp"

#include "hhros2_log/log.h"

namespace
{
template <typename T>
T read_io_scalar(const io_data &io)
{
    return *reinterpret_cast<volatile T *>(io.io_address);
}

template <typename T>
void write_io_scalar(io_data *io, T value)
{
    *reinterpret_cast<volatile T *>(io->io_address) = value;
}
} // namespace

io_data_type parse_io_data_type(const std::string &data_type_name)
{
    if (data_type_name == "BOOL" || data_type_name == "BIT")
    {
        return io_data_type::BOOL;
    }
    if (data_type_name == "SINT")
    {
        return io_data_type::SINT;
    }
    if (data_type_name == "USINT")
    {
        return io_data_type::USINT;
    }
    if (data_type_name == "INT")
    {
        return io_data_type::INT;
    }
    if (data_type_name == "UINT")
    {
        return io_data_type::UINT;
    }
    if (data_type_name == "DINT")
    {
        return io_data_type::DINT;
    }
    if (data_type_name == "UDINT")
    {
        return io_data_type::UDINT;
    }
    if (data_type_name == "REAL")
    {
        return io_data_type::REAL;
    }
    return io_data_type::NONE;
}

void log_io_value(const io_data &io)
{
    switch (io.data_type)
    {
    case io_data_type::REAL:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) %f",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<float>(io));
        return;
    case io_data_type::DINT:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) %d",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<std::int32_t>(io));
        return;
    case io_data_type::INT:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) %d",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<std::int16_t>(io));
        return;
    case io_data_type::SINT:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) %d",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<std::int8_t>(io));
        return;
    case io_data_type::UDINT:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) 0x%08x",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<std::uint32_t>(io));
        return;
    case io_data_type::UINT:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) 0x%04x",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<std::uint16_t>(io));
        return;
    case io_data_type::USINT:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) 0x%02x",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  read_io_scalar<std::uint8_t>(io));
        return;
    case io_data_type::BOOL:
        LOG_DEBUG(LogType::MOTORLOG, "[IO] %d (0x%04x:%02d|%02d) %d",
                  io.slave_pos,
                  io.io_idx,
                  io.io_subIdx,
                  io.io_bit_pos,
                  io_read_bit(io.io_address, io.io_bit_pos));
        return;
    case io_data_type::NONE:
        return;
    }
}

void write_io_value(io_data *io, std::int64_t integer_value, float real_value)
{
    if (io == nullptr)
    {
        return;
    }

    switch (io->data_type)
    {
    case io_data_type::REAL:
        write_io_scalar(io, real_value);
        return;
    case io_data_type::DINT:
        write_io_scalar(io, static_cast<std::int32_t>(integer_value));
        return;
    case io_data_type::INT:
        write_io_scalar(io, static_cast<std::int16_t>(integer_value));
        return;
    case io_data_type::SINT:
        write_io_scalar(io, static_cast<std::int8_t>(integer_value));
        return;
    case io_data_type::UDINT:
        write_io_scalar(io, static_cast<std::uint32_t>(integer_value));
        return;
    case io_data_type::UINT:
        write_io_scalar(io, static_cast<std::uint16_t>(integer_value));
        return;
    case io_data_type::USINT:
        write_io_scalar(io, static_cast<std::uint8_t>(integer_value));
        return;
    case io_data_type::BOOL:
        io_write_bit(io->io_address, io->io_bit_pos, integer_value != 0);
        return;
    case io_data_type::NONE:
        return;
    }
}
