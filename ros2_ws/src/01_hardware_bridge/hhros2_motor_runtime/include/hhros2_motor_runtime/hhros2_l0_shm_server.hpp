#pragma once

/**
 * @file hhros2_l0_shm_server.hpp
 * @brief Writer side of the hhros2 shared-memory ABI, owned by the L0 runtime.
 *
 * IMPORTANT: this header (and its .cpp) intentionally pull in NO ROS 2 headers.
 * The L0 process is the hard real-time kernel of the architecture and must stay
 * free of the ROS middleware. It only depends on hhros2_motor_protocol, which is
 * a header-only ABI contract.
 *
 * The server creates and owns the POSIX shared-memory segment; the ros2_control
 * HAL (hhros2_hal::ShmMotorClient) attaches to it as the reader of feedback and
 * writer of command.
 */

#include <cstdint>
#include <string>

#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"

namespace hhros2_l0
{

class L0ShmServer
{
public:
    L0ShmServer() = default;
    ~L0ShmServer();

    L0ShmServer(const L0ShmServer &) = delete;
    L0ShmServer & operator=(const L0ShmServer &) = delete;

    /// Create + map (and zero-initialize) the shared segment.
    bool start(const std::string & name);
    void stop();
    bool running() const { return segment_ != nullptr; }

    /// Publish a measured snapshot (seqlock writer, feedback direction).
    void publish_feedback(
        const hhros2::shm::JointFeedback * joints,
        int count,
        const hhros2::shm::ImuSample & imu);

    /// Consistent read of the latest command (seqlock reader, command direction).
    bool fetch_command(hhros2::shm::JointCommand * joints, int count);

    /// True when the HAL / hhros2_core has enabled torque output.
    bool enabled() const;

    void set_cycle_counter(std::uint64_t value);
    void set_fault_code(std::uint32_t code);

private:
    int fd_ = -1;
    std::string name_;
    hhros2::shm::MotorShmSegment * segment_ = nullptr;
};

}  // namespace hhros2_l0
