#include "internal/ipc_motor_diag_sampler.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include "hhros2_log/log.h"

namespace
{
using SteadyClock = std::chrono::steady_clock;

constexpr uint32_t kDiagVersion = 1;
constexpr auto kDiagSamplePeriod = std::chrono::milliseconds(100);

ecat::task *g_registered_tasks[master_number] = {nullptr, nullptr, nullptr};
std::atomic<int32_t> g_cycle_wc_state[master_number];
std::atomic<uint32_t> g_cycle_counter[master_number];
Ethercat_comm_diag g_cached_diag{};
int32_t g_last_logged_wc_state[master_number] = {-2, -2, -2};
int32_t g_server_prev_lostframe[master_number] = {-1, -1, -1};
int32_t g_direct_prev_lostframe[master_number] = {-1, -1, -1};
SteadyClock::time_point g_server_last_diag_sample{};
SteadyClock::time_point g_direct_last_diag_sample{};

const char *wc_state_to_string(int32_t wc_state)
{
    switch (wc_state)
    {
    case static_cast<int32_t>(ecat::wc_state_type::zero):
        return "zero";
    case static_cast<int32_t>(ecat::wc_state_type::incomplete):
        return "incomplete";
    case static_cast<int32_t>(ecat::wc_state_type::complete):
        return "complete";
    default:
        return "unknown";
    }
}

void init_cached_diag()
{
    std::memset(&g_cached_diag, 0, sizeof(g_cached_diag));
    g_cached_diag.version = kDiagVersion;
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        auto &master_diag = g_cached_diag.master_diag[master_idx];

        master_diag.wc_state = -1;
        master_diag.lost_frame_count = -1;
        master_diag.lost_frame_delta = 0;
        master_diag.slaves_responding = -1;
        master_diag.master_al_state = -1;
        master_diag.latency_flag = 0;
        master_diag.latency_max_us = -1;
        master_diag.latency_min_us = -1;
        master_diag.latency_avg_us = -1;
        master_diag.cycle_counter = 0;
    }
}

void update_fast_diag_fields(Ethercat_comm_diag *diag)
{
    diag->version = kDiagVersion;
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        auto &master_diag = diag->master_diag[master_idx];

        master_diag.wc_state =
            g_cycle_wc_state[master_idx].load(std::memory_order_relaxed);
        master_diag.cycle_counter =
            g_cycle_counter[master_idx].load(std::memory_order_relaxed);
    }
}

void sample_slow_diag_fields(
    Ethercat_comm_diag *diag,
    int32_t prev_lostframe[master_number])
{
    ++diag->update_seq;
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        auto &master_diag = diag->master_diag[master_idx];  // 每个主站一个索引
        auto *task = g_registered_tasks[master_idx];
        if (task == nullptr)
        {
            master_diag.lost_frame_count = -1;
            master_diag.lost_frame_delta = 0;
            master_diag.slaves_responding = -1;
            master_diag.master_al_state = -1;
            master_diag.latency_flag = 0;
            master_diag.latency_max_us = -1;
            master_diag.latency_min_us = -1;
            master_diag.latency_avg_us = -1;
            continue;
        }

        int lostframe_count = 0;
        int slaves_responding = 0;
        uint8_t master_al_state = 0;
        ecat::RunTimeData latency{};

        ecat::get_lostframe_count(task, lostframe_count);   // 得到相关主站的丢帧数
        ecat::get_slaves_responding(task, slaves_responding);    // 得到相关主站的响应从站数
        ecat::get_Master_State(task, master_al_state);      // 得到主站相关状态
        ecat::get_latency_in_task(task, &latency);              // 得到相关主站的通信延迟情况

        master_diag.lost_frame_count = lostframe_count;
        if (prev_lostframe[master_idx] < 0)
        {
            master_diag.lost_frame_delta = 0;
        }
        else
        {
            master_diag.lost_frame_delta =
                lostframe_count - prev_lostframe[master_idx];
        }
        if (master_diag.lost_frame_delta < 0)
        {
            master_diag.lost_frame_delta = 0;
        }
        prev_lostframe[master_idx] = lostframe_count;

        master_diag.slaves_responding = slaves_responding;
        master_diag.master_al_state = static_cast<int32_t>(master_al_state);
        master_diag.latency_flag = static_cast<int32_t>(latency.flag);
        master_diag.latency_max_us = latency.maxDelay;
        master_diag.latency_min_us = latency.minDelay;
        master_diag.latency_avg_us = latency.avgDelay;
    }
    update_fast_diag_fields(diag);
}

void warn_if_diag_abnormal(const Ethercat_comm_diag &diag)
{
    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        const auto &master_diag = diag.master_diag[master_idx];
        if (master_diag.wc_state == g_last_logged_wc_state[master_idx])
        {
            continue;
        }

        if (master_diag.wc_state < 0)
        {
            g_last_logged_wc_state[master_idx] = master_diag.wc_state;
        }
        else if (master_diag.wc_state !=
                 static_cast<int32_t>(ecat::wc_state_type::complete))
         {
            LOG_WARNING(LogType::MOTORLOG,
                        "[Server][EtherCATDiag] master%d WKC异常，当前状态=%s",
                        master_idx,
                        wc_state_to_string(master_diag.wc_state));
        }
        else if (g_last_logged_wc_state[master_idx] != -2)
        {
            LOG_INFO(LogType::MOTORLOG,
                     "[Server][EtherCATDiag] master%d WKC恢复正常，当前状态=%s",
                     master_idx,
                     wc_state_to_string(master_diag.wc_state));
        }
        g_last_logged_wc_state[master_idx] = master_diag.wc_state;
    }
}

void maybe_sample_diag(
    SteadyClock::time_point *last_sample,
    int32_t prev_lostframe[master_number])
{
    const auto now = SteadyClock::now();
    if (*last_sample == SteadyClock::time_point{} ||    // 慢速诊断每哥一段时间采样一次，快速诊断每次调用都采样，理解为控制或者还没有初始化过
        now - *last_sample >= kDiagSamplePeriod)        // last_sample为默认值时，这是上一次慢速采样的时间点，或者距离上一次慢速采样已经过了预定的周期，其实就是美国一个时间段需要慢速采样
    {
        sample_slow_diag_fields(&g_cached_diag, prev_lostframe);
        warn_if_diag_abnormal(g_cached_diag);
        *last_sample = now;
        return;
    }

    update_fast_diag_fields(&g_cached_diag);
}
} // namespace

void ipc_motor_diag_register_tasks(
    ecat::task *master0,
    ecat::task *master1,
    ecat::task *master2)
{
    g_registered_tasks[0] = master0;
    g_registered_tasks[1] = master1;
    g_registered_tasks[2] = master2;

    for (int master_idx = 0; master_idx < master_number; ++master_idx)
    {
        g_cycle_wc_state[master_idx].store(-1, std::memory_order_relaxed);
        g_cycle_counter[master_idx].store(0, std::memory_order_relaxed);
        g_last_logged_wc_state[master_idx] = -2;
        g_server_prev_lostframe[master_idx] = -1;
        g_direct_prev_lostframe[master_idx] = -1;
    }

    g_server_last_diag_sample = SteadyClock::time_point{};
    g_direct_last_diag_sample = SteadyClock::time_point{};
    init_cached_diag();
}

void ipc_motor_diag_update_cycle_status(int master_index, int wc_state)
{
    if (master_index < 0 || master_index >= master_number)
    {
        return;
    }

    g_cycle_wc_state[master_index].store(wc_state, std::memory_order_relaxed);
    g_cycle_counter[master_index].fetch_add(1, std::memory_order_relaxed);
}

void ipc_motor_diag_update_server_sample()
{
    maybe_sample_diag(&g_server_last_diag_sample, g_server_prev_lostframe);
}

void ipc_motor_diag_sample_direct(Ethercat_comm_diag *diag)
{
    if (diag == nullptr)
    {
        return;
    }

    maybe_sample_diag(&g_direct_last_diag_sample, g_direct_prev_lostframe);
    *diag = g_cached_diag;
}

Ethercat_comm_diag ipc_motor_diag_snapshot()
{
    return g_cached_diag;
}
