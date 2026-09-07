#pragma once
#include <vector>
#include <memory>
#include "internal/encos_can_protocol.hpp"
#include "internal/io.hpp"
#include "ecat/detail/config.hpp"

class IoController
{
public:
    bool control_enable = false;
    uint16_t slave_pos;
    uint32_t freq = 0;
    int64_t count_flag = 0;
    std::vector<int> disabled_motor_indices;
    std::vector<std::unique_ptr<io_data>> rx_;
    std::vector<std::unique_ptr<io_data>> tx_;

public:
    IoController() {}
    ~IoController() {}

    // encos
    EtherCAT_Msg Rx_Message;
    EtherCAT_Msg Tx_Message;

    void on_cycle_encos(int slave_pos);
    void send_motor_ctrl_cmd(EtherCAT_Msg *TxMessage, uint8_t passage, uint16_t motor_id, float kp, float kd, float pos, float spd, float tor, int slave_pos);
    // encos
    void set_motor_position(EtherCAT_Msg *TxMessage, uint8_t passage, uint16_t motor_id, float pos, uint16_t spd, uint16_t cur, uint8_t ack_status);
    void set_motor_speed(EtherCAT_Msg *tx_msg, uint8_t passage, uint16_t motor_id, float spd, uint16_t cur, uint8_t ack_status);
    void set_motor_cur_tor(EtherCAT_Msg *tx_msg, uint8_t passage, uint16_t motor_id, int16_t cur_tor, uint8_t ctrl_status, uint8_t ack_status);
    // double encons_get_positon(io_data* io , int passage);

    void encons_get_torque(io_data *io);
    void encons_get_angle_slave0(io_data *io);
    void encons_get_angular_velocity_slave0(io_data *io);

    void encons_get_angle_slave1(io_data *io);
    void encons_get_angular_velocity_slave1(io_data *io);

    void encons_get_actual_current_slave0(io_data *io);
    void encons_get_actual_current_slave1(io_data *io);

    void MotorIDReading(EtherCAT_Msg *tx_msg, int passage);
    void MotorIDSetting(EtherCAT_Msg *tx_msg, uint16_t motor_id, uint16_t motor_id_new, int passage);
    void MotorZeroSetting(EtherCAT_Msg *tx_msg, uint16_t motor_id, int passage);

    void on_cycle(bool enable);
    virtual void controlIO() = 0;
    virtual void bind_pdo() = 0;

    io_data_type tans(const std::string &d);
    void printIO(io_data *io);
    void setIO(io_data *io, int64_t value, float fvalue);
};

class IO_example_ro : public IoController
{
public:
    void controlIO();
    void bind_pdo() {}
};

class IO_example_rw : public IoController
{
public:
    void controlIO();
    void bind_pdo() {}
};
