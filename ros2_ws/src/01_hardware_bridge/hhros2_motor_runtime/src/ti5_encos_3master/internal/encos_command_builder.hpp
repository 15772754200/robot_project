#pragma once

#include "internal/encos_can_protocol.hpp"

#include <cstdint>

namespace encos
{

bool build_hybrid_force_position_command(EtherCAT_Msg *message,
                                         std::uint8_t passage,
                                         std::uint16_t motor_id,
                                         float kp,
                                         float kd,
                                         float position,
                                         float speed,
                                         float torque);

bool build_position_command(EtherCAT_Msg *message,
                            std::uint8_t passage,
                            std::uint16_t motor_id,
                            float position,
                            std::uint16_t speed,
                            std::uint16_t current,
                            std::uint8_t ack_status);

bool build_current_torque_command(EtherCAT_Msg *message,
                                  std::uint8_t passage,
                                  std::uint16_t motor_id,
                                  std::int16_t current_or_torque,
                                  std::uint8_t control_status,
                                  std::uint8_t ack_status);

bool build_motor_speed_command(EtherCAT_Msg *message,
                               std::uint8_t passage,
                               std::uint16_t motor_id,
                               float speed,
                               std::uint16_t current,
                               std::uint8_t ack_status);

bool build_motor_id_reading_command(EtherCAT_Msg *message,
                                    std::uint8_t passage);

bool build_motor_id_setting_command(EtherCAT_Msg *message,
                                    std::uint8_t passage,
                                    std::uint16_t motor_id,
                                    std::uint16_t new_motor_id);

bool build_motor_zero_setting_command(EtherCAT_Msg *message,
                                      std::uint8_t passage,
                                      std::uint16_t motor_id);

} // namespace encos
