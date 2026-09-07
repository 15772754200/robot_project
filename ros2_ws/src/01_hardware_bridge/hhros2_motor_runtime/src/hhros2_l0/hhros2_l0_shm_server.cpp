#include "hhros2_motor_runtime/hhros2_l0_shm_server.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <new>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define HHROS2_HAVE_POSIX_SHM 1
#else
#define HHROS2_HAVE_POSIX_SHM 0
#endif

namespace hhros2_l0
{

using hhros2::shm::ImuSample;
using hhros2::shm::JointCommand;
using hhros2::shm::JointFeedback;
using hhros2::shm::kJointCount;
using hhros2::shm::kShmSize;
using hhros2::shm::MotorShmSegment;

L0ShmServer::~L0ShmServer()
{
    stop();
}

bool L0ShmServer::start(const std::string & name)
{
#if HHROS2_HAVE_POSIX_SHM
    stop();
    name_ = name;
    const std::string path = name.front() == '/' ? name : "/" + name;

    fd_ = ::shm_open(path.c_str(), O_RDWR | O_CREAT, 0660);
    if (fd_ < 0)
    {
        return false;
    }
    if (::ftruncate(fd_, static_cast<off_t>(kShmSize)) != 0)
    {
        stop();
        return false;
    }
    void * addr = ::mmap(
        nullptr, kShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr == MAP_FAILED)
    {
        stop();
        return false;
    }
    segment_ = new (addr) MotorShmSegment();
    return true;
#else
    (void)name;
    return false;
#endif
}

void L0ShmServer::stop()
{
#if HHROS2_HAVE_POSIX_SHM
    if (segment_ != nullptr)
    {
        ::munmap(segment_, kShmSize);
        segment_ = nullptr;
    }
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
    if (!name_.empty())
    {
        const std::string path = name_.front() == '/' ? name_ : "/" + name_;
        ::shm_unlink(path.c_str());
        name_.clear();
    }
#endif
}

void L0ShmServer::publish_feedback(
    const JointFeedback * joints, int count, const ImuSample & imu)
{
    if (segment_ == nullptr || joints == nullptr)
    {
        return;
    }
    const int n = std::min(count, kJointCount);
    const std::uint32_t seq =
        segment_->feedback_seq.load(std::memory_order_relaxed);
    segment_->feedback_seq.store(seq + 1, std::memory_order_release);  // odd
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(segment_->feedback, joints, sizeof(JointFeedback) * n);
    segment_->imu = imu;
    std::atomic_thread_fence(std::memory_order_release);
    segment_->feedback_seq.store(seq + 2, std::memory_order_release);  // even
}

bool L0ShmServer::fetch_command(JointCommand * joints, int count)
{
    if (segment_ == nullptr || joints == nullptr)
    {
        return false;
    }
    const int n = std::min(count, kJointCount);
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const std::uint32_t seq_before =
            segment_->command_seq.load(std::memory_order_acquire);
        if ((seq_before & 1U) != 0U)
        {
            continue;
        }
        std::memcpy(joints, segment_->command, sizeof(JointCommand) * n);
        std::atomic_thread_fence(std::memory_order_acquire);
        const std::uint32_t seq_after =
            segment_->command_seq.load(std::memory_order_relaxed);
        if (seq_before == seq_after)
        {
            return true;
        }
    }
    return false;
}

bool L0ShmServer::enabled() const
{
    return segment_ != nullptr &&
           segment_->motor_enable.load(std::memory_order_acquire) != 0U;
}

void L0ShmServer::set_cycle_counter(std::uint64_t value)
{
    if (segment_ != nullptr)
    {
        segment_->l0_cycle_counter.store(value, std::memory_order_release);
    }
}

void L0ShmServer::set_fault_code(std::uint32_t code)
{
    if (segment_ != nullptr)
    {
        segment_->l0_fault_code.store(code, std::memory_order_release);
    }
}

}  // namespace hhros2_l0
