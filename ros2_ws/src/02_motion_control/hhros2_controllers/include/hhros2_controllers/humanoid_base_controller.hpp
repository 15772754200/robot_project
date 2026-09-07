#pragma once

// humanoid_base_controller: the chainable 500 Hz core of the motion-control      // tmny edit
// layer. Upstream cores (RL policy, WBC/QP) publish a full hybrid command        // tmny edit
// (q_des, dq_des, tau_ff, kp, kd) at 50-100 Hz; this controller interpolates it  // tmny edit
// inside its update() loop and writes the result straight into the HAL command   // tmny edit
// interfaces. Because it is a ros2_control chainable controller, the reference    // tmny edit
// can also arrive via reference interfaces (pointer exchange, zero DDS latency).  // tmny edit

#include <memory>
#include <string>
#include <vector>

#include "controller_interface/chainable_controller_interface.hpp"
#include "hhros2_interfaces/msg/joint_motor.hpp"
#include "rclcpp/subscription.hpp"
#include "realtime_tools/realtime_buffer.hpp"

namespace hhros2_controllers
{

class HumanoidBaseController
: public controller_interface::ChainableControllerInterface
{
public:
    HumanoidBaseController() = default;

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

protected:
    std::vector<hardware_interface::CommandInterface>
    on_export_reference_interfaces() override;

    bool on_set_chained_mode(bool chained_mode) override;

    // Humble's ChainableControllerInterface declares this without a time/period; // tmny edit
    // newer distros add arguments. Keep the no-arg signature for Humble.
    controller_interface::return_type update_reference_from_subscribers()
        override;

    controller_interface::return_type update_and_write_commands(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
    // Per-joint reference slot ordering inside reference_interfaces_.           // tmny edit
    enum Slot { POS = 0, VEL = 1, EFF = 2, KP = 3, KD = 4, kSlots = 5 };

    std::size_t ref_index(std::size_t joint, Slot slot) const
    {
        return joint * kSlots + static_cast<std::size_t>(slot);
    }

    std::vector<std::string> joint_names_;
    std::size_t n_joints_ = 0;

    // Smoothing time constant for interpolating sparse references to 500 Hz.    // tmny edit
    double smoothing_tau_s_ = 0.02;

    // Latest target from the subscriber path (non-chained upstream).            // tmny edit
    realtime_tools::RealtimeBuffer<
        std::shared_ptr<hhros2_interfaces::msg::JointMotor>>
        rt_command_;
    rclcpp::Subscription<hhros2_interfaces::msg::JointMotor>::SharedPtr
        command_sub_;

    // Interpolated command actually written to the HAL each cycle.             // tmny edit
    std::vector<double> applied_pos_;
    std::vector<double> applied_vel_;
    std::vector<double> applied_eff_;
    std::vector<double> applied_kp_;
    std::vector<double> applied_kd_;

};

}  // namespace hhros2_controllers
