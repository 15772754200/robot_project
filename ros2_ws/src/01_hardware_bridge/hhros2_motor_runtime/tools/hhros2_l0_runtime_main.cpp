/**
 * @file hhros2_l0_runtime_main.cpp
 * @brief Standalone L0 real-time runtime entry point (NO ROS 2).
 *
 * Runs the hard real-time loop that owns the EtherCAT bus / actuators and the
 * IMU, and exposes the 23-joint + IMU state through the hhros2 shared-memory
 * ABI. The ros2_control HAL attaches as a client. Because this process holds no
 * ROS middleware, ROS-side jitter or crashes never perturb the bus loop.
 *
 * Backend selection:
 *   --backend sim   -> SimL0Backend (default; runs anywhere, for bring-up)
 *   --backend ecat  -> EtherCAT backend wrapping ThreeMasterTi5EncosRuntime
 *                      (compiled only with -DROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=ON;
 *                       integration hook documented below).
 *
 * Real-time hygiene (Linux): mlockall + SCHED_FIFO + CPU affinity should be
 * applied here exactly as the legacy runtime does. Kept minimal in this entry
 * point so it builds and runs on a dev host.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"
#include "hhros2_motor_runtime/hhros2_l0_backend.hpp"
#include "hhros2_motor_runtime/hhros2_l0_shm_server.hpp"
#include "hhros2_log/log.h"

// 条件包含 EtherCAT 后端头文件
#ifdef HHROS2_HAVE_ECAT_BACKEND
#include "hhros2_motor_runtime/hhros2_ecat_l0_backend.hpp"
#endif

namespace
{
std::atomic<bool> g_running{true};
void handle_signal(int) { g_running.store(false); }

struct Options
{
    std::string shm_name = hhros2::shm::kDefaultShmName;
    std::string backend = "sim";
    double rate_hz = 1000.0;
};

Options parse(int argc, char ** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--shm" && i + 1 < argc) o.shm_name = argv[++i];
        else if (a == "--backend" && i + 1 < argc) o.backend = argv[++i];
        else if (a == "--rate" && i + 1 < argc) o.rate_hz = std::stod(argv[++i]);
    }
    return o;
}

std::unique_ptr<hhros2_l0::L0HardwareBackend> make_backend(const std::string & name)
{
    if (name == "ecat") {
        #ifdef HHROS2_HAVE_ECAT_BACKEND
            return std::make_unique<hhros2_l0::EcatL0Backend>();
        #else
            LOG_INFO(LogType::MOTORLOG, "[L0] HHROS2_HAVE_ECAT_BACKEND no open ecat backend err! falling back to sim");
            // std::cerr << "[L0] HHROS2_HAVE_ECAT_BACKEND no open ecat backend err! '" << "', falling back to sim\n";
            return std::make_unique<hhros2_l0::SimL0Backend>();
        #endif

    }
    else if (name == "sim") {
        return std::make_unique<hhros2_l0::SimL0Backend>();
    }
    else {
        std::cerr << "[L0] Unknown backend '" << name << "', falling back to sim\n";
        return std::make_unique<hhros2_l0::SimL0Backend>();
    }
}

}  // namespace

int main(int argc, char ** argv)
{
    const Options opt = parse(argc, argv);

    Logger::getInstance()->initialize("run_logs", LogLevel::DEBUG);
    LOG_INFO(LogType::MOTORLOG, "L0 runtime starting (backend=%s, shm=%s)", 
             opt.backend.c_str(), opt.shm_name.c_str());
             
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    constexpr int kN = hhros2::shm::kJointCount;
    hhros2_l0::L0ShmServer server;
    if (!server.start(opt.shm_name))
    {
        LOG_INFO(LogType::MOTORLOG, "[L0] failed to create shared memory: %s",opt.shm_name.c_str());
        // std::cerr << "[L0] failed to create shared memory '" << opt.shm_name
        //           << "'\n";
        return 1;
    }

    auto backend = make_backend(opt.backend);
    if (!backend->init())
    {
        LOG_INFO(LogType::MOTORLOG, "[L0] backend init failed");
        // std::cerr << "[L0] backend init failed\n";
        return 1;
    }

    const double dt = 1.0 / opt.rate_hz;
    const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(dt));

    // std::cout << "[L0] runtime up: shm='" << opt.shm_name << "' backend='"
    //           << opt.backend << "' rate=" << opt.rate_hz << "Hz\n";
    LOG_INFO(LogType::MOTORLOG, "L0 runtime up shm =%s, backend=%s,rate=%dHz", 
             opt.backend.c_str(), opt.shm_name.c_str(),opt.rate_hz);

    std::vector<hhros2::shm::JointCommand> cmd(kN);
    std::vector<hhros2::shm::JointFeedback> fb(kN);
    hhros2::shm::ImuSample imu{};
    std::uint64_t cycle = 0;
    auto next = std::chrono::steady_clock::now();

    while (g_running.load())
    {
        const bool enabled = server.enabled();
        if (!server.fetch_command(cmd.data(), kN))
        {
            // No fresh command: command the safe passive state.
            for (auto & c : cmd) c = hhros2::shm::JointCommand{};
        }
        backend->apply(cmd.data(), kN, enabled);
        backend->step(dt);
        backend->sample(fb.data(), kN, imu);
        imu.stamp_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
        server.publish_feedback(fb.data(), kN, imu);
        server.set_cycle_counter(++cycle);

        next += period;
        std::this_thread::sleep_until(next);
        // LOG_INFO(LogType::MOTORLOG, "[L0] runtime runing cycles:%d",cycle);
    }

    std::cout << "[L0] runtime stopped after " << cycle << " cycles\n";
    backend->shutdown();
    server.stop();
    return 0;
}
