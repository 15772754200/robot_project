#include "hhros2_core/safety_governor_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include "hhros2_log/log.h"

namespace hhros2_core
{

using namespace std::chrono_literals;
using SafetyStatus = hhros2_interfaces::msg::SafetyStatus;
using ArbitrationMode = hhros2_interfaces::msg::ArbitrationMode;

namespace
{

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

}  // namespace

SafetyGovernorNode::SafetyGovernorNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("hhros2_core", options)
{
    shm_name_ = declare_parameter<std::string>("shm_name", "hhros2_motor_shm");
    controller_manager_ =
        declare_parameter<std::string>("controller_manager", "/controller_manager");
    base_controller_ = declare_parameter<std::string>(
        "base_controller", "humanoid_base_controller");
    damping_controller_ =
        declare_parameter<std::string>("damping_controller", "damping_controller");
    heartbeat_timeout_s_ =
        declare_parameter<double>("heartbeat_timeout_s", 0.2);
    tilt_fault_rad_ = declare_parameter<double>("tilt_fault_rad", 0.6);
    l0_liveness_timeout_s_ = declare_parameter<double>("l0_liveness_timeout_s", 0.5);
    l0_liveness_timeout_s_ = std::max(0.0, l0_liveness_timeout_s_);
    reflex_cut_l0_enable_ =
        declare_parameter<bool>("reflex_cut_l0_enable", false);

    beat_sub_ = create_subscription<hhros2_interfaces::msg::Heartbeat>(
        "/hhros2/heartbeat", rclcpp::QoS(20),
        std::bind(&SafetyGovernorNode::on_heartbeat, this, std::placeholders::_1),
        subscription_options());
    imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
        "/imu/data", rclcpp::SensorDataQoS(),
        std::bind(&SafetyGovernorNode::on_imu, this, std::placeholders::_1),
        subscription_options());

    safety_pub_ = create_publisher<SafetyStatus>("/hhros2_core/safety_status",
        rclcpp::QoS(10).transient_local(), publisher_options());
    health_pub_ = create_publisher<hhros2_interfaces::msg::SystemHealth>(
        "/hhros2_core/system_health", rclcpp::QoS(10), publisher_options());
    mode_pub_ = create_publisher<ArbitrationMode>(
        "/humanoid/arbitration_mode", rclcpp::QoS(10).transient_local(),
        publisher_options());

    mode_srv_ = create_service<hhros2_interfaces::srv::SetControlMode>(
        "/hhros2_core/set_control_mode",
        std::bind(&SafetyGovernorNode::on_set_mode, this,
            std::placeholders::_1, std::placeholders::_2));
    sys_srv_ = create_service<hhros2_interfaces::srv::SetSystemState>(
        "/hhros2_core/set_system_state",
        std::bind(&SafetyGovernorNode::on_set_system_state, this,
            std::placeholders::_1, std::placeholders::_2));

    switch_client_ =
        create_client<controller_manager_msgs::srv::SwitchController>(
            controller_manager_ + "/switch_controller");

    if (!shm_.open(shm_name_, /*create=*/false))
    {
        LOG_WARNING(LogType::CONTROLLERLOG,
            "Could not attach shm '%s' yet; enable line will retry lazily.",
            shm_name_.c_str());
    }
    else
    {
        // A governance restart must never inherit a previous enable request.
        shm_.set_enable(false);
        system_enabled_ = false;
        LOG_INFO(LogType::CONTROLLERLOG,
            "Attached shm '%s'; motor enable forced low at startup.",
            shm_name_.c_str());
    }

    timer_ = create_wall_timer(
        100ms, std::bind(&SafetyGovernorNode::evaluate, this));  // 10 Hz        // tmny edit
    publish_arbitration(ArbitrationMode::MODE_PASSIVE, "core");
    LOG_INFO(LogType::CONTROLLERLOG, "hhros2_core safety governor started.");
}

void SafetyGovernorNode::on_heartbeat(
    hhros2_interfaces::msg::Heartbeat::ConstSharedPtr msg)
{
    health_[msg->node_name] = SubsystemHealth{msg->level, now()};
}

void SafetyGovernorNode::on_imu(sensor_msgs::msg::Imu::ConstSharedPtr msg)
{
    // tmny edit
    // Tilt = angle between the body-up axis and world gravity, derived from the
    // orientation quaternion. Exceeding the threshold is a fall and a Level 2
    // fault, the canonical "robot is going down" trigger.
    const double x = msg->orientation.x, y = msg->orientation.y;
    const double z = msg->orientation.z, w = msg->orientation.w;
    const double body_up_z = w * w - x * x - y * y + z * z;
    tilt_rad_ = std::acos(std::clamp(body_up_z, -1.0, 1.0));
}

void SafetyGovernorNode::evaluate()
{
    uint8_t worst = SafetyStatus::LEVEL_OK;
    std::string source = "ok";
    std::string reason = "nominal";

    // Heartbeat staleness -> Level 2 (loss of a control-critical subsystem).    // tmny edit
    hhros2_interfaces::msg::SystemHealth health_msg;
    health_msg.header.stamp = now();
    for (auto & [name, h] : health_)
    {
        const double age = (now() - h.last_beat).seconds();
        uint8_t lvl = h.level;
        if (age > heartbeat_timeout_s_)
        {
            lvl = SafetyStatus::LEVEL_CRITICAL;
        }
        health_msg.subsystem.push_back(name);
        health_msg.level.push_back(lvl);
        health_msg.last_heartbeat_age_s.push_back(age);
        if (lvl > worst)
        {
            worst = lvl;
            source = name;
            reason = (age > heartbeat_timeout_s_) ? "heartbeat timeout"
                                                  : "subsystem reported fault";
        }
    }

    // IMU tilt -> Level 2 fall.                                                  // tmny edit
    if (tilt_rad_ > tilt_fault_rad_ && SafetyStatus::LEVEL_CRITICAL > worst)
    {
        worst = SafetyStatus::LEVEL_CRITICAL;
        source = "imu";
        reason = "tilt exceeded fall threshold";
    }

    // L0 fault code -> Level 2.                                                  // tmny edit
    if (shm_.is_open() && shm_.l0_fault_code() != 0 &&
        SafetyStatus::LEVEL_CRITICAL > worst)
    {
        worst = SafetyStatus::LEVEL_CRITICAL;
        source = "l0";
        reason = "L0 runtime fault";
    }

    health_msg.aggregate_level = worst;
    health_pub_->publish(health_msg);

    safety_level_ = worst;
    if (worst >= SafetyStatus::LEVEL_CRITICAL && !reflex_active_)
    {
        trigger_safe_reflex(source + ": " + reason);
    }

    SafetyStatus ss;
    ss.header.stamp = now();
    ss.level = worst;
    ss.source = source;
    ss.reason = reason;
    ss.safe_reflex_active = reflex_active_;
    safety_pub_->publish(ss);
}

void SafetyGovernorNode::trigger_safe_reflex(const std::string & reason)
{
    // tmny edit
    // Safe direct path: bypass perception and high-level control entirely. Switch
    // the active controller to damping so the robot folds down gently, and
    // optionally cut the L0 bus enable as a hard backstop.
    LOG_ERROR(LogType::CONTROLLERLOG, "SAFE REFLEX (L2): %s", reason.c_str());
    reflex_active_ = true;
    switch_to_controller(damping_controller_, base_controller_);
    publish_arbitration(ArbitrationMode::MODE_DAMPING, "core");
    if (reflex_cut_l0_enable_)
    {
        if (!shm_.is_open())
        {
            shm_.open(shm_name_, false);
        }
        shm_.set_enable(false);
    }
}

void SafetyGovernorNode::switch_to_controller(
    const std::string & activate, const std::string & deactivate)
{
    if (!switch_client_->service_is_ready())
    {
        LOG_WARNING(LogType::CONTROLLERLOG, "switch_controller service not ready.");
        return;
    }
    auto req = std::make_shared<
        controller_manager_msgs::srv::SwitchController::Request>();
    req->activate_controllers = {activate};
    req->deactivate_controllers = {deactivate};
    req->strictness = controller_manager_msgs::srv::SwitchController::Request::
        BEST_EFFORT;
    req->activate_asap = true;
    switch_client_->async_send_request(req);
    active_controller_ = activate;
}

void SafetyGovernorNode::publish_arbitration(
    uint8_t mode, const std::string & source)
{
    arbitrated_mode_ = mode;
    ArbitrationMode m;
    m.header.stamp = now();
    m.active_mode = mode;
    m.active_controller = active_controller_;
    m.source = source;
    mode_pub_->publish(m);
}

bool SafetyGovernorNode::wait_for_l0_cycle()
{
    if (!shm_.is_open())
    {
        return false;
    }

    const auto initial_cycle = shm_.l0_cycle_counter();
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::duration<double>(l0_liveness_timeout_s_);

    do
    {
        if (shm_.l0_cycle_counter() != initial_cycle)
        {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < deadline);

    return shm_.l0_cycle_counter() != initial_cycle;
}

void SafetyGovernorNode::on_set_mode(
    const std::shared_ptr<hhros2_interfaces::srv::SetControlMode::Request> req,
    std::shared_ptr<hhros2_interfaces::srv::SetControlMode::Response> res)
{
    // The arbiter refuses mode changes while a safety reflex is latched.        // tmny edit
    if (reflex_active_)
    {
        res->success = false;
        res->active_mode = arbitrated_mode_;
        res->message = "safety reflex active; mode change refused";
        return;
    }
    if (req->mode != ArbitrationMode::MODE_PASSIVE &&
        req->mode != ArbitrationMode::MODE_DAMPING)
    {
        switch_to_controller(base_controller_, damping_controller_);
    }
    publish_arbitration(static_cast<uint8_t>(req->mode), "request");
    res->success = true;
    res->active_mode = arbitrated_mode_;
    res->message = "mode set";
}

void SafetyGovernorNode::on_set_system_state(
    const std::shared_ptr<hhros2_interfaces::srv::SetSystemState::Request> req,
    std::shared_ptr<hhros2_interfaces::srv::SetSystemState::Response> res)
{
    if (req->enable && reflex_active_)
    {
        res->success = false;
        res->message = "cannot enable while safety reflex active";
        return;
    }

    if (!shm_.is_open() && !shm_.open(shm_name_, false))
    {
        res->success = false;
        res->message = "L0 shared memory unavailable; motor state not changed";
        LOG_ERROR(LogType::CONTROLLERLOG,
            "System state request rejected: cannot attach shm '%s'.",
            shm_name_.c_str());
        return;
    }

    // Do not claim that a command reached the hardware runtime merely because
    // the shared-memory object exists. An advancing cycle counter proves that
    // an L0 process currently owns and services this segment.
    if (req->enable && shm_.l0_fault_code() != 0U)
    {
        res->success = false;
        res->message = "L0 reports a fault; motor enable refused";
        return;
    }

    // A stop request is written first so the shared safety line is low even
    // when the runtime disappears while the request is being handled.
    if (!req->enable)
    {
        shm_.set_enable(false);
    }

    if (!wait_for_l0_cycle())
    {
        res->success = false;
        res->message = "L0 runtime is not running; motor state not confirmed";
        if (req->enable)
        {
            shm_.set_enable(false);
        }
        return;
    }

    if (req->enable)
    {
        shm_.set_enable(true);
    }

    if (shm_.enabled() != req->enable)
    {
        res->success = false;
        res->message = "failed to update L0 motor enable state";
        if (req->enable)
        {
            shm_.set_enable(false);
        }
        return;
    }

    // Confirm that the L0 loop is still alive after the command was published.
    if (!wait_for_l0_cycle())
    {
        res->success = false;
        res->message = "L0 stopped while applying motor state; request not confirmed";
        if (req->enable)
        {
            shm_.set_enable(false);
        }
        return;
    }

    system_enabled_ = req->enable;
    res->success = true;
    res->message = req->enable ? "system enabled" : "system disabled";
    LOG_INFO(LogType::CONTROLLERLOG, "System motor enable set to %d.", req->enable);
}

}  // namespace hhros2_core
