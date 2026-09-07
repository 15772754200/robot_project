#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ti5_encos
{

constexpr int kMotorNumber = 23;
constexpr int kMasterNumber = 3;

struct MasterRuntimeConfig
{
    int cpu_affinity = 1;
    int priority = 94;
    int interval = 0;
    std::int64_t cycle_time_ns = 1000000;
    std::int64_t shift_time_ns = 0;
    std::string eni_file;
    // 空白名单表示允许该 master 下扫描到的所有从站。
    std::vector<int> enabled_slave_positions;
    // 黑名单优先级高于白名单，用于临时屏蔽某些从站的 PDO 注册。
    std::vector<int> skipped_slave_positions;
    // 轴级禁用：从站仍参与 EtherCAT 通信，但指定电机不执行命令输出。
    std::vector<int> disabled_motor_indices;
};

/**
 * @brief Runtime configuration for the three EtherCAT masters.
 *
 * Values are consumed during initialize()/start().  ENI paths are resolved by
 * the runtime package and must point to readable EtherCAT network information
 * files.  Timing fields use nanoseconds except priority/affinity fields, which
 * map directly to Linux scheduling configuration.
 */
struct RuntimeConfig
{
    std::array<MasterRuntimeConfig, kMasterNumber> masters{};
    int ipc_server_cpu_affinity = 4;
    int ipc_server_priority = 60;
    int log_level = 6;
    bool lock_memory = true;
    bool initialize_qiuniu = true;
    bool start_ipc_server = false;   // 不使用ipc_server时设置为false
    bool record_tasks = false;

    RuntimeConfig();
};

/**
 * @brief Resolve the packaged default ENI file for one master.
 *
 * @param master_index Zero-based master index in [0, kMasterNumber).
 * @return Absolute or package-share path to the bundled ENI file.
 * @throws std::out_of_range or std::runtime_error when the master index/path is
 *         invalid, depending on the implementation path.
 */
std::string bundled_eni_file_for_master(int master_index);

/**
 * @brief Raw scaled command consumed by the hardware runtime.
 *
 * Fields are device/protocol integer units, not SI units.  ROS-facing unit
 * conversion must happen before constructing this command.
 */
struct MotorCommand
{
    double kp = 0;
    double kd = 0;
    double torque = 0;
    double position = 0;
    double velocity = 0;
};

/**
 * @brief Raw scaled motor state snapshot from one runtime axis.
 */
struct MotorState
{
    MotorCommand command;
    double position = 0;
    double velocity = 0;
    double current = 0;
};

/**
 * @brief Per-master diagnostic snapshot from the EtherCAT runtime.
 */
struct MasterDiag
{
    int32_t wc_state = -1;
    int32_t lost_frame_count = -1;
    int32_t lost_frame_delta = 0;
    int32_t slaves_responding = -1;
    int32_t master_al_state = -1;
    int32_t latency_flag = 0;
    int32_t latency_max_us = -1;
    int32_t latency_min_us = -1;
    int32_t latency_avg_us = -1;
    uint32_t cycle_counter = 0;
};

/**
 * @brief Full three-master diagnostic snapshot.
 */
struct RuntimeDiag
{
    uint32_t version = 0;
    uint32_t update_seq = 0;
    std::array<MasterDiag, kMasterNumber> masters{};
};

/**
 * @brief Complete command/feedback/diagnostic snapshot.
 *
 * Snapshot copies are safe to inspect after return and do not expose runtime
 * internal storage.
 */
struct RuntimeSnapshot
{
    std::array<std::array<MotorState, kMotorNumber>, kMasterNumber> motors{};
    RuntimeDiag diag;
};

/**
 * @brief 三主站 EtherCAT 电机运行时管理器。
 *
 * 该类拥有 EtherCAT master、IPC server 和硬件控制循环资源。它是硬件
 * runtime 边界，ROS bridge 只能通过 transport adapter 间接使用它。
 *
 * @thread_safety 生命周期函数应由一个 owner 线程串行调用。set_motor_enable()
 *                和 set_motor_command() 可在 runtime 启动后由 adapter 调用，
 *                具体同步由实现层保证。
 * @realtime initialize()/start()/wait()/release() 会访问文件、SDK、线程和日志，
 *           不能在实时控制路径调用。set_motor_command() 只接受已缩放命令，
 *           调用方仍需遵守 adapter 的实时性约束。
 * @failure initialize()/start() 失败时返回 false 并通过 error 输出可操作原因。
 *          request_stop()/wait()/release() 用于确保异常路径释放硬件资源。
 */
class ThreeMasterTi5EncosRuntime
{
public:
    explicit ThreeMasterTi5EncosRuntime(RuntimeConfig config = RuntimeConfig{});
    ~ThreeMasterTi5EncosRuntime();

    ThreeMasterTi5EncosRuntime(const ThreeMasterTi5EncosRuntime &) = delete;
    ThreeMasterTi5EncosRuntime &operator=(const ThreeMasterTi5EncosRuntime &) = delete;

    /**
     * @brief Initialize hardware resources without starting the control loop.
     */
    bool initialize(std::string *error = nullptr);

    /**
     * @brief Initialize if needed and start all configured masters.
     */
    bool start(std::string *error = nullptr);

    /**
     * @brief Request cooperative shutdown of runtime loops.
     */
    void request_stop();

    /**
     * @brief Wait for runtime loops to stop.
     */
    void wait();

    /**
     * @brief Release SDK, IPC and EtherCAT resources.
     */
    void release();

    bool initialized() const;
    bool started() const;

    void set_motor_enable(bool enable);
    void set_motor_command(int master_index, int motor_index, const MotorCommand &command);
    bool get_motor_state(int master_index, int motor_index, MotorState *state) const;
    RuntimeSnapshot snapshot() const;

    static void initialize_default_motor_commands();

private:
    struct Impl;

    void set_error(std::string *error, const std::string &message) const;

    RuntimeConfig config_;
    std::unique_ptr<Impl> impl_;    /* 这是pimpl写法，隐藏实现细节 */
    bool initialized_ = false;
    bool started_ = false;
    bool released_ = false;
};

} // namespace ti5_encos
