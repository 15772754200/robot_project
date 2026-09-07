#pragma once

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <hhros2_interfaces/srv/set_control_mode.hpp>
#include <memory>
#include "hhros2_teleop/keyboard_cmd/keyboard.h"

namespace hhros2_teleop {

class KeyPublisherNode : public rclcpp_lifecycle::LifecycleNode {
public:
    explicit KeyPublisherNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

    // 生命周期回调
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_configure(const rclcpp_lifecycle::State&) override;

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_activate(const rclcpp_lifecycle::State&) override;

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_deactivate(const rclcpp_lifecycle::State&) override;

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_cleanup(const rclcpp_lifecycle::State&) override;

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_shutdown(const rclcpp_lifecycle::State&) override;

private:
    void requestMode(uint8_t mode);
    void publishCmdVel(double x, double y, double z);

    rclcpp_lifecycle::LifecyclePublisher<geometry_msgs::msg::Twist>::SharedPtr vel_pub_;
    rclcpp::Client<hhros2_interfaces::srv::SetControlMode>::SharedPtr mode_client_;
    std::shared_ptr<KeyBoard> keyboard_;
};

} // namespace hhros2_teleop