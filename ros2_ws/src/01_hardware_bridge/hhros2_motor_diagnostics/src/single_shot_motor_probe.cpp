#include "probe/single_shot_probe.h"

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <hhros2_interfaces/msg/joint_motor.hpp>
#include <hhros2_motor_protocol/hhros2_shm_layout.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/bool.hpp>
#include <yaml-cpp/yaml.h>

namespace
{
constexpr int kJointCount = 23;
constexpr double kCsvSamplePeriodMs = 1.0;

using JointMotor = hhros2_interfaces::msg::JointMotor;
using JointState = sensor_msgs::msg::JointState;
using MotorShmSegment = hhros2::shm::MotorShmSegment;

std::array<std::string, kJointCount> load_canonical_joint_names()
{
    const std::string path =
        ament_index_cpp::get_package_share_directory("hhros2_description") +
        "/config/joint_order.yaml";
    const YAML::Node names_node = YAML::LoadFile(path)["joint_names"];
    if (!names_node.IsSequence() || names_node.size() != kJointCount)
    {
        throw std::runtime_error(path + " must define exactly 23 joint_names");
    }

    std::array<std::string, kJointCount> names;
    std::unordered_set<std::string> unique_names;
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        if (!names_node[index].IsScalar())
        {
            throw std::runtime_error(path + " contains a non-string joint name");
        }
        names[index] = names_node[index].as<std::string>();
        if (names[index].empty() || !unique_names.insert(names[index]).second)
        {
            throw std::runtime_error(path + " contains an empty or duplicate joint name");
        }
    }
    return names;
}

std::string format_csv_double(double value)
{
    std::ostringstream stream;
    stream.precision(9);
    stream << value;
    return stream.str();
}

class SingleShotMotorProbeNode final : public rclcpp::Node
{
public:
    SingleShotMotorProbeNode()
        : Node("single_shot_motor_probe")
    {
        command_topic_ =
            declare_parameter<std::string>("command_topic", "/joint_command");
        feedback_topic_ =
            declare_parameter<std::string>("feedback_topic", "/joint_states");
        shm_name_ = declare_parameter<std::string>(
            "shm_name",
            hhros2::shm::kDefaultShmName);
        enable_topic_ = declare_parameter<std::string>(
            "enable_topic",
            "/motor_control_enable");
        command_index_ = declare_parameter<int>("command_index", 0);
        platform_index_ = declare_parameter<int>("platform_index", command_index_);
        master_index_ = declare_parameter<int>("master_index", 0);
        motor_index_ = declare_parameter<int>("motor_index", 0);
        position_step_rad_ =
            declare_parameter<double>("position_step_rad", 0.0);    // 一下子跳跃多少呢
        step_count_ = declare_parameter<int>("step_count", 1);
        kp_ = declare_parameter<double>("kp", 0.0);
        kd_ = declare_parameter<double>("kd", 0.0);
        command_kp_values_param_ =
            declare_parameter<std::vector<double>>(
                "command_kp_values",
                std::vector<double>(kJointCount, 0.0));
        command_kd_values_param_ =                                  // 非目标target
            declare_parameter<std::vector<double>>(
                "command_kd_values",
                std::vector<double>(kJointCount, 0.0));
        non_target_position_rad_ =
            declare_parameter<double>("non_target_position_rad", 0.0);
        step_all_command_positions_ =
            declare_parameter<bool>("step_all_command_positions", false);
        all_joint_step_rad_ =
            declare_parameter<double>("all_joint_step_rad", position_step_rad_);
        velocity_ = declare_parameter<double>("velocity", 0.0);
        effort_ = declare_parameter<double>("effort", 0.0);
        motion_threshold_rad_ =
            declare_parameter<double>("motion_threshold_rad", 0.001);
        velocity_threshold_rad_s_ =
            declare_parameter<double>("velocity_threshold_rad_s", 0.001);
        baseline_timeout_sec_ =
            declare_parameter<double>("baseline_timeout_sec", 3.0);
        result_timeout_sec_ =
            declare_parameter<double>("result_timeout_sec", 5.0);
        pre_step_hold_sec_ =
            declare_parameter<double>("pre_step_hold_sec", 1.0);
        post_step_hold_sec_ =
            declare_parameter<double>("post_step_hold_sec", 1.0);
        command_publish_period_ms_ =
            declare_parameter<double>("command_publish_period_ms", 5.0);
        send_enable_ = declare_parameter<bool>("send_enable", false);
        csv_path_ = declare_parameter<std::string>("csv_path", "");
        joint_names_ = load_canonical_joint_names();

        validate_parameters();
        map_motor_shm(/*warn_on_failure=*/true);
        open_csv_if_requested();

        command_pub_ = create_publisher<JointMotor>(
            command_topic_,
            rclcpp::QoS(1).reliable().durability_volatile());
        enable_pub_ = create_publisher<std_msgs::msg::Bool>(
            enable_topic_,
            rclcpp::QoS(1).reliable().durability_volatile());
        feedback_sub_ = create_subscription<JointState>(
            feedback_topic_,
            rclcpp::QoS(rclcpp::KeepLast(1)).best_effort().durability_volatile(),
            [this](const JointState::SharedPtr msg) {
                on_joint_state_feedback(*msg);
            });
        RCLCPP_INFO(
            get_logger(),
            "Listening for sensor_msgs/JointState feedback on %s",
            feedback_topic_.c_str());

        const auto timer_period =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double, std::milli>(
                    command_publish_period_ms_));
        command_timer_ = create_wall_timer(timer_period, [this]() { on_timer(); });
        sample_timer_ = create_wall_timer(
            std::chrono::milliseconds(1),
            [this]() { record_periodic_sample_csv(); });

        trace_ = single_shot_probe::map_trace();
        reset_trace_for_new_run();
        start_ns_ = single_shot_probe::now_ns();
    }

    ~SingleShotMotorProbeNode() override
    {
        close_motor_shm();
    }

    bool done() const
    {
        return done_;
    }

private:
    void reset_trace_for_new_run()
    {
        if (trace_ == nullptr)
        {
            return;
        }

        std::memset(trace_, 0, sizeof(*trace_));
        trace_->magic = single_shot_probe::kMagic;
        trace_->version = single_shot_probe::kVersion;
    }

    bool map_motor_shm(bool warn_on_failure)
    {
        if (motor_shm_ != nullptr)
        {
            return true;
        }

        const std::string path =
            shm_name_.empty() || shm_name_.front() == '/'
                ? shm_name_
                : "/" + shm_name_;
        motor_shm_fd_ = ::shm_open(path.c_str(), O_RDWR, 0660);
        if (motor_shm_fd_ < 0)
        {
            if (warn_on_failure)
            {
                RCLCPP_WARN(
                    get_logger(),
                    "Cannot open motor shared memory '%s'; CSV shm columns "
                    "will stay empty until it becomes available.",
                    path.c_str());
            }
            return false;
        }

        void *addr = ::mmap(
            nullptr,
            hhros2::shm::kShmSize,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            motor_shm_fd_,
            0);
        if (addr == MAP_FAILED)
        {
            ::close(motor_shm_fd_);
            motor_shm_fd_ = -1;
            if (warn_on_failure)
            {
                RCLCPP_WARN(
                    get_logger(),
                    "Cannot mmap motor shared memory '%s'; CSV shm columns "
                    "will stay empty.",
                    path.c_str());
            }
            return false;
        }

        motor_shm_ = static_cast<MotorShmSegment *>(addr);
        if (motor_shm_->abi_version != hhros2::shm::kShmAbiVersion ||
            motor_shm_->joint_count != hhros2::shm::kJointCount)
        {
            RCLCPP_WARN(
                get_logger(),
                "Motor shared memory ABI mismatch: abi=%u joints=%u",
                motor_shm_->abi_version,
                motor_shm_->joint_count);
        }
        return true;
    }

    void close_motor_shm()
    {
        if (motor_shm_ != nullptr)
        {
            ::munmap(motor_shm_, hhros2::shm::kShmSize);
            motor_shm_ = nullptr;
        }
        if (motor_shm_fd_ >= 0)
        {
            ::close(motor_shm_fd_);
            motor_shm_fd_ = -1;
        }
    }

    bool read_shm_command_position(double *position)
    {
        if (position == nullptr || !map_motor_shm(/*warn_on_failure=*/false))
        {
            return false;
        }

        const auto index = static_cast<std::size_t>(command_index_);
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const std::uint32_t seq_before =
                motor_shm_->command_seq.load(std::memory_order_acquire);
            if ((seq_before & 1U) != 0U)
            {
                continue;
            }
            const double value = motor_shm_->command[index].position;
            std::atomic_thread_fence(std::memory_order_acquire);
            const std::uint32_t seq_after =
                motor_shm_->command_seq.load(std::memory_order_relaxed);
            if (seq_before == seq_after)
            {
                *position = value;
                return true;
            }
        }
        return false;
    }

    bool read_shm_feedback_position(double *position)
    {
        if (position == nullptr || !map_motor_shm(/*warn_on_failure=*/false))
        {
            return false;
        }

        const auto index = static_cast<std::size_t>(platform_index_);
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const std::uint32_t seq_before =
                motor_shm_->feedback_seq.load(std::memory_order_acquire);
            if ((seq_before & 1U) != 0U)
            {
                continue;
            }
            const double value = motor_shm_->feedback[index].position;
            std::atomic_thread_fence(std::memory_order_acquire);
            const std::uint32_t seq_after =
                motor_shm_->feedback_seq.load(std::memory_order_relaxed);
            if (seq_before == seq_after)
            {
                *position = value;
                return true;
            }
        }
        return false;
    }

    void validate_parameters()
    {
        if (command_index_ < 0 || command_index_ >= kJointCount)
        {
            throw std::runtime_error("command_index must be in [0, 22]");
        }
        if (platform_index_ < 0 || platform_index_ >= kJointCount)
        {
            throw std::runtime_error("platform_index must be in [0, 22]");
        }
        if (master_index_ < 0 || master_index_ > 2)
        {
            throw std::runtime_error("master_index must be in [0, 2]");
        }
        if (motor_index_ < 0 || motor_index_ >= kJointCount)
        {
            throw std::runtime_error("motor_index must be in [0, 22]");
        }
        if (motion_threshold_rad_ < 0.0 || velocity_threshold_rad_s_ < 0.0)
        {
            throw std::runtime_error(
                "motion thresholds must be non-negative");
        }
        if (kp_ < 0.0 || kd_ < 0.0)
        {
            throw std::runtime_error("target kp/kd values must be non-negative");
        }
        if (step_count_ <= 0)
        {
            throw std::runtime_error("step_count must be positive");
        }
        if (!std::isfinite(non_target_position_rad_))
        {
            throw std::runtime_error("non_target_position_rad must be finite");
        }
        if (!std::isfinite(all_joint_step_rad_))
        {
            throw std::runtime_error("all_joint_step_rad must be finite");
        }
        copy_gain_array_parameter(
            command_kp_values_param_,
            "command_kp_values",
            &command_kp_values_);
        copy_gain_array_parameter(
            command_kd_values_param_,
            "command_kd_values",
            &command_kd_values_);
        if (baseline_timeout_sec_ <= 0.0 || result_timeout_sec_ <= 0.0)
        {
            throw std::runtime_error("timeouts must be positive");
        }
        if (pre_step_hold_sec_ < 0.0)
        {
            throw std::runtime_error("pre_step_hold_sec must be non-negative");
        }
        if (post_step_hold_sec_ < 0.0)
        {
            throw std::runtime_error("post_step_hold_sec must be non-negative");
        }
        if (command_publish_period_ms_ <= 0.0)
        {
            throw std::runtime_error(
                "command_publish_period_ms must be positive");
        }
    }

    int post_movement_hold_tick_count() const
    {
        const double tick_count =
            std::ceil(post_step_hold_sec_ * 1000.0 /
                      command_publish_period_ms_);
        if (tick_count <= 0.0)
        {
            return 0;
        }
        return static_cast<int>(tick_count);
    }

    void prepare_next_step(std::uint64_t now_ns)
    {
        ++completed_step_count_;
        if (completed_step_count_ >= step_count_)
        {
            done_ = true;
            return;
        }

        hold_position_ = target_position_;
        target_position_ = hold_position_ + position_step_rad_;
        hold_started_ns_ = now_ns;
        command_published_ = false;
        movement_seen_ = false;
        have_step_reference_ = false;
        post_movement_hold_ticks_remaining_ = 0;
        RCLCPP_INFO(
            get_logger(),
            "Preparing step %d/%d: hold_pos=%.6f target_pos=%.6f",
            completed_step_count_ + 1,
            step_count_,
            hold_position_,
            target_position_);
    }

    void copy_gain_array_parameter(
        const std::vector<double> &values,
        const char *name,
        std::array<double, kJointCount> *target)
    {
        if (target == nullptr)
        {
            return;
        }
        if (values.size() != static_cast<std::size_t>(kJointCount))
        {
            throw std::runtime_error(
                std::string(name) + " must contain exactly 23 values");
        }

        for (int index = 0; index < kJointCount; ++index)
        {
            const double value = values[static_cast<std::size_t>(index)];
            if (!std::isfinite(value) || value < 0.0)
            {
                throw std::runtime_error(
                    std::string(name) +
                    " values must be finite and non-negative");
            }
            (*target)[static_cast<std::size_t>(index)] = value;
        }
    }

    void on_joint_state_feedback(const JointState &msg)
    {
        std::size_t feedback_index = static_cast<std::size_t>(platform_index_);
        if (!msg.name.empty())
        {
            const auto &target_name =
                joint_names_[static_cast<std::size_t>(platform_index_)];
            const auto it =
                std::find(msg.name.begin(), msg.name.end(), target_name);
            if (it == msg.name.end())
            {
                return;
            }
            feedback_index =
                static_cast<std::size_t>(it - msg.name.begin());
        }

        if (msg.position.size() <= feedback_index)
        {
            return;
        }

        const double velocity =
            msg.velocity.size() > feedback_index ? msg.velocity[feedback_index]
                                                 : 0.0;
        on_feedback_sample(msg.position[feedback_index], velocity);
    }

    void on_feedback_sample(double position, double velocity)
    {
        const auto feedback_rx_ns = single_shot_probe::now_ns();
        latest_joint_state_position_ = position;
        have_latest_joint_state_position_ = true;

        if (!have_baseline_)
        {
            baseline_position_ = position;
            baseline_velocity_ = velocity;
            hold_position_ = baseline_position_;
            target_position_ = hold_position_ + position_step_rad_;
            last_feedback_position_ = position;
            last_feedback_velocity_ = velocity;
            have_baseline_ = true;
            hold_started_ns_ = feedback_rx_ns;
            publish_hold_command();
            return;
        }

        last_feedback_position_ = position;
        last_feedback_velocity_ = velocity;

        if (!command_published_)
        {
            return;
        }

        if (movement_seen_)
        {
            return;
        }

        if (!have_step_reference_)
        {
            return;
        }

        const double command_delta =
            target_position_ - step_reference_position_;
        const double feedback_delta =
            position - step_reference_position_;
        const bool has_command_direction =
            std::fabs(command_delta) > 0.0;
        const bool moved_toward_target =
            has_command_direction &&
            (feedback_delta * command_delta > 0.0);
        const bool position_moved =
            moved_toward_target &&
            std::fabs(feedback_delta) >= motion_threshold_rad_;
        if (!position_moved)
        {
            return;
        }

        movement_seen_ = true;
        post_movement_hold_ticks_remaining_ =
            post_movement_hold_tick_count();
        if (trace_ != nullptr && single_shot_probe::ready(trace_) &&
            trace_->state_seen_ns == 0U)
        {
            trace_->state_seen_ns = feedback_rx_ns;
            trace_->state_seen_position = position;
            trace_->state_seen_velocity = velocity;
            single_shot_probe::print_summary_if_ready(trace_);
        }

    }

    double stepped_non_target_position() const
    {
        if (!step_all_command_positions_)
        {
            return non_target_position_rad_;
        }
        return non_target_position_rad_ + all_joint_step_rad_;
    }

    JointMotor make_command(double position, bool step_active) const
    {
        JointMotor command;
        command.header.stamp = now();
        command.header.frame_id = "single_shot_motor_probe";
        command.joint_names = joint_names_;
        command.effort.fill(std::numeric_limits<double>::quiet_NaN());
        (void)step_active;
        const double unset = std::numeric_limits<double>::quiet_NaN();
        for (int index = 0; index < kJointCount; ++index)
        {
            const auto array_index = static_cast<std::size_t>(index);
            command.kp[array_index] = unset;
            command.kd[array_index] = unset;
            command.position[array_index] = unset;
            command.velocity[array_index] = unset;
        }
        // 后面对目标关节单独赋值
        command.kp[static_cast<std::size_t>(command_index_)] = kp_;
        command.kd[static_cast<std::size_t>(command_index_)] = kd_;
        command.position[static_cast<std::size_t>(command_index_)] = position;
        command.velocity[static_cast<std::size_t>(command_index_)] = velocity_;
        command.effort[static_cast<std::size_t>(command_index_)] = effort_;
        return command;
    }

    void publish_enable_if_requested()
    {
        if (!send_enable_)
        {
            return;
        }

        std_msgs::msg::Bool enable_msg;
        enable_msg.data = true;
        enable_pub_->publish(enable_msg);
    }

    void publish_hold_command()
    {
        publish_enable_if_requested();
        const bool step_active = completed_step_count_ > 0;
        auto command = make_command(hold_position_, step_active);
        update_latest_reference_position(command);
        command_pub_->publish(command);
    }

    void publish_step_hold_command()
    {
        publish_enable_if_requested();
        auto command = make_command(target_position_, true);
        update_latest_reference_position(command);
        command_pub_->publish(command);
    }

    void arm_and_publish_step_command()
    {
        step_reference_position_ = last_feedback_position_;
        have_step_reference_ = true;

        RCLCPP_INFO(
            get_logger(),
            "Publishing step %d/%d: reference_pos=%.6f target_pos=%.6f "
            "step_rad=%.6f",
            completed_step_count_ + 1,
            step_count_,
            step_reference_position_,
            target_position_,
            position_step_rad_);

        single_shot_probe::arm_trace(
            trace_,
            platform_index_,
            master_index_,
            motor_index_,
            target_position_);

        publish_enable_if_requested();
        auto command = make_command(target_position_, true);

        if (trace_ != nullptr)
        {
            trace_->topic_pub_ns = single_shot_probe::now_ns();
        }
        update_latest_reference_position(command);
        command_pub_->publish(command);
        command_published_ = true;
        command_pub_ns_ = single_shot_probe::now_ns();
    }

    void update_latest_reference_position(const JointMotor &command)
    {
        latest_reference_position_ =
            command.position[static_cast<std::size_t>(command_index_)];
        have_latest_reference_position_ = true;
    }

    void on_timer()
    {
        const auto now_ns = single_shot_probe::now_ns();
        if (!have_baseline_)    // 如果没有基准线，其实就是在等待反馈
        {
            const double elapsed_sec =
                static_cast<double>(now_ns - start_ns_) / 1.0e9;
            if (elapsed_sec > baseline_timeout_sec_)
            {
                RCLCPP_ERROR(
                    get_logger(),
                    "Timed out waiting %.3fs for baseline feedback on %s "
                    "(sensor_msgs/JointState). Check that joint_state_broadcaster "
                    "is active and publishing.",
                    baseline_timeout_sec_,
                    feedback_topic_.c_str());
                RCLCPP_ERROR(
                    get_logger(),
                    "等待 %s 基线反馈超时 %.3fs，请先确认 joint_state_broadcaster "
                    "已 active 且 /joint_states 持续发布",
                    feedback_topic_.c_str(),
                    baseline_timeout_sec_);
                done_ = true;
            }
            return;
        }
        // 这个就是先让电机进入保持状态先保持一段时间
        if (!command_published_)
        {
            if (completed_step_count_ > 0)
            {
                arm_and_publish_step_command();
                return;
            }

            const double hold_elapsed_sec =
                static_cast<double>(now_ns - hold_started_ns_) / 1.0e9;
            if (hold_elapsed_sec < pre_step_hold_sec_)
            {
                publish_hold_command();
                return;
            }

            arm_and_publish_step_command();
            return;
        }

        if (movement_seen_) // 已经观察到目标电机发生明显变化
        {
            publish_step_hold_command();
            if (post_movement_hold_ticks_remaining_ > 0)
            {
                --post_movement_hold_ticks_remaining_;
                return;
            }

            prepare_next_step(now_ns);
            return;
        }

        publish_step_hold_command();
        const double elapsed_sec =
            static_cast<double>(now_ns - command_pub_ns_) / 1.0e9;
        if (elapsed_sec <= result_timeout_sec_)
        {
            return;
        }

        RCLCPP_WARN(
            get_logger(),
            "等待反馈运动超时 %.3fs：baseline_pos=%.6f target_pos=%.6f。"
            "如果底层 summary 已打印，说明通信到了反馈层；如果没有，"
            "检查 motor_enable、kp/kd、目标 master/motor 映射和电机是否上使能。",
            result_timeout_sec_,
            baseline_position_,
            target_position_);
        done_ = true;
    }

    void open_csv_if_requested()
    {
        if (csv_path_.empty())
        {
            RCLCPP_WARN(
                get_logger(),
                "没有输入 probe CSV路径，将无法写入 CSV");
            return;
        }

        csv_.open(csv_path_, std::ios::out | std::ios::trunc);
        if (!csv_)
        {
            RCLCPP_WARN(
                get_logger(),
                "无法打开 probe CSV: %s，将无法写入 CSV",
                csv_path_.c_str());
            return;
        }

        csv_.precision(9);
        csv_ << "sample_ms,shm_command_position_rad,"
                "shm_feedback_position_rad,joint_states_position_rad,"
                "reference_position_rad\n";
        csv_.flush();
    }

    void record_periodic_sample_csv()
    {
        if (!csv_)
        {
            return;
        }

        double command_position = 0.0;
        double feedback_position = 0.0;
        const bool have_command_position =
            read_shm_command_position(&command_position);
        const bool have_feedback_position =
            read_shm_feedback_position(&feedback_position);

        csv_ << format_csv_double(sample_index_ * kCsvSamplePeriodMs) << ",";
        if (have_command_position)
        {
            csv_ << format_csv_double(command_position);
        }
        csv_ << ",";
        if (have_feedback_position)
        {
            csv_ << format_csv_double(feedback_position);
        }
        csv_ << ",";
        if (have_latest_joint_state_position_)
        {
            csv_ << format_csv_double(latest_joint_state_position_);
        }
        csv_ << ",";
        if (have_latest_reference_position_)
        {
            csv_ << format_csv_double(latest_reference_position_);
        }
        csv_ << "\n";

        ++sample_index_;
        csv_.flush();
    }

    std::string command_topic_;
    std::string feedback_topic_;
    std::string shm_name_;
    std::string enable_topic_;
    std::string csv_path_;
    int command_index_ = 0;
    int platform_index_ = 0;
    int master_index_ = 0;
    int motor_index_ = 0;
    int step_count_ = 1;
    double position_step_rad_ = 0.0;
    double kp_ = 0.0;
    double kd_ = 0.0;
    double non_target_position_rad_ = 0.0;
    double all_joint_step_rad_ = 0.0;
    double velocity_ = 0.0;
    double effort_ = 0.0;
    double motion_threshold_rad_ = 0.001;
    double velocity_threshold_rad_s_ = 0.001;
    double baseline_timeout_sec_ = 3.0;
    double result_timeout_sec_ = 5.0;
    double pre_step_hold_sec_ = 1.0;
    double post_step_hold_sec_ = 1.0;
    double command_publish_period_ms_ = 5.0;
    bool step_all_command_positions_ = false;
    bool send_enable_ = false;

    single_shot_probe::TraceData *trace_ = nullptr;
    int motor_shm_fd_ = -1;
    MotorShmSegment *motor_shm_ = nullptr;
    rclcpp::Publisher<JointMotor>::SharedPtr command_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr enable_pub_;
    rclcpp::Subscription<JointState>::SharedPtr feedback_sub_;
    rclcpp::TimerBase::SharedPtr command_timer_;
    rclcpp::TimerBase::SharedPtr sample_timer_;

    std::uint64_t start_ns_ = 0U;
    std::uint64_t command_pub_ns_ = 0U;
    std::uint64_t hold_started_ns_ = 0U;
    std::uint64_t sample_index_ = 0U;
    bool have_baseline_ = false;
    bool command_published_ = false;
    bool movement_seen_ = false;
    bool done_ = false;
    int completed_step_count_ = 0;
    int post_movement_hold_ticks_remaining_ = 0;
    double baseline_position_ = 0.0;
    double baseline_velocity_ = 0.0;
    double last_feedback_position_ = 0.0;
    double last_feedback_velocity_ = 0.0;
    double latest_joint_state_position_ = 0.0;
    double latest_reference_position_ = 0.0;
    double step_reference_position_ = 0.0;
    double hold_position_ = 0.0;
    double target_position_ = 0.0;
    bool have_step_reference_ = false;
    bool have_latest_joint_state_position_ = false;
    bool have_latest_reference_position_ = false;
    std::vector<double> command_kp_values_param_;
    std::vector<double> command_kd_values_param_;
    std::array<double, kJointCount> command_kp_values_{};
    std::array<double, kJointCount> command_kd_values_{};
    std::array<std::string, kJointCount> joint_names_{};
    std::ofstream csv_;
};
} // namespace

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    int exit_code = 0;
    try
    {
        auto node = std::make_shared<SingleShotMotorProbeNode>();
        rclcpp::executors::SingleThreadedExecutor executor;
        executor.add_node(node);
        while (rclcpp::ok() && !node->done())
        {
            executor.spin_some(std::chrono::milliseconds(1));
        }
        executor.remove_node(node);
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "single_shot_motor_probe failed: %s\n", e.what());
        exit_code = 1;
    }
    rclcpp::shutdown();
    return exit_code;
}
