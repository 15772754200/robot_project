#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "hhros2_sim/mujoco_engine.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/node.hpp"
#include "rclcpp_lifecycle/state.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_srvs/srv/trigger.hpp"

namespace hhros2_sim
{

class MujocoSystemHardware : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(MujocoSystemHardware)

  ~MujocoSystemHardware() override;

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;
  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_deactivate(
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
  enum class RopeCommand
  {
    kNone,
    kRaise,
    kRelease,
  };

  std::size_t n_ = 0;

  std::vector<double> hw_pos_, hw_vel_, hw_eff_;
  std::vector<double> cmd_pos_, cmd_vel_, cmd_eff_, cmd_kp_, cmd_kd_;
  std::vector<double> imu_states_;
  std::vector<std::string> imu_iface_names_;

  std::vector<std::string> mujoco_joint_names_;
  std::string model_path_;
  std::string initial_pose_path_;
  std::string floating_base_joint_;
  std::string qos_config_path_;
  double sim_rate_hz_ = 0.0;
  bool active_ = false;

  void set_imu_state(const std::string & iface, double value);
  void start_ros_interfaces();
  void publish_telemetry(const rclcpp::Time & time);

  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr base_pose_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr rope_length_pub_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr rope_raise_service_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr rope_release_service_;

  // Service callbacks submit commands; only the control thread touches MuJoCo.
  std::atomic<RopeCommand> rope_command_{RopeCommand::kNone};
  std::atomic<bool> rope_available_{false};
  std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> node_executor_;
  std::thread node_spin_thread_;

  std::vector<double> home_positions_;
  std::unique_ptr<MujocoEngine> engine_;
};

}  // namespace hhros2_sim
