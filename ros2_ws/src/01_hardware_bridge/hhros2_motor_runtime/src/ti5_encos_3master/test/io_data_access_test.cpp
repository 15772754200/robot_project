#include "internal/io_data_access.hpp"

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

void test_parse_io_data_type()
{
    EXPECT_TRUE(parse_io_data_type("BOOL") == io_data_type::BOOL);
    EXPECT_TRUE(parse_io_data_type("BIT") == io_data_type::BOOL);
    EXPECT_TRUE(parse_io_data_type("SINT") == io_data_type::SINT);
    EXPECT_TRUE(parse_io_data_type("USINT") == io_data_type::USINT);
    EXPECT_TRUE(parse_io_data_type("INT") == io_data_type::INT);
    EXPECT_TRUE(parse_io_data_type("UINT") == io_data_type::UINT);
    EXPECT_TRUE(parse_io_data_type("DINT") == io_data_type::DINT);
    EXPECT_TRUE(parse_io_data_type("UDINT") == io_data_type::UDINT);
    EXPECT_TRUE(parse_io_data_type("REAL") == io_data_type::REAL);
    EXPECT_TRUE(parse_io_data_type("UNKNOWN") == io_data_type::NONE);
}

void test_write_integer_io_value()
{
    std::uint32_t storage = 0;
    io_data io{};
    io.io_address = reinterpret_cast<volatile std::uint16_t *>(&storage);
    io.data_type = io_data_type::UDINT;

    write_io_value(&io, 0x12345678, 0.0F);
    EXPECT_TRUE(storage == 0x12345678U);
}

void test_write_bool_io_value()
{
    std::uint8_t storage = 0;
    io_data io{};
    io.io_address = reinterpret_cast<volatile std::uint16_t *>(&storage);
    io.data_type = io_data_type::BOOL;
    io.io_bit_pos = 3;

    write_io_value(&io, 1, 0.0F);
    EXPECT_TRUE(storage == 0x08U);

    write_io_value(&io, 0, 0.0F);
    EXPECT_TRUE(storage == 0x00U);
}
} // namespace

int main()
{
    test_parse_io_data_type();
    test_write_integer_io_value();
    test_write_bool_io_value();

    return 0;
}
