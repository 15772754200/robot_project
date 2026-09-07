#pragma once

/**
 * @file exipc_system_hardware.hpp
 * @brief ros2_control SystemInterface for the Yidong humanoid (L0.5 HAL).
 *
 * This plugin is the standardized hardware resource the rest of the stack
 * programs against. It exposes per-joint position/velocity/effort PLUS the
 * composite kp/kd command interfaces (the key enabler for RL + compliant
 * control), and an IMU sensor. In read()/write() it talks to the L0 real-time
 * runtime exclusively through lock-free shared memory, so the EtherCAT bus and
 * its 1-4 kHz loop stay isolated from the ROS process and its jitter.
 *
 * Sim-to-Real: the controllers and estimators bind to these interfaces, so
 * switching to hhros2_sim/MujocoSystemHardware needs zero changes upstream.
 */

#include <string>
#include <vector>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "hhros2_hal/shm_motor_client.hpp"
#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"

namespace hhros2_hal
{

class ExipcSystemHardware : public hardware_interface::SystemInterface
{
public:
    RCLCPP_SHARED_PTR_DEFINITIONS(ExipcSystemHardware)

    hardware_interface::CallbackReturn on_init(
        const hardware_interface::HardwareInfo & info) override;

    hardware_interface::CallbackReturn on_configure(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_activate(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_deactivate(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_cleanup(
        const rclcpp_lifecycle::State & previous_state) override;

    std::vector<hardware_interface::StateInterface> export_state_interfaces()
        override;

    std::vector<hardware_interface::CommandInterface>
    export_command_interfaces() override;

    hardware_interface::return_type read(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

    hardware_interface::return_type write(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
    void hold_current_position();

    // joint state storage (read from shared memory)
    std::vector<double> hw_pos_;
    std::vector<double> hw_vel_;
    std::vector<double> hw_eff_;

    // joint command storage (written to shared memory)
    std::vector<double> cmd_pos_;
    std::vector<double> cmd_vel_;
    std::vector<double> cmd_eff_;
    std::vector<double> cmd_kp_;
    std::vector<double> cmd_kd_;

    // IMU sensor state storage (parallel to imu_iface_names_)
    std::vector<double> imu_states_;
    std::vector<std::string> imu_iface_names_;

    // scratch shared-memory frames
    std::vector<hhros2::shm::JointFeedback> fb_;
    std::vector<hhros2::shm::JointCommand> cmd_frame_;
    hhros2::shm::ImuSample imu_sample_{};

    ShmMotorClient client_;
    std::string shm_name_ = hhros2::shm::kDefaultShmName;
    int exipc_port_ = 1;
    bool active_ = false;
    std::size_t read_miss_count_ = 0;
};

}  // namespace hhros2_hal
