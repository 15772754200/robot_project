#pragma once

// tmny edit
// hhros2_core: the horizontal ("one-across") governance daemon. It is the single
// authority for (a) tiered diagnostics from node heartbeats + IMU tilt, (b) the
// control-mode arbiter that decides which motion core may drive, and (c) the
// safety reflex that, on a Level 2 fault, bypasses every higher layer to switch
// the active controller to damping and/or cut the L0 bus enable. This mirrors
// the centralized E-Stop arbiters used by commercial humanoids.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "controller_manager_msgs/srv/switch_controller.hpp"
#include "hhros2_hal/shm_motor_client.hpp"
#include "hhros2_interfaces/msg/arbitration_mode.hpp"
#include "hhros2_interfaces/msg/heartbeat.hpp"
#include "hhros2_interfaces/msg/safety_status.hpp"
#include "hhros2_interfaces/msg/system_health.hpp"
#include "hhros2_interfaces/srv/set_control_mode.hpp"
#include "hhros2_interfaces/srv/set_system_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

namespace hhros2_core
{

class SafetyGovernorNode : public rclcpp::Node
{
public:
    explicit SafetyGovernorNode(const rclcpp::NodeOptions & options);

private:
    struct SubsystemHealth
    {
        uint8_t level = 0;
        rclcpp::Time last_beat;
    };

    void on_heartbeat(hhros2_interfaces::msg::Heartbeat::ConstSharedPtr msg);
    void on_imu(sensor_msgs::msg::Imu::ConstSharedPtr msg);
    void evaluate();  // periodic fault evaluation + publication                 // tmny edit

    void on_set_mode(
        const std::shared_ptr<hhros2_interfaces::srv::SetControlMode::Request> req,
        std::shared_ptr<hhros2_interfaces::srv::SetControlMode::Response> res);
    void on_set_system_state(
        const std::shared_ptr<hhros2_interfaces::srv::SetSystemState::Request> req,
        std::shared_ptr<hhros2_interfaces::srv::SetSystemState::Response> res);

    void publish_arbitration(uint8_t mode, const std::string & source);
    void trigger_safe_reflex(const std::string & reason);
    bool wait_for_l0_cycle();
    void switch_to_controller(
        const std::string & activate, const std::string & deactivate);

    // params
    std::string shm_name_;
    std::string controller_manager_;
    std::string base_controller_;
    std::string damping_controller_;
    double heartbeat_timeout_s_ = 0.2;    // 心跳超时阀值
    double tilt_fault_rad_ = 0.6;         // 倾斜故障阀值，单位rad
    double l0_liveness_timeout_s_ = 0.5;  // l0runtime存活确认超时
    bool reflex_cut_l0_enable_ = false;   // 安全反射时是否切断L0使能

    // state
    std::map<std::string, SubsystemHealth> health_;
    double tilt_rad_ = 0.0;
    uint8_t safety_level_ = 0;
    uint8_t arbitrated_mode_ = 0;
    std::string active_controller_;
    bool reflex_active_ = false;
    bool system_enabled_ = false;

    hhros2_hal::ShmMotorClient shm_;

    rclcpp::Subscription<hhros2_interfaces::msg::Heartbeat>::SharedPtr beat_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Publisher<hhros2_interfaces::msg::SafetyStatus>::SharedPtr safety_pub_;
    rclcpp::Publisher<hhros2_interfaces::msg::SystemHealth>::SharedPtr health_pub_;
    rclcpp::Publisher<hhros2_interfaces::msg::ArbitrationMode>::SharedPtr mode_pub_;
    rclcpp::Service<hhros2_interfaces::srv::SetControlMode>::SharedPtr mode_srv_;
    rclcpp::Service<hhros2_interfaces::srv::SetSystemState>::SharedPtr sys_srv_;
    rclcpp::Client<controller_manager_msgs::srv::SwitchController>::SharedPtr
        switch_client_;
    rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace hhros2_core
