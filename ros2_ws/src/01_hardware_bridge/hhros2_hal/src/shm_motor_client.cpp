#include "hhros2_hal/shm_motor_client.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define HHROS2_HAVE_POSIX_SHM 1
#else
#define HHROS2_HAVE_POSIX_SHM 0
#endif

namespace hhros2_hal
{

using hhros2::shm::ImuSample;
using hhros2::shm::JointCommand;
using hhros2::shm::JointFeedback;
using hhros2::shm::kJointCount;
using hhros2::shm::kShmSize;
using hhros2::shm::MotorShmSegment;

ShmMotorClient::~ShmMotorClient()
{
    close();
}

bool ShmMotorClient::open(const std::string & name, bool create)
{
#if HHROS2_HAVE_POSIX_SHM
    close();
    name_ = name;
    const std::string path = name.front() == '/' ? name : "/" + name;

    const int flags = create ? (O_RDWR | O_CREAT) : O_RDWR;
    fd_ = ::shm_open(path.c_str(), flags, 0660);
    if (fd_ < 0)
    {
        return false;
    }

    if (create)
    {
        if (::ftruncate(fd_, static_cast<off_t>(kShmSize)) != 0)
        {
            close();
            return false;
        }
        created_ = true;
    }

    void * addr = ::mmap(
        nullptr, kShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr == MAP_FAILED)
    {
        close();
        return false;
    }

    segment_ = static_cast<MotorShmSegment *>(addr);
    if (create)
    {
        // Placement-construct so the atomics start in a defined state.
        new (segment_) MotorShmSegment();
    }
    return true;
#else
    (void)name;
    (void)create;
    return false;  // shared memory unsupported on this platform (dev host only)
#endif
}

void ShmMotorClient::close()
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
    if (created_ && !name_.empty())
    {
        const std::string path = name_.front() == '/' ? name_ : "/" + name_;
        ::shm_unlink(path.c_str());
        created_ = false;
    }
#endif
}

bool ShmMotorClient::read_feedback(
    JointFeedback * joints, int count, ImuSample * imu)
{
    if (segment_ == nullptr)
    {
        return false;
    }
    const int n = std::min(count, kJointCount);

    // seqlock reader: retry while the writer is mid-update or the seq moved.
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const std::uint32_t seq_before =
            segment_->feedback_seq.load(std::memory_order_acquire);
        if ((seq_before & 1U) != 0U)
        {
            continue;  // writer in progress
        }
        if (joints != nullptr)
        {
            std::memcpy(joints, segment_->feedback, sizeof(JointFeedback) * n);
        }
        if (imu != nullptr)
        {
            *imu = segment_->imu;
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        const std::uint32_t seq_after =
            segment_->feedback_seq.load(std::memory_order_relaxed);
        if (seq_before == seq_after)
        {
            return true;
        }
    }
    return false;
}

void ShmMotorClient::write_command(const JointCommand * joints, int count)
{
    if (segment_ == nullptr || joints == nullptr)
    {
        return;
    }
    const int n = std::min(count, kJointCount);

    const std::uint32_t seq =
        segment_->command_seq.load(std::memory_order_relaxed);
    segment_->command_seq.store(seq + 1, std::memory_order_release);  // odd
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(segment_->command, joints, sizeof(JointCommand) * n);
    std::atomic_thread_fence(std::memory_order_release);
    segment_->command_seq.store(seq + 2, std::memory_order_release);  // even
}

void ShmMotorClient::set_enable(bool enable)
{
    if (segment_ != nullptr)
    {
        segment_->motor_enable.store(enable ? 1U : 0U, std::memory_order_release);
    }
}

bool ShmMotorClient::enabled() const
{
    return segment_ != nullptr &&
           segment_->motor_enable.load(std::memory_order_acquire) != 0U;
}

std::uint64_t ShmMotorClient::l0_cycle_counter() const
{
    return segment_ != nullptr
               ? segment_->l0_cycle_counter.load(std::memory_order_acquire)
               : 0U;
}

std::uint32_t ShmMotorClient::l0_fault_code() const
{
    return segment_ != nullptr
               ? segment_->l0_fault_code.load(std::memory_order_acquire)
               : 0U;
}

}  // namespace hhros2_hal
