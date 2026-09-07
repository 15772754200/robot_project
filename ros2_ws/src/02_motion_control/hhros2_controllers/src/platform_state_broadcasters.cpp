#include "hhros2_controllers/platform_state_broadcasters.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <unordered_map>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "probe/single_shot_probe.h"
#include "rclcpp/qos_overriding_options.hpp"

namespace hhros2_controllers
{
namespace
{

using controller_interface::interface_configuration_type;
using controller_interface::InterfaceConfiguration;
using CallbackReturn = controller_interface::CallbackReturn;

constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

rclcpp::PublisherOptions publisher_options()
{
  rclcpp::PublisherOptions options;
  options.qos_overriding_options = rclcpp::QosOverridingOptions(
      {
        rclcpp::QosPolicyKind::History,
        rclcpp::QosPolicyKind::Depth,
        rclcpp::QosPolicyKind::Reliability,
        rclcpp::QosPolicyKind::Durability,
      });
  return options;
}

std::unordered_map<std::string, std::size_t> state_interface_indexes(
  const std::vector<hardware_interface::LoanedStateInterface> & interfaces)
{
  std::unordered_map<std::string, std::size_t> indexes;
  indexes.reserve(interfaces.size());
  for (std::size_t index = 0; index < interfaces.size(); ++index) {
    indexes.emplace(interfaces[index].get_name(), index);
  }
  return indexes;
}

}  // namespace

CallbackReturn PlatformJointStateBroadcaster::on_init()
{
  try {
    auto_declare<std::vector<std::string>>("joints", {});
  } catch (const std::exception & exception) {
    RCLCPP_ERROR(
      get_node()->get_logger(), "Failed to declare parameters: %s",
      exception.what());
    return CallbackReturn::ERROR;
  }
  return CallbackReturn::SUCCESS;
}

InterfaceConfiguration
PlatformJointStateBroadcaster::command_interface_configuration() const
{
  return {interface_configuration_type::NONE, {}};
}

InterfaceConfiguration
PlatformJointStateBroadcaster::state_interface_configuration() const
{
  InterfaceConfiguration configuration;
  configuration.type = interface_configuration_type::INDIVIDUAL;
  configuration.names.reserve(joint_names_.size() * kStateInterfaces);
  for (const auto & joint : joint_names_) {
    configuration.names.push_back(
      joint + "/" + hardware_interface::HW_IF_POSITION);
    configuration.names.push_back(
      joint + "/" + hardware_interface::HW_IF_VELOCITY);
    configuration.names.push_back(
      joint + "/" + hardware_interface::HW_IF_EFFORT);
  }
  return configuration;
}

CallbackReturn PlatformJointStateBroadcaster::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  joint_names_ = get_node()->get_parameter("joints").as_string_array();
  if (joint_names_.empty()) {
    RCLCPP_ERROR(get_node()->get_logger(), "'joints' parameter is empty");
    return CallbackReturn::ERROR;
  }
  if (std::unordered_map<std::string, bool> unique;
    std::any_of(
      joint_names_.begin(), joint_names_.end(),
      [&unique](const auto & name) {return !unique.emplace(name, true).second;}))
  {
    RCLCPP_ERROR(get_node()->get_logger(), "'joints' contains duplicates");
    return CallbackReturn::ERROR;
  }

  publisher_ = get_node()->create_publisher<sensor_msgs::msg::JointState>(
    "joint_states", rclcpp::SensorDataQoS(), publisher_options());
  realtime_publisher_ = std::make_shared<
    realtime_tools::RealtimePublisher<sensor_msgs::msg::JointState>>(
    publisher_);
  auto & message = realtime_publisher_->msg_;
  message.name = joint_names_;
  message.position.resize(joint_names_.size());
  message.velocity.resize(joint_names_.size());
  message.effort.resize(joint_names_.size());
  state_indexes_.assign(
    joint_names_.size(),
    std::array<std::size_t, kStateInterfaces>{
      kInvalidIndex, kInvalidIndex, kInvalidIndex});
  return CallbackReturn::SUCCESS;
}

CallbackReturn PlatformJointStateBroadcaster::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  const auto indexes = state_interface_indexes(state_interfaces_);
  static const std::array<const char *, kStateInterfaces> interface_names{{
    hardware_interface::HW_IF_POSITION,
    hardware_interface::HW_IF_VELOCITY,
    hardware_interface::HW_IF_EFFORT,
  }};
  for (std::size_t joint = 0; joint < joint_names_.size(); ++joint) {
    for (std::size_t slot = 0; slot < kStateInterfaces; ++slot) {
      const auto full_name = joint_names_[joint] + "/" + interface_names[slot];
      const auto found = indexes.find(full_name);
      if (found == indexes.end()) {
        RCLCPP_ERROR(
          get_node()->get_logger(), "Missing state interface '%s'",
          full_name.c_str());
        return CallbackReturn::ERROR;
      }
      state_indexes_[joint][slot] = found->second;
    }
  }
  return CallbackReturn::SUCCESS;
}

CallbackReturn PlatformJointStateBroadcaster::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  return CallbackReturn::SUCCESS;
}

controller_interface::return_type PlatformJointStateBroadcaster::update(
  const rclcpp::Time & time, const rclcpp::Duration & /*period*/)
{
  if (!realtime_publisher_ || !realtime_publisher_->trylock()) {
    return controller_interface::return_type::OK;
  }

  auto & message = realtime_publisher_->msg_;
  message.header.stamp = time;
  for (std::size_t joint = 0; joint < joint_names_.size(); ++joint) {
    message.position[joint] =
      state_interfaces_[state_indexes_[joint][POSITION]].get_value();
    message.velocity[joint] =
      state_interfaces_[state_indexes_[joint][VELOCITY]].get_value();
    message.effort[joint] =
      state_interfaces_[state_indexes_[joint][EFFORT]].get_value();
  }
  auto * trace = single_shot_probe::map_trace();
  if (trace && single_shot_probe::ready(trace) &&
    trace->ipc_feedback_rx_ns != 0U && trace->state_pub_ns == 0U)
  {
    trace->state_pub_ns = single_shot_probe::now_ns();
    trace->state_pub_header_ns = time.nanoseconds();
  }
  realtime_publisher_->unlockAndPublish();
  return controller_interface::return_type::OK;
}

CallbackReturn PlatformImuBroadcaster::on_init()
{
  try {
    auto_declare<std::string>("sensor_name", "");
    auto_declare<std::string>("frame_id", "");
  } catch (const std::exception & exception) {
    RCLCPP_ERROR(
      get_node()->get_logger(), "Failed to declare parameters: %s",
      exception.what());
    return CallbackReturn::ERROR;
  }
  return CallbackReturn::SUCCESS;
}

InterfaceConfiguration
PlatformImuBroadcaster::command_interface_configuration() const
{
  return {interface_configuration_type::NONE, {}};
}

InterfaceConfiguration PlatformImuBroadcaster::state_interface_configuration() const
{
  static const std::array<const char *, kImuInterfaces> interface_names{{
    "orientation.x", "orientation.y", "orientation.z", "orientation.w",
    "angular_velocity.x", "angular_velocity.y", "angular_velocity.z",
    "linear_acceleration.x", "linear_acceleration.y", "linear_acceleration.z",
  }};
  InterfaceConfiguration configuration;
  configuration.type = interface_configuration_type::INDIVIDUAL;
  configuration.names.reserve(kImuInterfaces);
  for (const auto * interface_name : interface_names) {
    configuration.names.push_back(sensor_name_ + "/" + interface_name);
  }
  return configuration;
}

CallbackReturn PlatformImuBroadcaster::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  sensor_name_ = get_node()->get_parameter("sensor_name").as_string();
  frame_id_ = get_node()->get_parameter("frame_id").as_string();
  if (sensor_name_.empty() || frame_id_.empty()) {
    RCLCPP_ERROR(
      get_node()->get_logger(),
      "'sensor_name' and 'frame_id' must both be non-empty");
    return CallbackReturn::ERROR;
  }

  publisher_ = get_node()->create_publisher<sensor_msgs::msg::Imu>(
    "~/imu", rclcpp::SensorDataQoS(), publisher_options());
  realtime_publisher_ = std::make_shared<
    realtime_tools::RealtimePublisher<sensor_msgs::msg::Imu>>(publisher_);
  realtime_publisher_->msg_.header.frame_id = frame_id_;
  state_indexes_.fill(kInvalidIndex);
  return CallbackReturn::SUCCESS;
}

CallbackReturn PlatformImuBroadcaster::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  static const std::array<const char *, kImuInterfaces> interface_names{{
    "orientation.x", "orientation.y", "orientation.z", "orientation.w",
    "angular_velocity.x", "angular_velocity.y", "angular_velocity.z",
    "linear_acceleration.x", "linear_acceleration.y", "linear_acceleration.z",
  }};
  const auto indexes = state_interface_indexes(state_interfaces_);
  for (std::size_t slot = 0; slot < kImuInterfaces; ++slot) {
    const auto full_name = sensor_name_ + "/" + interface_names[slot];
    const auto found = indexes.find(full_name);
    if (found == indexes.end()) {
      RCLCPP_ERROR(
        get_node()->get_logger(), "Missing state interface '%s'",
        full_name.c_str());
      return CallbackReturn::ERROR;
    }
    state_indexes_[slot] = found->second;
  }
  return CallbackReturn::SUCCESS;
}

CallbackReturn PlatformImuBroadcaster::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  return CallbackReturn::SUCCESS;
}

controller_interface::return_type PlatformImuBroadcaster::update(
  const rclcpp::Time & time, const rclcpp::Duration & /*period*/)
{
  if (!realtime_publisher_ || !realtime_publisher_->trylock()) {
    return controller_interface::return_type::OK;
  }

  auto & message = realtime_publisher_->msg_;
  message.header.stamp = time;
  message.orientation.x = state_interfaces_[state_indexes_[0]].get_value();
  message.orientation.y = state_interfaces_[state_indexes_[1]].get_value();
  message.orientation.z = state_interfaces_[state_indexes_[2]].get_value();
  message.orientation.w = state_interfaces_[state_indexes_[3]].get_value();
  message.angular_velocity.x = state_interfaces_[state_indexes_[4]].get_value();
  message.angular_velocity.y = state_interfaces_[state_indexes_[5]].get_value();
  message.angular_velocity.z = state_interfaces_[state_indexes_[6]].get_value();
  message.linear_acceleration.x = state_interfaces_[state_indexes_[7]].get_value();
  message.linear_acceleration.y = state_interfaces_[state_indexes_[8]].get_value();
  message.linear_acceleration.z = state_interfaces_[state_indexes_[9]].get_value();
  realtime_publisher_->unlockAndPublish();
  return controller_interface::return_type::OK;
}

}  // namespace hhros2_controllers

PLUGINLIB_EXPORT_CLASS(
  hhros2_controllers::PlatformJointStateBroadcaster,
  controller_interface::ControllerInterface)

PLUGINLIB_EXPORT_CLASS(
  hhros2_controllers::PlatformImuBroadcaster,
  controller_interface::ControllerInterface)
