

#include "internal/ethercat_master_controller.hpp"
#include "internal/encos_io_controller.hpp"
#include "internal/ethercat_probe_recorder.hpp"
#include "internal/ipc_motor_server_api.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <thread>
#include <utility>

#include <sched.h>
#include "hhros2_log/log.h"
#include <unistd.h> // getpid

namespace
{
bool cpu_allowed(int cpu)
{
  if (cpu < 0)
  {
    return false;
  }

  cpu_set_t allowed_cpus;
  CPU_ZERO(&allowed_cpus);
  if (sched_getaffinity(0, sizeof(allowed_cpus), &allowed_cpus) != 0)
  {
    LOG_WARNING(LogType::MOTORLOG,"读取EtherCAT CPU affinity失败: %s", std::strerror(errno));
    return false;
  }
  return CPU_ISSET(cpu, &allowed_cpus);
}
} // namespace

EthercatMasterController::EthercatMasterController(
    int master_index,
    std::vector<int> enabled_slave_positions,
    std::vector<int> skipped_slave_positions,
    std::vector<int> disabled_motor_indices)
    : task(master_index),
      master_index_(master_index),
      enabled_slave_positions_(std::move(enabled_slave_positions)),
      skipped_slave_positions_(std::move(skipped_slave_positions)),
      disabled_motor_indices_(std::move(disabled_motor_indices))
{
}

EthercatMasterController::~EthercatMasterController()
{
}

struct IoControllerProgram
{
  IoController *io_con;
  void init() {}
  void operator()()
  {
    io_con->count_flag++;
    io_con->on_cycle_encos(io_con->slave_pos);
  }
};

bool EthercatMasterController::slave_position_is_listed(
    const std::vector<int> &slave_positions,
    std::uint16_t slave_pos) const
{
  const int target = static_cast<int>(slave_pos);
  return std::find(slave_positions.begin(), slave_positions.end(), target) !=
         slave_positions.end();
}

bool EthercatMasterController::should_register_slave(
    std::uint16_t slave_pos) const
{
  if (!enabled_slave_positions_.empty() &&
      !slave_position_is_listed(enabled_slave_positions_, slave_pos))
  {
    return false;
  }

  return !slave_position_is_listed(skipped_slave_positions_, slave_pos);
}

bool EthercatMasterController::motor_index_is_listed(
    const std::vector<int> &motor_indices,
    int motor_index) const
{
  return std::find(motor_indices.begin(), motor_indices.end(), motor_index) !=
         motor_indices.end();
}

bool EthercatMasterController::should_disable_motor(int motor_index) const
{
  return motor_index_is_listed(disabled_motor_indices_, motor_index);
}

/* 把扫描和控制动作交给vendor里的ecat::task */
void EthercatMasterController::init(int affinity, int priority, int interval, std::int64_t cycle_time, std::int64_t shiftTime, const std::string &fileName)
{
  cpu_affinity_ = affinity;   // 保存外部传进来的affinity,在后边set_receive_callback回调中使用
  task.priority(priority);

  cpu_set_t cpus;
  CPU_ZERO(&cpus);

  if (cpu_allowed(affinity))
  {
    CPU_SET(affinity, &cpus);
    task.cpu_affinity(&cpus, sizeof(cpus));
  }
  else if (affinity >= 0)
  {
     LOG_WARNING(LogType::MOTORLOG,
        "跳过EtherCAT master %d CPU affinity=%d，该CPU不在当前进程允许集合内",
        master_index_,
        affinity);
  }
  task.set_interval(interval);
  run_period = cycle_time;

  if (!fileName.empty())
  {
    LOG_INFO(LogType::MOTORLOG, "EtherCAT master %d uses ENI XML: %s", master_index_, fileName.c_str());
    task.load_eni(fileName, cycle_time);
  }
  else
  {
    LOG_INFO(LogType::MOTORLOG, "EtherCAT master %d uses ESI configuration", master_index_);
    task.cycle_time(cycle_time, shiftTime);
    task.dc_mode(ecat::dc_mode::master_follow_slave);
  }

  // 设置config的回调函数，将在task.start的时候被到用
  task.set_config_callback([&]
                           {

    std::uint16_t slave_count = task.slave_count();

    for (std::uint16_t slave_pos = 0; slave_pos < slave_count; slave_pos++)
    { 
      auto profile_no = task.profile_no(slave_pos);
      LOG_DEBUG(LogType::MOTORLOG,
          "EtherCAT master %d slave %d profile %d of %d slaves",
          master_index_,
          slave_pos,
          profile_no,
          slave_count);
      const bool register_slave = should_register_slave(slave_pos);
      if (!register_slave)
      {
        LOG_WARNING(LogType::MOTORLOG,
            "EtherCAT master %d slave %d profile %d is filtered out; "
            "no PDO will be registered for this slave",
            master_index_,
            slave_pos,
            profile_no);
      }

      if (profile_no == 402)
      {
        int n_axis_in_slave = 1, slot_pos;
        int slots_count = task.slots_count(slave_pos);
        int slots_index_increment = task.slot_index_increment(slave_pos);
        if (slots_count < 0)
        {
          LOG_ERROR(LogType::MOTORLOG,
              "EtherCAT master %d slave %d returned invalid slots_count=%d",
              master_index_,
              slave_pos,
              slots_count);
          continue;
        }
        if (slots_count > 0)
        {
          if (slots_index_increment < 0)
          {
            LOG_ERROR(LogType::MOTORLOG,
                "EtherCAT master %d slave %d returned invalid slot index increment=%d",
                master_index_,
                slave_pos,
                slots_index_increment);
            continue;
          }
          n_axis_in_slave = slots_count;
        }
        if (!register_slave)
        {
          axis_count += n_axis_in_slave;
          continue;
        }
        for (slot_pos = 0; slot_pos < n_axis_in_slave; ++slot_pos)
        {
          const int axis_id = axis_count++;
          if (should_disable_motor(axis_id))
          {
            LOG_WARNING(LogType::MOTORLOG,
                "EtherCAT master %d motor %d is disabled; "
                "axis PDO/control program will not be registered",
                master_index_,
                axis_id);
            continue;
          }

          const int index_offset = slot_pos * slots_index_increment;
          auto axis = std::make_unique<MotorAxisData>();
          axis->slave_pos = slave_pos;
          axis->axis_id = axis_id;

          axis->master_id = static_cast<std::uint16_t>(master_index_);
          

          //获取PDO对应的domain中的地址偏移量，可根据实际需要添加
          if (ti5_motor_control_mode == MotorControlMode::PositionMode)
          {// 位置控制模式
              // task.try_register_pdo_entry(axis->error_code, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x603f + index_offset), 0}); // error code
              // task.try_register_pdo_entry(axis->control_word, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6040 + index_offset), 0}); // control word;
              // task.try_register_pdo_entry(axis->status_word, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6041 + index_offset), 0}); // status word
              // task.try_register_pdo_entry(axis->mode_of_operation, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6060 + index_offset), 0}); // mode of operation
              // task.try_register_pdo_entry(axis->mode_of_operation_display, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6061 + index_offset), 0}); // mode of operation display
              // task.try_register_pdo_entry(axis->target_position, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x607a + index_offset), 0}); // target position
              // task.try_register_pdo_entry(axis->position_actual_value, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6064 + index_offset), 0}); // position actual value
              // task.try_register_pdo_entry(axis->target_torque, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6071 + index_offset), 0}); //target_torque
              // task.try_register_pdo_entry(axis->torque_actual_value, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x6077 + index_offset), 0}); //torque_actual_value
              // task.try_register_pdo_entry(axis->target_velocity, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x60ff + index_offset), 0}); //target_velocity   
              // task.try_register_pdo_entry(axis->velocity_actual_value, slave_pos,
              //                             {static_cast<ecat::pdo_index_type>(0x606c + index_offset), 0}); //velocity_actual_value      
          }
          else if (ti5_motor_control_mode == MotorControlMode::HybridForcePositionMode)
          {// 力位控制模式
              // CIA402标准区，多轴需要加偏移量
              task.try_register_pdo_entry(axis->error_code, slave_pos,
                              {static_cast<ecat::pdo_index_type>(0x603f + index_offset), 0});

              task.try_register_pdo_entry(axis->control_word, slave_pos,
                                          {static_cast<ecat::pdo_index_type>(0x6040 + index_offset), 0});

              task.try_register_pdo_entry(axis->status_word, slave_pos,
                                          {static_cast<ecat::pdo_index_type>(0x6041 + index_offset), 0});

              task.try_register_pdo_entry(axis->mode_of_operation, slave_pos,
                                          {static_cast<ecat::pdo_index_type>(0x6060 + index_offset), 0});

              task.try_register_pdo_entry(axis->mode_of_operation_display, slave_pos,
                                          {static_cast<ecat::pdo_index_type>(0x6061 + index_offset), 0});
              const bool kp_ok =
                  task.try_register_pdo_entry(axis->kp, slave_pos, {0x200F, 1});
              const bool kd_ok =
                  task.try_register_pdo_entry(axis->kd, slave_pos, {0x200F, 2});
              const bool target_pos_ok =
                  task.try_register_pdo_entry(axis->target_pos, slave_pos, {0x200F, 3});
              const bool target_vel_ok =
                  task.try_register_pdo_entry(axis->target_vel, slave_pos, {0x200F, 4});
              const bool target_tor_ok =
                  task.try_register_pdo_entry(axis->target_tor, slave_pos, {0x200F, 5});
              const bool actual_pos_ok =
                  task.try_register_pdo_entry(axis->actual_pos, slave_pos, {0x2010, 0});
              const bool actual_vel_ok =
                  task.try_register_pdo_entry(axis->actual_vel, slave_pos, {0x2011, 0});
              const bool actual_cur_ok =
                  task.try_register_pdo_entry(axis->actual_cur, slave_pos, {0x2012, 0});

              LOG_DEBUG(LogType::MOTORLOG,
                  "Registered hybrid PDOs for master %d slave %d axis %d: "
                  "kp=%d kd=%d target_pos=%d target_vel=%d target_tor=%d "
                  "actual_pos=%d actual_vel=%d actual_cur=%d",
                  master_index_,
                  slave_pos,
                  axis->axis_id,
                  kp_ok,
                  kd_ok,
                  target_pos_ok,
                  target_vel_ok,
                  target_tor_ok,
                  actual_pos_ok,
                  actual_vel_ok,
                  actual_cur_ok);
          }         
          //添加轴，以及对应的MotorAxisControlProgram
          programs.emplace_back(MotorAxisControlProgram{axis.get()});
          axes.push_back(std::move(axis));
        }
      }
      else if (profile_no == 401 || profile_no == 5001)
      { 
        if (!register_slave)
        {
          continue;
        }

        std::unique_ptr<IoController> io_ins;
        if (profile_no == 5001)
          io_ins = std::make_unique<IO_example_rw>();
        else
          io_ins = std::make_unique<IO_example_ro>();

        std::vector<ecat::pdo_entry_info> rxEntries = task.get_io_rx_config(slave_pos, 0);

        io_ins->slave_pos = slave_pos;
        io_ins->disabled_motor_indices = disabled_motor_indices_;

        for (auto& pdo : rxEntries) {
          if (pdo.entry_idx.idx == 0)
            continue;
          auto io = std::make_unique<io_data>();
          io->slave_pos = slave_pos;    
          io->io_idx = pdo.entry_idx.idx;
          io->io_subIdx = pdo.entry_idx.sub_idx;
          io->data_type = io_ins->tans(pdo.datatype);
          task.try_register_pdo_entry(io->io_address, pdo.bit_len, slave_pos, pdo.entry_idx, &io->io_bit_pos);
          io_ins->rx_.push_back(std::move(io));
        }

        std::vector<ecat::pdo_entry_info> txEntries = task.get_io_tx_config(slave_pos, 0);
        for (auto& pdo : txEntries) {
          if (pdo.entry_idx.idx == 0)
            continue;
          auto io = std::make_unique<io_data>();
          io->slave_pos = slave_pos;
          io->io_idx = pdo.entry_idx.idx;
          io->io_subIdx = pdo.entry_idx.sub_idx;
          io->data_type = io_ins->tans(pdo.datatype);
          task.try_register_pdo_entry(io->io_address, pdo.bit_len, slave_pos, pdo.entry_idx, &io->io_bit_pos);
          io_ins->tx_.push_back(std::move(io));
        }

        io_ins->bind_pdo();
        io_ins->control_enable = true;
        programs.emplace_back(IoControllerProgram{ io_ins.get() });
        io_controllers.push_back(std::move(io_ins));
      }

    }
    ecat::S2SConfig::use().count(); });

  // 设置cycle的回调函数，会在task.start的时候被调用
  // 执行状态机切换以及运动控制
  task.set_receive_callback([&] {
    // 先绑定当前线程到指定的 CPU
    bind_current_thread_to_cpu_once(cpu_affinity_, master_index_, &thread_affinity_bound_);
    for (auto &prog : programs)
    {
      prog();
    }
    record_ethercat_receive_probe(master_index_);
    update_master_cycle_status(master_index_, static_cast<int>(task.get_wcstate())); });
}

void EthercatMasterController::start()
{
  task.start();
}

void EthercatMasterController::startAsMaster()
{
  task.setAsMaster();
}

bool EthercatMasterController::startAsSlave(EthercatMasterController *master)
{
  LOG_INFO(LogType::MOTORLOG,
      "Start EtherCAT master %d as slave with period %lld and master period %lld",
      master_index_,
      static_cast<long long>(run_period),
      static_cast<long long>(master->run_period));
  if (run_period < master->run_period)
  {
    LOG_ERROR(LogType::MOTORLOG,
        "Start EtherCAT slave failed: period %lld must be >= master period %lld",
        static_cast<long long>(run_period),
        static_cast<long long>(master->run_period));
    return false;
  }

  if (run_period % master->run_period != 0)
  {
    LOG_ERROR(LogType::MOTORLOG,
        "Start EtherCAT slave failed: period %lld must be a multiple of master period %lld",
        static_cast<long long>(run_period),
        static_cast<long long>(master->run_period));
    return false;
  }

  task.setAsSlave(&master->task, run_period / master->run_period);
  return true;
}

bool EthercatMasterController::startAsSlave(EthercatMasterController *master, int num)
{
  LOG_INFO(LogType::MOTORLOG,
      "Start EtherCAT master %d as slave %d with period %lld and master period %lld",
      master_index_,
      num,
      static_cast<long long>(run_period),
      static_cast<long long>(master->run_period));
  if (run_period < master->run_period)
  {
    LOG_ERROR(LogType::MOTORLOG,
        "Start EtherCAT slave failed: period %lld must be >= master period %lld",
        static_cast<long long>(run_period),
        static_cast<long long>(master->run_period));
    return false;
  }

  if (run_period % master->run_period != 0)
  {
    LOG_ERROR(LogType::MOTORLOG,
        "Start EtherCAT slave failed: period %lld must be a multiple of master period %lld",
        static_cast<long long>(run_period),
        static_cast<long long>(master->run_period));
    return false;
  }

  task.setAsSlave(&master->task, run_period / master->run_period, num);
  return true;
}

void EthercatMasterController::wait()
{
  task.wait();
}

void EthercatMasterController::release()
{
  task.release();
}


void EthercatMasterController::bind_current_thread_to_cpu_once(
    int cpu,
    int master_index,
    bool *bound)
{
  if (bound == nullptr || *bound || cpu < 0)
  {
    return;
  }

  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  CPU_SET(cpu, &cpus);
  if (sched_setaffinity(0, sizeof(cpus), &cpus) != 0)
  {
    LOG_WARNING(LogType::MOTORLOG,
        "设置EtherCAT master %d当前线程CPU affinity=%d失败: %s",
        master_index,
        cpu,
        std::strerror(errno));
    *bound = true;
    return;
  }

  LOG_INFO(LogType::MOTORLOG,
      "EtherCAT master %d当前线程已绑定到CPU%d",
      master_index,
      cpu);
  *bound = true;
}
