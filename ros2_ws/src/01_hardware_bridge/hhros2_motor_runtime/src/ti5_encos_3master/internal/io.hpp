#pragma once

#include <cstdint>

enum io_data_type : char
{
    NONE,
    BOOL,
    SINT,
    USINT,
    INT,
    UINT,
    DINT,
    UDINT,
    REAL,
};

struct io_data
{
  std::uint16_t io_idx;
  std::uint8_t io_subIdx;
  std::uint16_t slave_pos;
  io_data_type data_type;

  volatile std::uint16_t *io_address;
  std::uint8_t io_bit_pos;
};

template <typename T>
void io_write_bit(T *address, std::uint8_t pos, bool val)
{
    auto *byte = reinterpret_cast<volatile std::uint8_t *>(address);
    if (val)
    {
        *byte |= static_cast<std::uint8_t>(1U << pos);
    }
    else
    {
        *byte &= static_cast<std::uint8_t>(~(1U << pos));
    }
}
 
template <typename T>
bool io_read_bit(T *address, std::uint8_t pos)
{
    const auto *byte = reinterpret_cast<const volatile std::uint8_t *>(address);
    return ((*byte >> pos) & 0x01U) != 0U;
}
