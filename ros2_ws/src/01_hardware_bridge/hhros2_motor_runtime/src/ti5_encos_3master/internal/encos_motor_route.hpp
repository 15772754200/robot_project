#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace encos
{

/**
 * @brief Mapping from runtime motor slot to ENCOS CAN transport identity.
 *
 * `passage` is the 1-based CAN frame slot on the EtherCAT-to-CAN slave.
 * `motor_id` is the CAN identifier written into that slot.  They are equal in
 * the default wiring, but they are separate concepts and must be changed
 * together through this table when the hardware topology changes.
 */
struct MotorRoute
{
    std::uint8_t passage = 0;
    std::uint16_t motor_id = 0;
};

inline constexpr std::size_t kMotorsPerEncosSlave = 6U;

/**
 * @brief Default ENCOS routing for one EtherCAT-to-CAN slave.
 *
 * The current robot wiring uses passage N for CAN motor ID N.  Keep all ENCOS
 * command paths routed through this table so normal commands and disabled
 * commands cannot drift apart.
 */
inline constexpr std::array<MotorRoute, kMotorsPerEncosSlave>
    kDefaultMotorRoutes = {{
        {1, 1},
        {2, 2},
        {3, 3},
        {4, 4},
        {5, 5},
        {6, 6},
    }};

inline constexpr bool is_valid_motor_offset(int motor_offset)
{
    return motor_offset >= 0 &&
           static_cast<std::size_t>(motor_offset) <
               kDefaultMotorRoutes.size();
}

inline constexpr const MotorRoute *motor_route_for_offset(int motor_offset)
{
    return is_valid_motor_offset(motor_offset)
               ? &kDefaultMotorRoutes[static_cast<std::size_t>(motor_offset)]
               : nullptr;
}

} // namespace encos
