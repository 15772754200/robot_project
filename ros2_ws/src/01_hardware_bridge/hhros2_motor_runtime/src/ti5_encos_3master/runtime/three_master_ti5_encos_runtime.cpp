#include "hhros2_motor_runtime/ti5_encos_3master/three_master_ti5_encos_runtime.hpp"

#include "ecat/task.hpp"
#include "internal/ethercat_probe_recorder.hpp"
#include "internal/ipc_motor_server_api.hpp"
#include "internal/motor_shared_state.hpp"
#include "internal/ethercat_master_controller.hpp"
#include "internal/runtime_state_adapter.hpp"

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <qiuniu/init.h>
#include "hhros2_log/log.h"
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

std::atomic<bool> motor_enable{false};

#ifndef TI5_ENCOS_CONFIG_DIR
#define TI5_ENCOS_CONFIG_DIR ""
#endif

namespace
{

std::string config_file_path(const char *file_name)
{
    try
    {
        const auto share_dir =
            ament_index_cpp::get_package_share_directory("hhros2_motor_runtime");
        const std::string installed_path =
            share_dir + "/config/" + file_name;
        if (access(installed_path.c_str(), R_OK) == 0)
        {
            return installed_path;
        }
    }
    catch (const std::exception &)
    {
    }

    const std::string config_dir = TI5_ENCOS_CONFIG_DIR;
    if (config_dir.empty())
    {
        return {};
    }
    return config_dir + "/" + file_name;
}

} // namespace

namespace ti5_encos
{

struct ThreeMasterTi5EncosRuntime::Impl
{
    std::array<std::unique_ptr<EthercatMasterController>, kMasterNumber> masters;
};

RuntimeConfig::RuntimeConfig()
{
    for (int master_index = 0; master_index < kMasterNumber; ++master_index)
    {
        masters[master_index].cpu_affinity = master_index + 1;
    }
}

std::string bundled_eni_file_for_master(int master_index)
{
    const char *file_name = nullptr;
    if (master_index == 1)
    {
        file_name = "master1_eni.xml";
    }
    else if (master_index == 2)
    {
        file_name = "master2_eni.xml";
    }

    if (file_name == nullptr)
    {
        return {};
    }

    std::string path = config_file_path(file_name);
    if (path.empty() || access(path.c_str(), R_OK) != 0)
    {
        return {};
    }
    return path;
}

ThreeMasterTi5EncosRuntime::ThreeMasterTi5EncosRuntime(RuntimeConfig config)
    : config_(std::move(config)),
      impl_(std::make_unique<Impl>())
{
}

ThreeMasterTi5EncosRuntime::~ThreeMasterTi5EncosRuntime()
{
    request_stop();
    release();
}

bool ThreeMasterTi5EncosRuntime::initialize(std::string *error)
{
    /* 防止重复初始化 */
    if (initialized_)
    {
        return true;
    }

    if (config_.initialize_qiuniu)
    {
        qiuniu_init();
    }

    if (config_.lock_memory && mlockall(MCL_FUTURE | MCL_CURRENT) == -1)
    {
        set_error(error, std::string("failed to lock memory: ") + std::strerror(errno));
        return false;
    }

    initialize_default_motor_commands();

    for (int master_index = 0; master_index < kMasterNumber; ++master_index)
    {
        const auto &master_config = config_.masters[master_index];
        impl_->masters[master_index] =
            std::make_unique<EthercatMasterController>(
                master_index,
                master_config.enabled_slave_positions,
                master_config.skipped_slave_positions,
                master_config.disabled_motor_indices);
        impl_->masters[master_index]->init(master_config.cpu_affinity,
                                           master_config.priority,
                                           master_config.interval,
                                           master_config.cycle_time_ns,
                                           master_config.shift_time_ns,
                                           master_config.eni_file);
    }

    impl_->masters[0]->startAsMaster();   // 主站0作为时钟源
    if (!impl_->masters[1]->startAsSlave(impl_->masters[0].get()))    // 主站1同步到主站0
    {
        set_error(error, "start master1 as slave failed");
        return false;
    }
    if (!impl_->masters[2]->startAsSlave(impl_->masters[0].get(), 2)) // 主站2同步到主站0
    {
        set_error(error, "start master2 as slave failed");
        return false;
    }

    impl_->masters[0]->task.set_send_callback(
        []() { record_ethercat_send_probe(0); });
    impl_->masters[1]->task.set_send_callback(
        []() { record_ethercat_send_probe(1); });
    impl_->masters[2]->task.set_send_callback(
        []() { record_ethercat_send_probe(2); });

    ipc_motor_server_register_tasks(
        &impl_->masters[0]->task,
        &impl_->masters[1]->task,
        &impl_->masters[2]->task);
    
    if (config_.start_ipc_server)
    {
        configure_ipc_motor_server(
            config_.ipc_server_cpu_affinity,
            config_.ipc_server_priority);
        start_ipc_motor_server();
    }

    initialized_ = true;
    released_ = false;
    return true;
}

bool ThreeMasterTi5EncosRuntime::start(std::string *error)
{
    /* 防止重复启动 */
    if (started_)
    {
        return true;
    }
    /* 初始化底层资源 */
    if (!initialize(error))
    {
        return false;
    }
    /* 默认关闭电机使能，这里是一种安全设计，启动底层通信的时候，不应该默认让电机进入使能状态，否则可能出现意外运动 */
    motor_enable.store(false, std::memory_order_release);
    /* 启动所有master，注意impl_就是成员变量 */
    for (auto &master : impl_->masters)
    {
        master->start();
    }
    /* 对每个master记录任务 */
    for (auto &master : impl_->masters)
    {
        master->task.record(config_.record_tasks);
    }
    started_ = true;
    return true;
}

void ThreeMasterTi5EncosRuntime::request_stop()
{
    if (!initialized_)
    {
        return;
    }

    motor_enable.store(false, std::memory_order_release);
    stop_ipc_motor_server();
    ecat::RTTools::usleep(1000000);
    for (auto &master : impl_->masters)
    {
        if (master)
        {
            master->task.break_();
        }
    }
    started_ = false;
}

void ThreeMasterTi5EncosRuntime::wait()
{
    if (!initialized_)
    {
        return;
    }
    for (auto &master : impl_->masters)
    {
        if (master)
        {
            master->wait();
        }
    }
}

void ThreeMasterTi5EncosRuntime::release()
{
    if (!initialized_ || released_)
    {
        return;
    }

    for (auto &master : impl_->masters)
    {
        if (master)
        {
            master->release();
            master.reset();
        }
    }
    released_ = true;
    initialized_ = false;
    started_ = false;
}

bool ThreeMasterTi5EncosRuntime::initialized() const
{
    return initialized_;
}

bool ThreeMasterTi5EncosRuntime::started() const
{
    return started_;
}

void ThreeMasterTi5EncosRuntime::set_motor_enable(bool enable)
{
    motor_enable.store(enable, std::memory_order_release);
}

void ThreeMasterTi5EncosRuntime::set_motor_command(int master_index,
                                                   int motor_index,
                                                   const MotorCommand &command)
{
    if (!valid_motor_index(master_index, motor_index))
    {
        return;
    }

    set_motor_command_in_shared_state(&motor_msg,
                                      master_index,
                                      motor_index,
                                      command);
}

bool ThreeMasterTi5EncosRuntime::get_motor_state(int master_index,
                                                 int motor_index,
                                                 MotorState *state) const
{
    if (state == nullptr || !valid_motor_index(master_index, motor_index))
    {
        return false;
    }

    return get_motor_state_from_shared_state(motor_msg,
                                             master_index,
                                             motor_index,
                                             state);
}

RuntimeSnapshot ThreeMasterTi5EncosRuntime::snapshot() const
{
    RuntimeSnapshot snapshot;
    for (int master_index = 0; master_index < kMasterNumber; ++master_index)
    {
        for (int motor_index = 0; motor_index < kMotorNumber; ++motor_index)
        {
            get_motor_state(master_index, motor_index, &snapshot.motors[master_index][motor_index]);
        }
    }

    Ethercat_comm_diag diag{};
    ipc_motor_server_sample_diag(&diag);
    copy_diag_to_runtime_snapshot(diag, &snapshot.diag);
    return snapshot;
}

void ThreeMasterTi5EncosRuntime::initialize_default_motor_commands()
{
    ti5_encos::initialize_default_motor_commands(&motor_msg);
}

void ThreeMasterTi5EncosRuntime::set_error(std::string *error, const std::string &message) const
{
    if (error != nullptr)
    {
        *error = message;
    }
}

} // namespace ti5_encos
