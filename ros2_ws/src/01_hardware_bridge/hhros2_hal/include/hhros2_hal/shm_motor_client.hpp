#pragma once

/**
 * @file shm_motor_client.hpp
 * @brief POSIX shared-memory client used by the ros2_control HAL to exchange
 *        commands and feedback with the L0 real-time runtime.
 *
 * The client maps the hhros2::shm::MotorShmSegment ABI defined in
 * hhros2_motor_protocol. It is a pure data-plane object with no ROS dependency,
 * so it can be unit-tested and reused by hhros2_core for the safety enable line.
 */

#include <cstdint>
#include <string>

#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"

namespace hhros2_hal
{

class ShmMotorClient
{
public:
    ShmMotorClient() = default;
    ~ShmMotorClient();

    ShmMotorClient(const ShmMotorClient &) = delete;
    ShmMotorClient & operator=(const ShmMotorClient &) = delete;

    /**
     * @brief Map the shared segment.
     * @param name  POSIX shm name (e.g. "hhros2_motor_shm").
     * @param create  Create + size the segment if missing (true for the writer
     *                that comes up first; HAL normally attaches with false).
     * @return true on success.
     */
    bool open(const std::string & name, bool create = false);
    void close();
    bool is_open() const { return segment_ != nullptr; }

    /**
     * @brief Consistent (seqlock) read of the feedback + IMU regions.
     * @return true if a torn-free snapshot was obtained.
     */
    bool read_feedback(
        hhros2::shm::JointFeedback * joints,
        int count,
        hhros2::shm::ImuSample * imu);

    /// Seqlock publish of the command region (HAL -> L0).
    void write_command(const hhros2::shm::JointCommand * joints, int count);

    /// Torque enable line (typically driven by hhros2_core, exposed for reuse).
    void set_enable(bool enable);
    bool enabled() const;

    std::uint64_t l0_cycle_counter() const;
    std::uint32_t l0_fault_code() const;

private:
    int fd_ = -1;
    std::string name_;
    bool created_ = false;
    hhros2::shm::MotorShmSegment * segment_ = nullptr;
};

}  // namespace hhros2_hal
