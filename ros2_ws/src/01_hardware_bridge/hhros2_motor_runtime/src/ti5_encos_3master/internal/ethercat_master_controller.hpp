#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "hhros2_log/log.h"

#include "ecat/task.hpp"
#include "internal/cia402_power_controller.hpp"
#include "internal/io.hpp"
#include "internal/motor_axis_data.hpp"
#include "internal/ti5_motor_command_adapter.hpp"

extern std::atomic<bool> motor_enable;
class IoController;

struct MotorAxisControlProgram
{
    MotorAxisData *axis;
    Cia402PowerController power_;
    Ti5MotorCommandAdapter command_adapter_;

    void init() {}

    void operator()()
    {
        power_.axis = axis;
        power_.enable = motor_enable.load(std::memory_order_acquire);

        command_adapter_.axis = axis;
        command_adapter_.execute = power_.status;

        power_.on_cycle();
        command_adapter_.on_cycle();
    }
};

struct DigitalOutputTestProgram
{
    io_data *io;
    int count = 0;
    void init() {}
    void operator()()
    {
        count++;
        if (count < 1000)
        {
            *io->io_address = 0xffff;
        }
        else
        {
            *io->io_address = 0;
        }
        count %= 10000;
    }
};

struct DigitalInputLogProgram
{
    io_data *io;
    int count = 0;
    void init() {}
    void operator()()
    {
        count++;
        if (!(count % 1000))
        {
            const std::uint16_t val = *io->io_address;
            LOG_DEBUG(LogType::MOTORLOG,
                      "Read slave %d io %d value %d",
                      io->slave_pos,
                      io->io_idx,
                      val);
        }
        count %= 10000;
    }
};

class EthercatMasterController
{
public:
    EthercatMasterController(
        int master_index,
        std::vector<int> enabled_slave_positions = {},
        std::vector<int> skipped_slave_positions = {},
        std::vector<int> disabled_motor_indices = {});
    ~EthercatMasterController();

    void init(int affinity,
              int priority,
              int interval,
              std::int64_t cycle_time,
              std::int64_t shift_time,
              const std::string &eni_file);
    void start();
    void startAsMaster();
    bool startAsSlave(EthercatMasterController *master);
    bool startAsSlave(EthercatMasterController *master, int num);
    void wait();
    void release();

    int run_period = 1000000;
    ecat::task task;
    int axis_count = 0;
    std::vector<std::unique_ptr<MotorAxisData>> axes;
    std::vector<std::unique_ptr<io_data>> io_output;
    std::vector<std::unique_ptr<io_data>> io_input;
    std::vector<std::function<void()>> programs;
    std::vector<std::unique_ptr<IoController>> io_controllers;

private:
    bool slave_position_is_listed(
        const std::vector<int> &slave_positions,
        std::uint16_t slave_pos) const;
    bool should_register_slave(std::uint16_t slave_pos) const;
    bool motor_index_is_listed(
        const std::vector<int> &motor_indices,
        int motor_index) const;
    bool should_disable_motor(int motor_index) const;

    int master_index_;
    std::vector<int> enabled_slave_positions_;
    std::vector<int> skipped_slave_positions_;
    std::vector<int> disabled_motor_indices_;
private:
    int cpu_affinity_ = -1;   // 存储绑定的 CPU
    bool thread_affinity_bound_ = false;  // 防止重复绑定
    void bind_current_thread_to_cpu_once(int cpu,int master_index,bool *bound);
};
