#pragma once

/**
 * @file hhros2_shm_layout.hpp
 * @brief Shared-memory ABI between the L0 real-time runtime (writer of feedback,
 *        reader of command) and the ros2_control HAL (writer of command, reader
 *        of feedback).
 *
 * This is the hard real-time boundary of the "double-loop" architecture. The
 * segment is mapped by BOTH:
 *   - hhros2_motor_runtime (L0, 1-4 kHz, NO ROS headers)
 *   - hhros2_hal::ExipcSystemHardware (ros2_control SystemInterface, 500 Hz-1 kHz)
 *
 * Each direction is a single-producer / single-consumer seqlock so neither side
 * blocks the other. Array indices follow hhros2_description/config/
 * joint_order.yaml and therefore represent logical joints in SI units (rad,
 * rad/s, Nm). Physical motor-slot routing, unit scaling, and parallel-ankle
 * conversion are owned exclusively by the L0 runtime.
 *
 * ABI rules: append-only. Never reorder or resize existing fields without
 * bumping kShmAbiVersion and rebuilding both L0 and the HAL together.
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hhros2
{
namespace shm
{

constexpr std::uint32_t kShmAbiVersion = 1;
constexpr int kJointCount = 23;
constexpr char kDefaultShmName[] = "hhros2_motor_shm";

/// Hybrid impedance command for one joint (HAL -> L0).
struct JointCommand
{
    double position = 0.0;  // desired angle [rad]
    double velocity = 0.0;  // desired rate  [rad/s]
    double effort = 0.0;    // feed-forward torque [Nm]
    double kp = 0.0;        // stiffness gain
    double kd = 0.0;        // damping gain
};

/// Measured joint state (L0 -> HAL).
struct JointFeedback
{
    double position = 0.0;     // [rad]
    double velocity = 0.0;     // [rad/s]
    double effort = 0.0;       // measured torque [Nm]
    double temperature = 0.0;  // [degC]
    std::uint32_t error_flags = 0;
};

/// Raw IMU sample read by the L0 runtime (L0 -> HAL), orientation as xyzw.
struct ImuSample
{
    double orientation[4] = {0.0, 0.0, 0.0, 1.0};
    double angular_velocity[3] = {0.0, 0.0, 0.0};
    double linear_acceleration[3] = {0.0, 0.0, 0.0};
    std::uint64_t stamp_ns = 0;
};

/**
 * @brief Full shared-memory segment.
 *
 * seqlock convention: a writer increments its *_seq to an odd value before
 * mutating its region and to the next even value afterwards. A reader retries
 * while the seq is odd or changed across the read.
 */
struct MotorShmSegment
{
    std::uint32_t abi_version = kShmAbiVersion;
    std::uint32_t joint_count = kJointCount;

    // ---- command region: HAL writes, L0 reads ----
    std::atomic<std::uint32_t> command_seq{0};
    JointCommand command[kJointCount];

    // ---- enable / safety: HAL or hhros2_core writes, L0 reads ----
    // 0 = motors released (passive), 1 = torque enabled.
    std::atomic<std::uint32_t> motor_enable{0};

    // ---- feedback region: L0 writes, HAL reads ----
    std::atomic<std::uint32_t> feedback_seq{0};
    JointFeedback feedback[kJointCount];
    ImuSample imu;

    // ---- L0 liveness / fault (L0 writes, HAL + hhros2_core read) ----
    std::atomic<std::uint64_t> l0_cycle_counter{0};
    std::atomic<std::uint32_t> l0_fault_code{0};  // 0 = ok
};

static_assert(std::is_standard_layout<JointCommand>::value,
              "JointCommand must be standard-layout for shared memory");
static_assert(std::is_standard_layout<JointFeedback>::value,
              "JointFeedback must be standard-layout for shared memory");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "Cross-process seqlock requires lock-free 32-bit atomics");

constexpr std::size_t kShmSize = sizeof(MotorShmSegment);

}  // namespace shm
}  // namespace hhros2
