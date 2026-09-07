#pragma once

// damping_controller: the safe-reflex target. When hhros2_core detects a Level 2 // tmny edit
// fault it switches the active controller to this one, which commands zero        // tmny edit
// position/effort with a fixed joint damping (kp=0, kd=const) so the robot folds   // tmny edit
// down gently and protects the gearboxes instead of holding a stiff pose.         // tmny edit

#include <string>
#include <vector>

#include "controller_interface/controller_interface.hpp"

namespace hhros2_controllers
{

class DampingController : public controller_interface::ControllerInterface
{
public:
    DampingController() = default;

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
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
    enum Slot { POS = 0, VEL = 1, EFF = 2, KP = 3, KD = 4, kSlots = 5 };

    std::vector<std::string> joint_names_;
    std::size_t n_joints_ = 0;
    double damping_kd_ = 2.0;  // safe damping gain [Nm/(rad/s)]                  // tmny edit
};

}  // namespace hhros2_controllers
