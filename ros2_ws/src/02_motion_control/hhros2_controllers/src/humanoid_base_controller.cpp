#include "hhros2_controllers/humanoid_base_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "controller_interface/helpers.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"

#include "probe/single_shot_probe.h"
#include "hhros2_log/log.h"

namespace hhros2_controllers
{
namespace
{

rclcpp::SubscriptionOptions subscription_options()
{
    rclcpp::SubscriptionOptions options;
    options.qos_overriding_options = rclcpp::QosOverridingOptions({
        rclcpp::QosPolicyKind::History,
        rclcpp::QosPolicyKind::Depth,
        rclcpp::QosPolicyKind::Reliability,
        rclcpp::QosPolicyKind::Durability,
    });
    return options;
}

}  // namespace

using controller_interface::interface_configuration_type;
using controller_interface::InterfaceConfiguration;
using CallbackReturn = controller_interface::CallbackReturn;

controller_interface::CallbackReturn HumanoidBaseController::on_init()
{
    // Declare parameters with safe defaults (ROS parameter best practice).      // tmny edit
    try
    {
        auto_declare<std::vector<std::string>>("joints", {});
        auto_declare<double>("smoothing_tau", 0.02);
    }
    catch (const std::exception & e)
    {
        fprintf(stderr, "HumanoidBaseController on_init error: %s\n", e.what());
        return CallbackReturn::ERROR;
    }
    return CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn HumanoidBaseController::on_configure(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    joint_names_ = get_node()->get_parameter("joints").as_string_array();
    smoothing_tau_s_ = get_node()->get_parameter("smoothing_tau").as_double();
    n_joints_ = joint_names_.size();
    if (n_joints_ == 0)
    {
        LOG_ERROR(LogType::CONTROLLERLOG,"'joints' parameter is empty");
        return CallbackReturn::ERROR;
    }

    applied_pos_.assign(n_joints_, std::numeric_limits<double>::quiet_NaN());
    applied_vel_.assign(n_joints_, 0.0);
    applied_eff_.assign(n_joints_, 0.0);
    applied_kp_.assign(n_joints_, 0.0);
    applied_kd_.assign(n_joints_, 0.0);

    // Reference can also arrive on a topic for upstream nodes that are not       // tmny edit
    // chained (RL/WBC components share a process for intra-process zero copy).   // tmny edit
    command_sub_ = get_node()->create_subscription<
        hhros2_interfaces::msg::JointMotor>(
        "~/reference", rclcpp::SystemDefaultsQoS(),
        [this](const hhros2_interfaces::msg::JointMotor::SharedPtr msg)
        { rt_command_.writeFromNonRT(msg); },
        subscription_options());

    LOG_INFO(LogType::CONTROLLERLOG,
        "Configured base controller: %zu joints, smoothing_tau=%.3fs",
        n_joints_, smoothing_tau_s_);
    return CallbackReturn::SUCCESS;
}

InterfaceConfiguration
HumanoidBaseController::command_interface_configuration() const
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

InterfaceConfiguration
HumanoidBaseController::state_interface_configuration() const
{
    // Claim measured position/velocity so activation can seed the command and   // tmny edit
    // interpolation starts from the real pose (bumpless transfer).              // tmny edit
    InterfaceConfiguration config;
    config.type = interface_configuration_type::INDIVIDUAL;
    config.names.reserve(n_joints_ * 2);
    for (const auto & joint : joint_names_)
    {
        config.names.push_back(joint + "/" + hardware_interface::HW_IF_POSITION);
        config.names.push_back(joint + "/" + hardware_interface::HW_IF_VELOCITY);
    }
    return config;
}

std::vector<hardware_interface::CommandInterface>
HumanoidBaseController::on_export_reference_interfaces()
{
    reference_interfaces_.resize(
        n_joints_ * kSlots, std::numeric_limits<double>::quiet_NaN());

    std::vector<hardware_interface::CommandInterface> refs;
    refs.reserve(reference_interfaces_.size());
    const std::string prefix = get_node()->get_name();
    static const char * slot_name[kSlots] = {
        "position", "velocity", "effort", "kp", "kd"};
    for (std::size_t j = 0; j < n_joints_; ++j)
    {
        for (int s = 0; s < kSlots; ++s)
        {
            refs.emplace_back(
                prefix, joint_names_[j] + "/" + slot_name[s],
                &reference_interfaces_[ref_index(j, static_cast<Slot>(s))]);
        }
    }
    return refs;
}

bool HumanoidBaseController::on_set_chained_mode(bool /*chained_mode*/)
{
    return true;  // supports both chained and subscriber-driven operation        // tmny edit
}

controller_interface::CallbackReturn HumanoidBaseController::on_activate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // Seed applied command and references from the measured joint state so we    // tmny edit
    // never command a jump on activation.                                        // tmny edit
    for (std::size_t j = 0; j < n_joints_; ++j)
    {
        const double q = state_interfaces_[j * 2].get_value();
        applied_pos_[j] = q;
        applied_vel_[j] = 0.0;
        applied_eff_[j] = 0.0;
        applied_kp_[j] = 0.0;
        applied_kd_[j] = 0.0;
        reference_interfaces_[ref_index(j, POS)] = q;
        reference_interfaces_[ref_index(j, VEL)] = 0.0;
        reference_interfaces_[ref_index(j, EFF)] = 0.0;
        reference_interfaces_[ref_index(j, KP)] = 0.0;
        reference_interfaces_[ref_index(j, KD)] = 0.0;
    }
    rt_command_.reset();
    return CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn HumanoidBaseController::on_deactivate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // Release stiffness on the way out; the HAL / damping controller takes over. // tmny edit
    for (std::size_t j = 0; j < n_joints_; ++j)
    {
        command_interfaces_[j * kSlots + KP].set_value(0.0);
        command_interfaces_[j * kSlots + KD].set_value(0.0);
        command_interfaces_[j * kSlots + EFF].set_value(0.0);
    }
    return CallbackReturn::SUCCESS;
}

controller_interface::return_type
HumanoidBaseController::update_reference_from_subscribers()
{
    // Copy the latest subscriber target into the reference interfaces. In chained // tmny edit
    // mode an upstream controller writes reference_interfaces_ directly instead.  // tmny edit
    auto msg_ptr = rt_command_.readFromRT();
    if (msg_ptr == nullptr || (*msg_ptr) == nullptr)
    {
        return controller_interface::return_type::OK;
    }
    /***debug log start***/
    // 记录控制器收到命令的时刻
    auto *trace = single_shot_probe::map_trace();
    if (trace && single_shot_probe::ready(trace) &&
        trace->motor_node_rx_ns == 0U) {
        trace->motor_node_rx_ns = single_shot_probe::now_ns();
    }
     /***debug log end***/
     
    const auto & msg = **msg_ptr;
    
    // // 添加日志：打印第一个关节位置
    // if (msg.position.size() > 0) {
    //     RCLCPP_INFO(get_node()->get_logger(), 
    //         "HumanoidBaseController received reference: pos[0]=%.3f", msg.position[0]);
    // }
    //std::cout << "[Controller] Received ref pos[0]=" << msg.position[0] << std::endl;
    
    const std::size_t n = std::min<std::size_t>(n_joints_, msg.position.size());
    for (std::size_t j = 0; j < n; ++j)
    {
        reference_interfaces_[ref_index(j, POS)] = msg.position[j];
        reference_interfaces_[ref_index(j, VEL)] = msg.velocity[j];
        reference_interfaces_[ref_index(j, EFF)] = msg.effort[j];
        reference_interfaces_[ref_index(j, KP)] = msg.kp[j];
        reference_interfaces_[ref_index(j, KD)] = msg.kd[j];
    }
    return controller_interface::return_type::OK;
}

controller_interface::return_type
HumanoidBaseController::update_and_write_commands(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
    // First-order interpolation toward the reference. alpha in [0,1] grows with  // tmny edit
    // the control period, giving a smooth, rate-limited approach at 500 Hz.      // tmny edit
    const double dt = period.seconds();
    const double alpha =
        smoothing_tau_s_ > 1e-6 ? std::clamp(dt / smoothing_tau_s_, 0.0, 1.0)
                                : 1.0;

    for (std::size_t j = 0; j < n_joints_; ++j)
    {
        const double ref_pos = reference_interfaces_[ref_index(j, POS)];
        const double ref_vel = reference_interfaces_[ref_index(j, VEL)];
        const double ref_eff = reference_interfaces_[ref_index(j, EFF)];
        const double ref_kp = reference_interfaces_[ref_index(j, KP)];
        const double ref_kd = reference_interfaces_[ref_index(j, KD)];

        if (std::isnan(ref_pos))
        {
            continue;  // no reference yet; hold last applied command            // tmny edit
        }
        if (std::isnan(applied_pos_[j]))
        {
            applied_pos_[j] = state_interfaces_[j * 2].get_value();
        }

        applied_pos_[j] += alpha * (ref_pos - applied_pos_[j]);
        applied_vel_[j] += alpha * (ref_vel - applied_vel_[j]);
        applied_eff_[j] += alpha * (ref_eff - applied_eff_[j]);
        // Gains track the reference quickly so impedance changes are not lagged. // tmny edit
        applied_kp_[j] = ref_kp;
        applied_kd_[j] = ref_kd;

        command_interfaces_[j * kSlots + POS].set_value(applied_pos_[j]);
        command_interfaces_[j * kSlots + VEL].set_value(applied_vel_[j]);
        command_interfaces_[j * kSlots + EFF].set_value(applied_eff_[j]);
        command_interfaces_[j * kSlots + KP].set_value(applied_kp_[j]);
        command_interfaces_[j * kSlots + KD].set_value(applied_kd_[j]);
    }
    return controller_interface::return_type::OK;
}


}  // namespace hhros2_controllers

PLUGINLIB_EXPORT_CLASS(
    hhros2_controllers::HumanoidBaseController,
    controller_interface::ChainableControllerInterface)
