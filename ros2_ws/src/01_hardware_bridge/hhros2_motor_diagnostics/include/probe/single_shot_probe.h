#ifndef SINGLE_SHOT_PROBE_H
#define SINGLE_SHOT_PROBE_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef ROBOT_MOTOR_ENABLE_SINGLE_SHOT_PROBE
#define ROBOT_MOTOR_ENABLE_SINGLE_SHOT_PROBE 1
#endif

namespace single_shot_probe
{
constexpr const char *kSharedMemoryName = "/sb_single_shot_probe_v1";
constexpr std::uint32_t kMagic = 0x53425031U;
constexpr std::uint32_t kVersion = 3U;

struct TraceData
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t armed;
    std::uint32_t summary_printed;
    std::int32_t target_ros_joint_index;
    std::int32_t target_master_index;
    std::int32_t target_motor_index;
    std::int32_t reserved;
    double command_position_rad;
    std::uint64_t topic_pub_ns;
    std::uint64_t motor_node_rx_ns;
    std::uint64_t ec_send_ns;
    std::uint64_t ec_receive_ns;
    std::uint64_t ec_feedback_change_ns;
    std::uint64_t ipc_feedback_rx_ns;
    std::uint64_t state_pub_ns;
    std::uint64_t state_seen_ns;
    std::int64_t state_pub_header_ns;
    std::int64_t ec_send_position_actual;
    std::int64_t ec_send_velocity_actual;
    std::int64_t ec_send_current_actual;
    std::int64_t ec_feedback_position_actual;
    std::int64_t ec_feedback_velocity_actual;
    std::int64_t ec_feedback_current_actual;
    double state_pub_position;
    double state_pub_velocity;
    double state_seen_position;
    double state_seen_velocity;

    std::uint64_t hal_write_shm_ns;          // HAL 写入共享内存时刻
    std::uint64_t l0_read_shm_ns;            // L0 从共享内存读取命令时刻
    std::uint64_t adapter_write_pdo_ns;      // Ti5适配器写入PDO寄存器时刻
};

inline std::uint64_t now_ns()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0)
    {
        return 0;
    }
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

inline TraceData *map_trace()
{
#if !ROBOT_MOTOR_ENABLE_SINGLE_SHOT_PROBE
    return nullptr;
#else
    static TraceData *trace = nullptr;
    if (trace != nullptr)
    {
        return trace;
    }
    // 创建或打开共享内存对象，设置大小，并映射到当前进程地址空间
    const int fd = shm_open(kSharedMemoryName, O_CREAT | O_RDWR, 0666);
    if (fd < 0)
    {
        perror("[Probe] shm_open failed");
        return nullptr;
    }

    if (ftruncate(fd, static_cast<off_t>(sizeof(TraceData))) != 0)
    {
        perror("[Probe] ftruncate failed");
        close(fd);
        return nullptr;
    }
    // 建立虚拟内存到某一对象的映射
    void *mapped = mmap(nullptr, sizeof(TraceData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED)
    {
        perror("[Probe] mmap failed");
        return nullptr;
    }
    // 类型转换为 TraceData 指针，并检查魔数和版本号，如果不匹配就清零并初始化
    trace = static_cast<TraceData *>(mapped);
    if (trace->magic != kMagic || trace->version != kVersion)
    {
        std::memset(trace, 0, sizeof(TraceData));
        trace->magic = kMagic;
        trace->version = kVersion;
    }
    return trace;
#endif
}

inline void arm_trace(
    TraceData *trace,
    int target_ros_joint_index,
    int target_master_index,
    int target_motor_index,
    double command_position_rad)
{
    if (trace == nullptr)
    {
        return;
    }
    std::memset(trace, 0, sizeof(TraceData));
    trace->magic = kMagic;
    trace->version = kVersion;
    trace->armed = 1U;
    trace->target_ros_joint_index = target_ros_joint_index;
    trace->target_master_index = target_master_index;
    trace->target_motor_index = target_motor_index;
    trace->command_position_rad = command_position_rad;
}

inline bool ready(const TraceData *trace)
{
    return trace != nullptr && trace->armed != 0U;
}

inline double delta_us(std::uint64_t start_ns, std::uint64_t end_ns)
{
    if (start_ns == 0U || end_ns == 0U || end_ns < start_ns)
    {
        return -1.0;
    }
    return static_cast<double>(end_ns - start_ns) / 1000.0;
}

inline void print_raw_timestamps(const TraceData *trace)
{
    if (trace == nullptr)
    {
        return;
    }

    std::printf("[Probe][RawTimestamp] target_ros_joint_index=%d "
                "target_master_index=%d target_motor_index=%d "
                "command_position_rad=%.9f\n",
                trace->target_ros_joint_index,
                trace->target_master_index,
                trace->target_motor_index,
                trace->command_position_rad);
    std::printf("[Probe][RawTimestamp] topic_pub_ns=%llu\n",
                static_cast<unsigned long long>(trace->topic_pub_ns));
    std::printf("[Probe][RawTimestamp] motor_node_rx_ns=%llu\n",
                static_cast<unsigned long long>(trace->motor_node_rx_ns));
    std::printf("[Probe][RawTimestamp] ec_send_ns=%llu\n",
                static_cast<unsigned long long>(trace->ec_send_ns));
    std::printf("[Probe][RawTimestamp] ec_receive_ns=%llu\n",
                static_cast<unsigned long long>(trace->ec_receive_ns));
    std::printf("[Probe][RawTimestamp] ec_feedback_change_ns=%llu\n",
                static_cast<unsigned long long>(
                    trace->ec_feedback_change_ns));
    std::printf("[Probe][RawTimestamp] ipc_feedback_rx_ns=%llu\n",
                static_cast<unsigned long long>(trace->ipc_feedback_rx_ns));
    std::printf("[Probe][RawTimestamp] state_pub_ns=%llu\n",
                static_cast<unsigned long long>(trace->state_pub_ns));
    std::printf("[Probe][RawTimestamp] state_pub_header_ns=%lld\n",
                static_cast<long long>(trace->state_pub_header_ns));
    std::printf("[Probe][RawTimestamp] state_seen_ns=%llu\n",
                static_cast<unsigned long long>(trace->state_seen_ns));
}

inline void print_summary_if_ready(TraceData *trace)
{
    if (trace == nullptr || trace->summary_printed != 0U)
    {
        return;
    }
    if (trace->topic_pub_ns == 0U || trace->motor_node_rx_ns == 0U ||
        trace->ec_send_ns == 0U || trace->ec_receive_ns == 0U ||
        trace->ipc_feedback_rx_ns == 0U ||
        trace->state_pub_ns == 0U || trace->state_seen_ns == 0U)
    {
        return;
    }

    print_raw_timestamps(trace);
    std::printf("[Probe][Summary] topic_pub_ns=%llu motor_node_rx_ns=%llu ec_send_ns=%llu "
                "ec_receive_ns=%llu ec_feedback_change_ns=%llu ipc_feedback_rx_ns=%llu "
                "state_pub_ns=%llu state_seen_ns=%llu\n",
                static_cast<unsigned long long>(trace->topic_pub_ns),
                static_cast<unsigned long long>(trace->motor_node_rx_ns),
                static_cast<unsigned long long>(trace->ec_send_ns),
                static_cast<unsigned long long>(trace->ec_receive_ns),
                static_cast<unsigned long long>(trace->ec_feedback_change_ns),
                static_cast<unsigned long long>(trace->ipc_feedback_rx_ns),
                static_cast<unsigned long long>(trace->state_pub_ns),
                static_cast<unsigned long long>(trace->state_seen_ns));


    std::printf("[Probe][Summary] /joint_command发布 -> motor_node订阅 = %.3f us\n",
                delta_us(trace->topic_pub_ns, trace->motor_node_rx_ns));
    std::printf("[Probe][Summary] motor_node订阅 -> EtherCAT send_callback = %.3f us\n",
                delta_us(trace->motor_node_rx_ns, trace->ec_send_ns));
    std::printf("[Probe][Summary] /joint_command发布 -> EtherCAT send_callback = %.3f us\n",
                delta_us(trace->topic_pub_ns, trace->ec_send_ns));
    std::printf("[Probe][Summary] EtherCAT send_callback -> receive_callback = %.3f us\n",
                delta_us(trace->ec_send_ns, trace->ec_receive_ns));
    std::printf("[Probe][Summary] EtherCAT send_callback -> 底层反馈首次变化 = %.3f us\n",
                delta_us(trace->ec_send_ns, trace->ec_feedback_change_ns));
    std::printf("[Probe][Summary] receive_callback -> IPC客户端收到回包 = %.3f us\n",
                delta_us(trace->ec_receive_ns, trace->ipc_feedback_rx_ns));
    std::printf("[Probe][Summary] IPC客户端收到回包 -> /joint_states发布 = %.3f us\n",
                delta_us(trace->ipc_feedback_rx_ns, trace->state_pub_ns));
    std::printf("[Probe][Summary] /joint_states发布 -> 测试订阅看到 = %.3f us\n",
                delta_us(trace->state_pub_ns, trace->state_seen_ns));
    std::printf("[Probe][Summary] /joint_command发布 -> 测试订阅看到 = %.3f us\n",
                delta_us(trace->topic_pub_ns, trace->state_seen_ns));

    // 新增延时：控制器 → HAL → L0 → 适配器
    std::printf("[Probe][Summary] controller_rx -> hal_write_shm = %.3f us\n",
                delta_us(trace->motor_node_rx_ns, trace->hal_write_shm_ns));
    std::printf("[Probe][Summary] hal_write_shm -> l0_read_shm = %.3f us\n",
                delta_us(trace->hal_write_shm_ns, trace->l0_read_shm_ns));
    std::printf("[Probe][Summary] l0_read_shm -> adapter_write_pdo = %.3f us\n",
                delta_us(trace->l0_read_shm_ns, trace->adapter_write_pdo_ns));
    std::printf("[Probe][Summary] adapter_write_pdo -> ec_send = %.3f us\n",
                delta_us(trace->adapter_write_pdo_ns, trace->ec_send_ns));

    std::printf("[Probe][Summary] target_joint=%d state_seen_pos=%.6f\n",
                trace->target_ros_joint_index,
                trace->state_seen_position);
    std::printf("[Probe][Summary] bottom_feedback_change position_actual=%lld\n",
                static_cast<long long>(trace->ec_feedback_position_actual));

    trace->summary_printed = 1U;
    trace->armed = 0U;
}
} // namespace single_shot_probe

#endif
