#include "internal/ipc_motor_server.hpp"
#include "internal/ipc_motor_diag_sampler.hpp"
#include "internal/ipc_motor_message_codec.hpp"

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "hhros2_log/log.h"
#include <sched.h>
#include <unistd.h>

Motor_master motor_msg;    // 作为共享状态储存电机命令和反馈数据

namespace
{
    std::atomic<bool> g_server_running{false};
    std::atomic<int>  g_server_socket{-1};
    std::atomic<int>  g_server_cpu_affinity{4};
    std::atomic<int>  g_server_priority{60};    // server线程的实时优先级，默认60，范围0-99，值越大优先级越高
    constexpr std::size_t kIpcSocketPoolPacketDepth = 3U;

    int clamp_realtime_priority(int priority)
    {
        return std::clamp(priority, 0, 99);
    }

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
            LOG_WARNING(LogType::MOTORLOG, "读取IPC server CPU affinity失败: %s", std::strerror(errno));
            return false;
        }
        return CPU_ISSET(cpu, &allowed_cpus);
    }

    void stop_after_error(int socket_fd, const char *reason)
    {
        LOG_ERROR(LogType::MOTORLOG, "%s: %s", reason, std::strerror(errno));
        if (socket_fd >= 0)
        {
            close(socket_fd);
        }
        g_server_socket.store(-1, std::memory_order_release);
        g_server_running.store(false, std::memory_order_release);
    }
} // namespace

void *server(void *)    // 消费者，正常来说应该它创建，线程入口函数
{
    struct sockaddr_ipc saddr, claddr;  // saddr 这个是服务器自己的地址，也就是说本server要绑定在哪个ipc端口上
    socklen_t addrlen = sizeof(claddr);
    Motor_master net_recv_msg,     host_recv_cmd;   // net_recv_msg: 网络/ipc接收到的原始数据，host_recv_cmd: 解码之后的主机端收到的命令
    Motor_master host_send_actual, net_send_msg ;   // host_send_actual: 主机端要发送的实际值，也就是反馈给bridge的，net_send_msg: 编码之后准备通过网络/ipc发送的反馈数据

    size_t poolsz;
    int ret, sfd = -1;
    sfd = __RT(socket(AF_RTIPC, SOCK_DGRAM, IPCPROTO_EXIPC));   // 用的是udp的进程间通信，注意这个进程是实时进程与非实时进程间通信
    g_server_socket.store(sfd, std::memory_order_release);
    if (sfd < 0)
    {
        stop_after_error(sfd, "创建IPC socket失败");
        return NULL;
    }

    // EXIPC_POOLSZ 是 socket 侧可暂存的字节池。client/server 都按 1kHz
    // request-response 节奏运行，正常只应有 1 个命令在途；保留 3 个完整
    // Motor_master 包容量用于吸收一次调度抖动，但避免 10 帧旧命令积压。
    poolsz = static_cast<std::size_t>(BUF_SIZE) * kIpcSocketPoolPacketDepth;
    ret = __RT(setsockopt(sfd, SOL_EXIPC, EXIPC_POOLSZ, &poolsz, sizeof(poolsz)));
    if (ret < 0)
    {
        stop_after_error(sfd, "设置接收缓冲区失败");
        return NULL;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sipc_family = AF_RTIPC;
    saddr.sipc_port = EXIPC_PORT_1;
    ret = __RT(bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)));
    if (ret < 0)
    {
        stop_after_error(sfd, "端口绑定失败");
        return NULL;
    }

    LOG_INFO(LogType::MOTORLOG, "[Server] 启动成功，端口=%d，消息大小=%zu字节，IPC缓冲池=%zu包/%zu字节",
             EXIPC_PORT_1,
             BUF_SIZE,
             kIpcSocketPoolPacketDepth,
             poolsz);

    while (g_server_running.load(std::memory_order_acquire))
    {
        ipc_motor_diag_update_server_sample();

        // 接收命令并发送反馈的流程：
        // 1. recvfrom() 从IPC套接字接收数据到 net_recv_msg 中，数据大小应该是 BUF_SIZE 字节，来源地址存储在 claddr 中，实际接收的字节数存储在 ret 中。
        // 2. 如果成功接收了完整的 BUF_SIZE 字节，解码接收到的命令到 host_recv_cmd 中，更新共享状态并准备反馈数据到 host
        ret = __RT(recvfrom(sfd, &net_recv_msg, BUF_SIZE, 0,
                            (struct sockaddr *)&claddr, &addrlen));

        if (ret == BUF_SIZE)
        {   // 网络序接收的命令解码到主机端命令结构中，更新共享状态并准备反馈数据
            decode_motor_command_message(net_recv_msg, &host_recv_cmd);
            {
                std::lock_guard<std::mutex> lock(motor_shared_state_mutex());
                // 将主机端接收到的命令应用到共享状态中，这样运行时线程就可以看到最新的命令了
                apply_motor_command_to_shared_state(host_recv_cmd, &motor_msg);
                // 这个motor_msg是共享状态中的命令和实际值的结构体，运行时线程会更新其中的实际值字段，这里我们拷贝一份到 host_send_actual 中准备编码成网络序发送回bridge，反馈内容包括实际值和诊断信息
                // 拷贝一份到 host_send_actual 中准备编码成网络序发送回bridge，反馈内容包括实际值和诊断信息
                copy_motor_feedback_snapshot(motor_msg,
                                             ipc_motor_diag_snapshot(),
                                             &host_send_actual);
            }
            encode_motor_feedback_message(host_send_actual, &net_send_msg);

            ret = __RT(sendto(sfd, &net_send_msg, BUF_SIZE, 0,
                              (struct sockaddr *)&claddr, addrlen));

            if (ret != BUF_SIZE)
            {
                LOG_WARNING(LogType::MOTORLOG, "[Server] 实际值反馈失败：实际发送%d字节", ret);
            }
        }
        else if (ret > 0)
        {
            LOG_WARNING(LogType::MOTORLOG,
                        "[Server] 指令接收不完整：实际接收%d字节 / 需接收%zu字节",
                        ret,
                        BUF_SIZE);
        }
        else
        {
            if (!g_server_running.load(std::memory_order_acquire))
            {
                break;
            }
            LOG_ERROR(LogType::MOTORLOG, "[Server] 指令接收失败: %s", std::strerror(errno));
            stop_after_error(sfd, "recvfrom错误");
            break;
        }

        // IPC client 使用绝对 1ms deadline 控制发送节拍。
        // server 这里保持请求驱动，立即回到 recvfrom() 等待下一帧，
        // 避免“处理耗时 + 1ms sleep”把平均频率拖到 1kHz 以下。
    }

    const int fd_to_close = g_server_socket.exchange(-1, std::memory_order_acq_rel);
    if (fd_to_close >= 0)
    {
        close(fd_to_close);
    }
    return NULL;
}

void ipc_motor_server_register_tasks(ecat::task *master0, ecat::task *master1, ecat::task *master2)
{
    ipc_motor_diag_register_tasks(master0, master1, master2);
}

void update_master_cycle_status(int master_index, int wc_state)
{
    ipc_motor_diag_update_cycle_status(master_index, wc_state);
}

void ipc_motor_server_sample_diag(Ethercat_comm_diag *diag)
{
    ipc_motor_diag_sample_direct(diag);
}

void configure_ipc_motor_server(int cpu_affinity, int priority)
{
    g_server_cpu_affinity.store(cpu_affinity, std::memory_order_release);
    g_server_priority.store(
        clamp_realtime_priority(priority),
        std::memory_order_release);
}

// 线程配置（保持原实时属性）
void start_ipc_motor_server()
{
    bool expected = false;
    if (!g_server_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    pthread_t p2;
    pthread_attr_t ap2;
    struct sched_param sp2;
    cpu_set_t cp2;

    memset(&ap2, 0, sizeof(pthread_attr_t));
    memset(&sp2, 0, sizeof(struct sched_param));
    memset(&cp2, 0, sizeof(cpu_set_t));

    pthread_attr_init(&ap2);
    pthread_attr_setinheritsched(&ap2, PTHREAD_EXPLICIT_SCHED);
    const int priority =
        g_server_priority.load(std::memory_order_acquire);
    if (priority > 0)
    {
        pthread_attr_setschedpolicy(&ap2, SCHED_FIFO);
        sp2.sched_priority = priority;
        pthread_attr_setschedparam(&ap2, &sp2);
    }
    CPU_ZERO(&cp2);
    const int cpu_affinity =
        g_server_cpu_affinity.load(std::memory_order_acquire);
    if (cpu_allowed(cpu_affinity))
    {
        CPU_SET(cpu_affinity, &cp2);
        const int affinity_ret =
            pthread_attr_setaffinity_np(&ap2, sizeof(cp2), &cp2);
        if (affinity_ret != 0)
        {
            LOG_WARNING(LogType::MOTORLOG,
                        "设置IPC server CPU affinity=%d失败: %s",
                        cpu_affinity,
                        std::strerror(affinity_ret));
        }
    }
    else if (cpu_affinity >= 0)
    {
        LOG_WARNING(LogType::MOTORLOG,
                    "跳过IPC server CPU affinity=%d，该CPU不在当前进程允许集合内",
                    cpu_affinity);
    }

    const int create_ret = __RT(pthread_create(&p2, &ap2, server, NULL));
    pthread_attr_destroy(&ap2);
    if (create_ret != 0)
    {
        LOG_ERROR(LogType::MOTORLOG, "启动IPC server线程失败: %s", std::strerror(create_ret));
        g_server_running.store(false, std::memory_order_release);
        return;
    }

    const int name_ret = __RT(pthread_setname_np(p2, "motor-server-actual"));
    if (name_ret != 0)
    {
        LOG_WARNING(LogType::MOTORLOG, "设置IPC server线程名失败: %s", std::strerror(name_ret));
    }

    // __RT(pthread_join(p2, NULL));
    pthread_detach(p2); // 替代 pthread_join()，主线程不阻塞
}

void stop_ipc_motor_server()
{
    g_server_running.store(false, std::memory_order_release);
    const int sfd = g_server_socket.exchange(-1, std::memory_order_acq_rel);
    if (sfd >= 0)
    {
        close(sfd);
    }
}
