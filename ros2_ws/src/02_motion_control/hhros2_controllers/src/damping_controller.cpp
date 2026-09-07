#include "hhros2_controllers/damping_controller.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "hhros2_log/log.h"

namespace hhros2_controllers
{

using controller_interface::interface_configuration_type;
using controller_interface::InterfaceConfiguration;
using CallbackReturn = controller_interface::CallbackReturn;

controller_interface::CallbackReturn DampingController::on_init()
{
    try
    {
        auto_declare<std::vector<std::string>>("joints", {});
        auto_declare<double>("damping_kd", 2.0);
    }
    catch (const std::exception & e)
    {
        fprintf(stderr, "DampingController on_init error: %s\n", e.what());
        return CallbackReturn::ERROR;
    }
    return CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn DampingController::on_configure(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    joint_names_ = get_node()->get_parameter("joints").as_string_array();
    damping_kd_ = get_node()->get_parameter("damping_kd").as_double();
    n_joints_ = joint_names_.size();
    if (n_joints_ == 0)
    {
        LOG_ERROR(LogType::CONTROLLERLOG,"'joints' parameter is empty");
        return CallbackReturn::ERROR;
    }
    return CallbackReturn::SUCCESS;
}

InterfaceConfiguration DampingController::command_interface_configuration() const
{
    InterfaceConfiguration config;
    config.type = interface_configuration_type::INDIVIDUAL;
    config.names.reserve(n_joints_ * kSlots);
    for (const auto & joint : joint_names_)
    {
        config.names.push_back(joint + "/" + hardware_interface::HW_IF_POSITION);
        config.names.push_back(joint + "/" + hardware_interface::HW_IF_VELOCITY);
        config.names.push_back(joint + "/" + hardware_interface::HW_IF_EFFORT);
        config.names.push_back(joint + "/kp");
        config.names.push_back(joint + "/kd");
    }
    return config;
}

InterfaceConfiguration DampingController::state_interface_configuration() const
{
    // Safe reflex needs no state feedback; the HAL/L0 close the damping torque.  // tmny edit
    return {interface_configuration_type::NONE, {}};
}

controller_interface::CallbackReturn DampingController::on_activate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    return CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn DampingController::on_deactivate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    return CallbackReturn::SUCCESS;
}

controller_interface::return_type DampingController::update(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // Pure damping: zero stiffness, zero feed-forward, constant kd. The motors   // tmny edit
    // resist velocity only, so the robot settles instead of collapsing or        // tmny edit
    // holding a stiff and potentially dangerous posture.                         // tmny edit
    for (std::size_t j = 0; j < n_joints_; ++j)
    {
        command_interfaces_[j * kSlots + POS].set_value(0.0);
        command_interfaces_[j * kSlots + VEL].set_value(0.0);
        command_interfaces_[j * kSlots + EFF].set_value(0.0);
        command_interfaces_[j * kSlots + KP].set_value(0.0);
        command_interfaces_[j * kSlots + KD].set_value(damping_kd_);
    }
    return controller_interface::return_type::OK;
}

}  // namespace hhros2_controllers

PLUGINLIB_EXPORT_CLASS(
    hhros2_controllers::DampingController,
    controller_interface::ControllerInterface)
