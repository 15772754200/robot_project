#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ti5_encos
{

/**
 * @brief Logical control mode encoded by bridge/runtime command conversion.
 *
 * PositionMode commands use position/velocity-oriented scaling.  Hybrid mode
 * carries the force/position command fields used by the ENCOS/TI5 bridge path.
 */
enum class MotorControlMode
{
    PositionMode,
    HybridForcePositionMode,
};

namespace ipc
{

constexpr int kIpcMotorPort = 1;
constexpr int kMotorCountPerMaster = 23;
constexpr int kMasterCount = 3;

/**
 * @brief One motor command/feedback record in the IPC wire packet.
 *
 * All fields are signed 32-bit integers in network byte order when transmitted
 * through the Linux realtime IPC device.  Unit scaling is intentionally not
 * encoded here; conversion modules in bridge/runtime own radian, rad/s, Nm and
 * gain scaling.
 * TODO: feeback is not sufficiently!!!
 */
struct MotorMessageSingle
{
    double kp_cmd = 0;
    double kd_cmd = 0;
    double Tor_cmd = 0;
    double position_cmd = 0;
    double velocity_cmd = 0;
    double position_actual = 0;
    double velocity_actual = 0;
    double current_actual = 0;
};

/**
 * @brief Fixed-size motor group for one EtherCAT master.
 */
struct MotorGroup
{
    MotorMessageSingle motor_id[kMotorCountPerMaster];
};

/**
 * @brief Per-master EtherCAT diagnostics sampled by the runtime IPC server.
 *
 * Counts and AL/WC states mirror the hardware runtime diagnostics.  Latency
 * fields are microseconds; unavailable values are represented by runtime-side
 * sentinel values before serialization.
 */
struct EthercatMasterDiag
{
    std::int32_t wc_state = 0;          // EtherCAT working counter state, wkc is a counter of how many slaves have responded to the master in a cycle, and is used to detect communication issues
    std::int32_t lost_frame_count = 0;  // Total number of lost frames since runtime start
    std::int32_t lost_frame_delta = 0;  // Number of lost frames since last IPC update
    std::int32_t slaves_responding = 0; // Number of slaves responding to the master (<= expected slave count)
    std::int32_t master_al_state = 0;   // al: application layer. master AL state, which is a state machine that indicates the overall communication status of the master.  Common states include "Not Ready to Operate", "Safe Operational", and "Operational".  Non-operational states indicate communication issues or misconfiguration.
    std::int32_t latency_flag = 0;      // Bitfield of latency flags; currently only bit 0 is used to indicate if latency values are valid (0 = invalid/unavailable, 1 = valid)
    std::int32_t latency_max_us = 0;    // Maximum observed EtherCAT cycle latency in microseconds since last IPC update; latency is the time from the start of the master cycle to the reception of the last expected slave response, and is used to detect timing issues in the communication loop
    std::int32_t latency_min_us = 0;    // Minimum observed EtherCAT cycle latency in microseconds since last IPC update
    std::int32_t latency_avg_us = 0;    // Average observed EtherCAT cycle latency in microseconds since last IPC update
    std::uint32_t cycle_counter = 0;    // Total number of master cycles since runtime start; incremented by the runtime IPC server on each update, and can be used to track the overall runtime duration and detect if the master is cycling as expected
};

/**
 * @brief Diagnostics header shared by all masters in one IPC packet.
 */
struct EthercatCommDiag
{
    std::uint32_t version = 0;
    std::uint32_t update_seq = 0;
    EthercatMasterDiag master_diag[kMasterCount];   // per master diagnostics; master_diag[0] corresponds to master_id[0] in the MotorPacket, and so on
};

/**
 * @brief Complete IPC wire packet exchanged between bridge and runtime.
 *
 * The layout is part of the ABI between processes.  Any field change must be
 * accompanied by codec tests and compatibility planning.
 */
struct MotorPacket
{
    MotorGroup master_id[kMasterCount];
    EthercatCommDiag ec_diag;
};

static_assert(std::is_standard_layout<MotorMessageSingle>::value,
              "IPC motor message must keep a C-compatible layout");
static_assert(std::is_standard_layout<MotorPacket>::value,
              "IPC motor packet must keep a C-compatible layout");
static_assert(sizeof(MotorMessageSingle) == sizeof(double) * 8,
              "IPC motor message wire size changed unexpectedly");
static_assert(sizeof(EthercatMasterDiag) == sizeof(std::int32_t) * 9 +
                                             sizeof(std::uint32_t),
              "EtherCAT master diagnostic wire size changed unexpectedly");
static_assert(sizeof(MotorPacket) ==
                  sizeof(MotorGroup) * kMasterCount +
                      sizeof(EthercatCommDiag),
              "IPC motor packet wire size changed unexpectedly");

} // namespace ipc
} // namespace ti5_encos

using Motor_msg_single = ti5_encos::ipc::MotorMessageSingle;
using Motor_id = ti5_encos::ipc::MotorGroup;
using Ethercat_master_diag = ti5_encos::ipc::EthercatMasterDiag;
using Ethercat_comm_diag = ti5_encos::ipc::EthercatCommDiag;
using Motor_master = ti5_encos::ipc::MotorPacket;

constexpr int motor_number = ti5_encos::ipc::kMotorCountPerMaster;
constexpr int master_number = ti5_encos::ipc::kMasterCount;
constexpr int EXIPC_PORT_1 = ti5_encos::ipc::kIpcMotorPort;
constexpr std::size_t BUF_SIZE = sizeof(Motor_master);
