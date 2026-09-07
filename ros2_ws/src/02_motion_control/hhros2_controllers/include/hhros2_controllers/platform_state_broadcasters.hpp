#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "controller_interface/controller_interface.hpp"
#include "rclcpp/publisher.hpp"
#include "realtime_tools/realtime_publisher.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace hhros2_controllers
{

// Project-owned ros2_control broadcasters. The stock broadcasters do not opt
// their publishers into ROS QoS overrides, so their runtime DDS policy can
// silently disagree with hhros_bringup/config/qos_overrides.yaml.
class PlatformJointStateBroadcaster
  : public controller_interface::ControllerInterface
{
public:
  controller_interface::CallbackReturn on_init() override;

  controller_interface::InterfaceConfiguration
  command_interface_configuration() const override;

  controller_interface::InterfaceConfiguration
  state_interface_configuration() const override;

  controller_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  controller_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  controller_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  controller_interface::return_type update(
    const rclcpp::Time & time,
    const rclcpp::Duration & period) override;

private:
  static constexpr std::size_t kStateInterfaces = 3;
  enum StateSlot : std::size_t { POSITION = 0, VELOCITY = 1, EFFORT = 2 };

  std::vector<std::string> joint_names_;
  std::vector<std::array<std::size_t, kStateInterfaces>> state_indexes_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr publisher_;
  std::shared_ptr<
    realtime_tools::RealtimePublisher<sensor_msgs::msg::JointState>>
  realtime_publisher_;
};

class PlatformImuBroadcaster
  : public controller_interface::ControllerInterface
{
public:
  controller_interface::CallbackReturn on_init() override;

  controller_interface::InterfaceConfiguration
  command_interface_configuration() const override;

  controller_interface::InterfaceConfiguration
  state_interface_configuration() const override;

  controller_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  controller_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  controller_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  controller_interface::return_type update(
    const rclcpp::Time & time,
    const rclcpp::Duration & period) override;

private:
  static constexpr std::size_t kImuInterfaces = 10;

  std::string sensor_name_;
  std::string frame_id_;
  std::array<std::size_t, kImuInterfaces> state_indexes_{};
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr publisher_;
  std::shared_ptr<realtime_tools::RealtimePublisher<sensor_msgs::msg::Imu>>
  realtime_publisher_;
};

}  // namespace hhros2_controllers
