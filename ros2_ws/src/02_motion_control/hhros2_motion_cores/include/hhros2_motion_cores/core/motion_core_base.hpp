#pragma once

// Shared scaffolding for a "motion core" - one of the two interchangeable cores
// (RL policy or WBC/QP) that feed the chainable humanoid_base_controller. A core
// is a high-level control node (component) that, at 50-100 Hz, turns the desired
// task (cmd_vel + state estimate) into a full hybrid joint command and publishes
// it on the base controller's reference topic. Only the core matching the active
// arbitration mode emits commands, so RL and WBC never fight over the actuators.

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "geometry_msgs/msg/twist.hpp"
#include "hhros2_interfaces/msg/arbitration_mode.hpp"
#include "hhros2_interfaces/msg/base_state.hpp"
#include "hhros2_interfaces/msg/joint_motor.hpp"
#include "rclcpp/rclcpp.hpp"

namespace hhros2_motion_cores
{

class MotionCoreBase : public rclcpp::Node
{
public:
    MotionCoreBase(const std::string & name, const rclcpp::NodeOptions & options);

protected:
    // Derived cores fill `out` with the desired hybrid command for this tick.
    // 返回 true 表示需要发布命令，false 表示跳过发布
    virtual bool compute(
        const geometry_msgs::msg::Twist & cmd_vel,
        const hhros2_interfaces::msg::BaseState & state,
        hhros2_interfaces::msg::JointMotor & out) = 0;

    const std::vector<std::string> & joint_names() const { return joint_names_; }
    std::size_t n_joints() const { return joint_names_.size(); }
    bool is_active() const;
    std::uint64_t mode_generation() const;

private:
    void on_timer();

    std::vector<std::string> joint_names_;
    uint8_t served_mode_ = 0;
    double rate_hz_ = 50.0;

    geometry_msgs::msg::Twist last_cmd_vel_;
    hhros2_interfaces::msg::BaseState last_state_;
    mutable std::mutex input_mutex_;
    std::atomic<uint8_t> active_mode_{0};
    std::atomic<std::uint64_t> mode_generation_{0};

    rclcpp::Publisher<hhros2_interfaces::msg::JointMotor>::SharedPtr ref_pub_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
    rclcpp::Subscription<hhros2_interfaces::msg::BaseState>::SharedPtr state_sub_;
    rclcpp::Subscription<hhros2_interfaces::msg::ArbitrationMode>::SharedPtr
        mode_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace hhros2_motion_cores
