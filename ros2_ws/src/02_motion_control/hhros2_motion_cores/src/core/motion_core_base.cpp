#include "hhros2_motion_cores/core/motion_core_base.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace hhros2_motion_cores
{

using namespace std::chrono_literals;
using ArbitrationMode = hhros2_interfaces::msg::ArbitrationMode;

namespace
{

std::string NormalizeModeName(std::string mode)
{
    std::transform(
        mode.begin(), mode.end(), mode.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (mode.rfind("mode_", 0) == 0)
    {
        mode.erase(0, 5);
    }
    return mode;
}

uint8_t ParseServedMode(const std::string & mode_name)
{
    const auto mode = NormalizeModeName(mode_name);
    constexpr std::array<std::pair<const char *, uint8_t>, 7> modes{{
        {"passive", ArbitrationMode::MODE_PASSIVE},
        {"damping", ArbitrationMode::MODE_DAMPING},
        {"stand", ArbitrationMode::MODE_STAND},
        {"walk", ArbitrationMode::MODE_WALK},
        {"run", ArbitrationMode::MODE_RUN},
        {"wbc", ArbitrationMode::MODE_WBC},
        {"motion", ArbitrationMode::MODE_MOTION},
    }};
    const auto found = std::find_if(
        modes.begin(), modes.end(),
        [&mode](const auto & item) { return mode == item.first; });
    if (found != modes.end())
    {
        return found->second;
    }
    throw std::invalid_argument("unknown served_mode '" + mode_name + "'");
}

rclcpp::QosOverridingOptions platform_qos_overrides()
{
    return {
        rclcpp::QosPolicyKind::History,
        rclcpp::QosPolicyKind::Depth,
        rclcpp::QosPolicyKind::Reliability,
        rclcpp::QosPolicyKind::Durability,
    };
}

rclcpp::PublisherOptions publisher_options()
{
    rclcpp::PublisherOptions options;
    options.qos_overriding_options = platform_qos_overrides();
    return options;
}

rclcpp::SubscriptionOptions subscription_options()
{
    rclcpp::SubscriptionOptions options;
    options.qos_overriding_options = platform_qos_overrides();
    return options;
}

rclcpp::SubscriptionOptions latched_subscription_options()
{
    auto options = subscription_options();
    options.use_intra_process_comm = rclcpp::IntraProcessSetting::Disable;
    return options;
}

}  // namespace

MotionCoreBase::MotionCoreBase(
    const std::string & name, const rclcpp::NodeOptions & options)
: rclcpp::Node(name, options)
{
    // Parameters: which joints we drive, our loop rate, and which arbitration
    // mode activates this core (set by hhros2_core's mode arbiter).
    joint_names_ = declare_parameter<std::vector<std::string>>(
        "joints", std::vector<std::string>{});
    rate_hz_ = declare_parameter<double>("rate_hz", 50.0);
    served_mode_ = static_cast<uint8_t>(declare_parameter<int>("served_mode", 0));

    ref_pub_ = create_publisher<hhros2_interfaces::msg::JointMotor>(
        "reference", rclcpp::QoS(1), publisher_options());
    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "/cmd_vel", rclcpp::QoS(1),
        [this](geometry_msgs::msg::Twist::ConstSharedPtr m)
        {
            std::lock_guard<std::mutex> lock(input_mutex_);
            last_cmd_vel_ = *m;
        },
        subscription_options());
    state_sub_ = create_subscription<hhros2_interfaces::msg::BaseState>(
        "/robot/state_estimate", rclcpp::QoS(1),
        [this](hhros2_interfaces::msg::BaseState::ConstSharedPtr m)
        {
            std::lock_guard<std::mutex> lock(input_mutex_);
            last_state_ = *m;
        },
        subscription_options());
    mode_sub_ = create_subscription<hhros2_interfaces::msg::ArbitrationMode>(
        "/humanoid/arbitration_mode", rclcpp::QoS(1),
        [this](hhros2_interfaces::msg::ArbitrationMode::ConstSharedPtr m)
        {
            const auto previous_mode = active_mode_.exchange(m->active_mode);
            if (previous_mode != m->active_mode) {
                mode_generation_.fetch_add(1, std::memory_order_relaxed);
            }
        },
        latched_subscription_options());

    const auto period = std::chrono::duration<double>(1.0 / rate_hz_);
    timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&MotionCoreBase::on_timer, this));

    RCLCPP_INFO(
        get_logger(),
        "Motion core '%s' up: %zu joints, %.0fHz, served_mode=%d",
        name.c_str(), joint_names_.size(), rate_hz_, served_mode_);
}

void MotionCoreBase::on_timer()
{
    // Only the core matching the arbitrated mode is allowed to publish. This is
    // the hand-off guarantee between the RL and WBC cores.
    if (active_mode_ != served_mode_)
    {
        return;
    }
    // JointMotor carries fixed-size [23] arrays (std::array), so we index into
    // them directly rather than resizing.
    hhros2_interfaces::msg::JointMotor cmd;
    cmd.header.stamp = now();
    const std::size_t n = std::min(joint_names_.size(), cmd.joint_names.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        cmd.joint_names[i] = joint_names_[i];
    }
    geometry_msgs::msg::Twist cmd_vel;
    hhros2_interfaces::msg::BaseState state;
    {
        std::lock_guard<std::mutex> lock(input_mutex_);
        cmd_vel = last_cmd_vel_;
        state = last_state_;
    }
    if (compute(cmd_vel, state, cmd)) {
        ref_pub_->publish(cmd);
    }
}

bool MotionCoreBase::is_active() const
{
    return active_mode_ == served_mode_;
}

std::uint64_t MotionCoreBase::mode_generation() const
{
    return mode_generation_.load(std::memory_order_relaxed);
}

}  // namespace hhros2_motion_cores
