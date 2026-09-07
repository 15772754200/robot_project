// 英克斯电机控制
#include "internal/encos_command_builder.hpp"
#include "internal/encos_feedback_parser.hpp"
#include "internal/encos_io_controller.hpp"
#include "internal/encos_motor_route.hpp"
#include "internal/io_data_access.hpp"
#include "internal/motor_axis_data.hpp"
#include "internal/motor_shared_state.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include "hhros2_log/log.h"

EncosAxisData encos_axis;
MotorControlMode encos_motor_control_mode = MotorControlMode::HybridForcePositionMode; // 默认力位混控模式，如果需要调整电机控制模式，修改改变量即可
extern std::atomic<bool> motor_enable;

namespace
{
constexpr int kEncosMasterIndex = 0;    // encos挂载在第0个主站
constexpr int kEncosMotorsPerSlave =
    static_cast<int>(encos::kMotorsPerEncosSlave); // 每个从站下挂载6个电机
constexpr int kCanPdoFieldCount = 11;   // 一个can报文映射到pdo里有11个字段

enum class CanPdoField
{
    MotorId = 0,
    RemoteTransmissionRequest,
    DataLengthCode,
    Data0,
    Data1,
    Data2,
    Data3,
    Data4,
    Data5,
    Data6,
    Data7,
};

constexpr std::array<std::array<int, kCanPdoFieldCount>, kEncosMotorsPerSlave>
    kCanTxPdoSubIndexes = {{{3,   4,  5,  6,  7,  8,  9, 16, 17, 18, 19},  // 这个就是第一个电机的报文
                            {20, 21, 22, 23, 24, 25, 32, 33, 34, 35, 36},
                            {37, 38, 39, 40, 41, 48, 49, 50, 51, 52, 53},
                            {54, 55, 56, 57, 64, 65, 66, 67, 68, 69, 70},
                            {71, 72, 73, 80, 81, 82, 83, 84, 85, 86, 87},
                            {88, 89, 96, 97, 98, 99, 100, 101, 102, 103, 104}}};

bool is_supported_encos_slave(int slave_pos)
{
    return slave_pos == 0 || slave_pos == 1;
}

int first_motor_index_for_slave(int slave_pos)
{
    return slave_pos * kEncosMotorsPerSlave;
}

bool motor_is_disabled(const IoController *controller, int motor_index)
{
    if (controller == nullptr)
    {
        return false;
    }

    const auto &disabled_motor_indices = controller->disabled_motor_indices;
    return std::find(
               disabled_motor_indices.begin(),
               disabled_motor_indices.end(),
               motor_index) != disabled_motor_indices.end();
}

void send_disabled_command_for_motor(IoController *controller,
                                     int slave_pos,
                                     int motor_offset)
{
    const auto *route = encos::motor_route_for_offset(motor_offset);
    if (route == nullptr)
    {
        return;
    }

    if (encos_motor_control_mode == MotorControlMode::PositionMode)
    {
        // controller->set_motor_position(
        //     &controller->Tx_Message,
        //     route->passage,
        //     route->motor_id,
        //     0.0f,
        //     0,
        //     0,
        //     0);
    }
    else if (encos_motor_control_mode == MotorControlMode::HybridForcePositionMode)
    {
        controller->send_motor_ctrl_cmd(
            &controller->Tx_Message,
            route->passage,
            route->motor_id,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            slave_pos);
    }
}

int can_pdo_value(const EtherCAT_Msg &message, int motor_index, CanPdoField field)
{
    switch (field)
    {
    case CanPdoField::MotorId:
        return message.motor[motor_index].id;
    case CanPdoField::RemoteTransmissionRequest:
        return message.motor[motor_index].rtr;
    case CanPdoField::DataLengthCode:
        return message.motor[motor_index].dlc;
    case CanPdoField::Data0:
        return message.motor[motor_index].data[0];
    case CanPdoField::Data1:
        return message.motor[motor_index].data[1];
    case CanPdoField::Data2:
        return message.motor[motor_index].data[2];
    case CanPdoField::Data3:
        return message.motor[motor_index].data[3];
    case CanPdoField::Data4:
        return message.motor[motor_index].data[4];
    case CanPdoField::Data5:
        return message.motor[motor_index].data[5];
    case CanPdoField::Data6:
        return message.motor[motor_index].data[6];
    case CanPdoField::Data7:
        return message.motor[motor_index].data[7];
    }

    return 0;
}
/*
 * @brief: 这个是把写给can的变成pdo
 */
void write_can_tx_to_pdo(IoController *controller,
                         const EtherCAT_Msg &message,
                         uint8_t passage)
{
    if (passage < 1 || passage > kEncosMotorsPerSlave)
    {
        return;
    }

    const int motor_index = passage - 1;
    const auto &sub_indexes = kCanTxPdoSubIndexes[motor_index]; // 从大数组中，拿到电机小数组

    for (auto &io : controller->rx_)
    {
        for (int field_index = 0; field_index < kCanPdoFieldCount; ++field_index)
        {
            if (io->io_subIdx != sub_indexes[field_index])
            {
                continue;
            }
            // 把不需要的pdo位置给跳过，就留下与转can板对应的
            const int64_t value = can_pdo_value(
                message,
                motor_index,
                static_cast<CanPdoField>(field_index));
            controller->setIO(io.get(), value, static_cast<float>(value));
            break;
        }
    }
}

void send_position_commands(IoController *controller, int slave_pos)
{
    const int first_motor_index = first_motor_index_for_slave(slave_pos);
    std::array<Motor_msg_single, kEncosMotorsPerSlave> commands{};
    {
        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        for (int motor_offset = 0; motor_offset < kEncosMotorsPerSlave; ++motor_offset)
        {
            commands[motor_offset] =
                motor_msg.master_id[kEncosMasterIndex]
                    .motor_id[first_motor_index + motor_offset];
        }
    }

    for (int motor_offset = 0; motor_offset < kEncosMotorsPerSlave; ++motor_offset)
    {
        const auto *route = encos::motor_route_for_offset(motor_offset);
        if (route == nullptr)
        {
            continue;
        }

        if (motor_is_disabled(controller, first_motor_index + motor_offset))
        {
            send_disabled_command_for_motor(controller, slave_pos, motor_offset);
            continue;
        }

        controller->set_motor_position(
            &controller->Tx_Message,
            route->passage,
            route->motor_id,
            commands[motor_offset].position_cmd / 1000.0f,
            50,
            300,
            1);
    }
}

void send_hybrid_force_position_commands(IoController *controller, int slave_pos)
{
    const int first_motor_index = first_motor_index_for_slave(slave_pos);
    std::array<Motor_msg_single, kEncosMotorsPerSlave> commands{};
    {
        std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
        for (int motor_offset = 0; motor_offset < kEncosMotorsPerSlave; ++motor_offset)
        {
            commands[motor_offset] =
                motor_msg.master_id[kEncosMasterIndex]
                    .motor_id[first_motor_index + motor_offset];
        }
    }

    for (int motor_offset = 0; motor_offset < kEncosMotorsPerSlave; ++motor_offset)
    {
        const auto *route = encos::motor_route_for_offset(motor_offset);
        if (route == nullptr)
        {
            continue;
        }

        if (motor_is_disabled(controller, first_motor_index + motor_offset))
        {
            send_disabled_command_for_motor(controller, slave_pos, motor_offset);
            continue;
        }

        const auto &command = commands[motor_offset];
        controller->send_motor_ctrl_cmd(
            &controller->Tx_Message,
            route->passage,
            route->motor_id,
            command.kp_cmd,
            command.kd_cmd,
            command.position_cmd,
            command.velocity_cmd,
            command.Tor_cmd,
            slave_pos);
    }
}

void send_disabled_commands(IoController *controller, int slave_pos)
{
    for (int motor_offset = 0; motor_offset < kEncosMotorsPerSlave; ++motor_offset)
    {
        send_disabled_command_for_motor(controller, slave_pos, motor_offset);
    }
}

void collect_slave_feedback(IoController *controller, int slave_pos)
{
    for (auto &io : controller->tx_)
    {
        if (slave_pos == 0)
        {
            controller->encons_get_angle_slave0(io.get());
            controller->encons_get_angular_velocity_slave0(io.get());
            controller->encons_get_actual_current_slave0(io.get());
        }
        else
        {
            controller->encons_get_angle_slave1(io.get());
            controller->encons_get_angular_velocity_slave1(io.get());
            controller->encons_get_actual_current_slave1(io.get());
        }
    }
}

void publish_slave_feedback(int slave_pos)
{
    const int first_motor_index = first_motor_index_for_slave(slave_pos);
    std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
    for (int motor_offset = 0; motor_offset < kEncosMotorsPerSlave; ++motor_offset)
    {
        const int motor_index = first_motor_index + motor_offset;
        auto &feedback =
            motor_msg.master_id[kEncosMasterIndex].motor_id[motor_index];
        feedback.position_actual = encos_axis.encos_angle_actual_value[motor_index];
        feedback.velocity_actual =
            encos_axis.encos_angular_velocity_actual_value[motor_index];
        feedback.current_actual = encos_axis.encos_current_actual_value[motor_index];
    }
}
} // namespace

void IoController::on_cycle_encos(int slave_pos)
{
    static bool reading_motor_id_flag = false;
    static bool setting_motor_id_flag = false;

    if (!is_supported_encos_slave(slave_pos))
    {
        return;
    }

    if (reading_motor_id_flag || setting_motor_id_flag)
    {
        if (slave_pos != 0)
        {
            return;
        }

        if (reading_motor_id_flag)
        {
            MotorIDReading(&Tx_Message, 1);
            MotorIDReading(&Tx_Message, 2);
            MotorIDReading(&Tx_Message, 3);
        }

        if (setting_motor_id_flag)
        {
            MotorIDSetting(&Tx_Message, 1, 3, 1);
        }

        for (auto &io : tx_)
        {
            printIO(io.get());
        }
        LOG_DEBUG(LogType::MOTORLOG,"[IO] end");
        return;
    }

    const bool enabled = motor_enable.load(std::memory_order_acquire);
    if (!enabled)
    {
        send_disabled_commands(this, slave_pos);
    }
    else if (encos_motor_control_mode == MotorControlMode::PositionMode)
    {
        // send_position_commands(this, slave_pos);
    }
    else if (encos_motor_control_mode == MotorControlMode::HybridForcePositionMode)
    {
        send_hybrid_force_position_commands(this, slave_pos);
    }

    collect_slave_feedback(this, slave_pos);
    publish_slave_feedback(slave_pos);
}

// encos
void IoController::send_motor_ctrl_cmd(EtherCAT_Msg *TxMessage, uint8_t passage, uint16_t motor_id, float kp, float kd, float pos, float spd, float tor, int slave_pos)
{
    if (encos::build_hybrid_force_position_command(
            TxMessage, passage, motor_id, kp, kd, pos, spd, tor))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::set_motor_position(EtherCAT_Msg *TxMessage, uint8_t passage, uint16_t motor_id, float pos, uint16_t spd, uint16_t cur, uint8_t ack_status)
{
    if (encos::build_position_command(
            TxMessage, passage, motor_id, pos, spd, cur, ack_status))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::set_motor_cur_tor(EtherCAT_Msg *TxMessage, uint8_t passage, uint16_t motor_id, int16_t cur_tor,
                                     uint8_t ctrl_status, uint8_t ack_status)
{
    if (encos::build_current_torque_command(
            TxMessage, passage, motor_id, cur_tor, ctrl_status, ack_status))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::on_cycle(bool enable)
{
    if (!enable)
        return;
    controlIO();
}

io_data_type IoController::tans(const std::string &d)
{
    return parse_io_data_type(d);
}

/*************************************encons_get_angle************************************************ */

void IoController::encons_get_angle_slave0(io_data *io)
{
    encos::parse_angle_feedback(*io, 0, &encos_axis);
}

void IoController::encons_get_angle_slave1(io_data *io)
{
    encos::parse_angle_feedback(*io, 1, &encos_axis);
}
/*************************************encons_get_angle************************************************ */

/*************************************encons_get_angular_velocity************************************************ */
void IoController::encons_get_angular_velocity_slave0(io_data *io)
{
    encos::parse_velocity_feedback(*io, 0, &encos_axis);
}

void IoController::encons_get_angular_velocity_slave1(io_data *io)
{
    encos::parse_velocity_feedback(*io, 1, &encos_axis);
}

/*************************************encons_get_angular_velocity************************************************ */

/*************************************encons_get_actual_current************************************************ */
void IoController::encons_get_actual_current_slave0(io_data *io)
{
    encos::parse_current_feedback(*io, 0, &encos_axis);
}
void IoController::encons_get_actual_current_slave1(io_data *io)
{
    encos::parse_current_feedback(*io, 1, &encos_axis);
}
/*************************************encons_get_actual_current************************************************ */

void IoController::MotorIDReading(EtherCAT_Msg *TxMessage, int passage)
{
    if (encos::build_motor_id_reading_command(TxMessage, passage))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::MotorIDSetting(EtherCAT_Msg *TxMessage, uint16_t motor_id, uint16_t motor_id_new, int passage)
{
    if (encos::build_motor_id_setting_command(
            TxMessage, passage, motor_id, motor_id_new))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::MotorZeroSetting(EtherCAT_Msg *TxMessage, uint16_t motor_id, int passage)
{
    if (encos::build_motor_zero_setting_command(TxMessage, passage, motor_id))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::set_motor_speed(EtherCAT_Msg *TxMessage, uint8_t passage, uint16_t motor_id, float spd, uint16_t cur, uint8_t ack_status)
{
    if (encos::build_motor_speed_command(
            TxMessage, passage, motor_id, spd, cur, ack_status))
    {
        write_can_tx_to_pdo(this, *TxMessage, passage);
    }
}

void IoController::encons_get_torque(io_data *io)
{
    static std::uint16_t current_12bit = 0;
    if (io->io_subIdx == 84)
    {
        current_12bit =
            *reinterpret_cast<volatile std::uint8_t *>(io->io_address);
        current_12bit &= 0x000F;
        current_12bit = current_12bit << 8;
    }
    if (io->io_subIdx == 85)
    {
        current_12bit |=
            *reinterpret_cast<volatile std::uint8_t *>(io->io_address);
        const double actual_current = (current_12bit - 2047.5) * 60.0 / 4095.0;
        LOG_DEBUG(LogType::MOTORLOG, "[IO] 12-bit current raw=%u, actual=%.2fA",
                  current_12bit, actual_current);
    }
}

void IoController::printIO(io_data *io)
{
    if (io == nullptr)
    {
        return;
    }

    log_io_value(*io);
}

void IoController::setIO(io_data *io, int64_t value, float fvalue)
{
    write_io_value(io, value, fvalue);
}

void IO_example_ro::controlIO()
{
    static uint32_t count_ro = 0;
    count_ro++;
    if (!(count_ro % freq))
    {
        for (auto &io : tx_)
        {
            printIO(io.get());
        }
        LOG_DEBUG(LogType::MOTORLOG,"[IO] read-only example cycle end");
    }
    count_ro %= 10000;
}

void IO_example_rw::controlIO()
{
    static uint32_t count_rw = 0;
    static int64_t value = 0;
    static float fvalue = 0.0;
    count_rw++;
    if (!(count_rw % freq))
    {
        for (auto &io : tx_)
        {
            printIO(io.get());
        }
        for (auto &io : rx_)
        {
            setIO(io.get(), value, fvalue);
        }
        value = ~value;
        LOG_DEBUG(LogType::MOTORLOG,"[IO] read-write example cycle end");
    }
    count_rw %= 10000;
}
