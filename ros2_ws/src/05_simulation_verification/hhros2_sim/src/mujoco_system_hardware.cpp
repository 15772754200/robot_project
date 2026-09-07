#include "hhros2_sim/mujoco_system_hardware.hpp"

#include <yaml-cpp/yaml.h>

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace hhros2_sim
{
namespace
{
constexpr char kLogger[] = "hhros2_sim.MujocoSystemHardware";
constexpr char kImuSensorName[] = "base_imu";
constexpr char kUnmodeledJointName[] = "head_yaw_joint";
constexpr std::size_t kJointCount = 23;
constexpr std::array<const char *, 5> kJointCommandInterfaceNames{{
  "position", "velocity", "effort", "kp", "kd"}};
constexpr std::array<const char *, 3> kJointStateInterfaceNames{{
  "position", "velocity", "effort"}};
constexpr std::array<const char *, 10> kImuInterfaceNames{{
  "orientation.x", "orientation.y", "orientation.z", "orientation.w",
  "angular_velocity.x", "angular_velocity.y", "angular_velocity.z",
  "linear_acceleration.x", "linear_acceleration.y", "linear_acceleration.z"}};

rclcpp::PublisherOptions publisher_options()
{
  rclcpp::PublisherOptions options;
  options.qos_overriding_options = rclcpp::QosOverridingOptions({
    rclcpp::QosPolicyKind::History,
    rclcpp::QosPolicyKind::Depth,
    rclcpp::QosPolicyKind::Reliability,
    rclcpp::QosPolicyKind::Durability,
  });
  return options;
}

std::string resolve_uri(const std::string & uri)
{
  const std::string prefix = "package://";
  if (uri.rfind(prefix, 0) == 0) {
    const std::string rest = uri.substr(prefix.size());
    const auto slash = rest.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= rest.size()) {
      throw std::runtime_error(
              "Resource URI must be package://<package>/<relative_path>");
    }
    return ament_index_cpp::get_package_share_directory(
      rest.substr(0, slash)) + rest.substr(slash);
  }
  if (!uri.empty() && uri.front() == '/') {
    return uri;
  }
  throw std::runtime_error(
          "Resource path must be absolute or use package://<package>/<relative_path>");
}

struct HomePose
{
  std::array<double, 3> base_position{{0.0, 0.0, 0.80}};
  std::array<double, 4> base_orientation_wxyz{{1.0, 0.0, 0.0, 0.0}};
  std::vector<double> joint_positions;
};

template<std::size_t Size>
std::array<double, Size> read_finite_array(
  const YAML::Node & node, const char * key)
{
  const YAML::Node values = node[key];
  if (!values || !values.IsSequence() || values.size() != Size) {
    throw std::runtime_error(
            std::string("home pose field has invalid size: ") + key);
  }
  std::array<double, Size> result{};
  for (std::size_t i = 0; i < Size; ++i) {
    result[i] = values[i].as<double>();
    if (!std::isfinite(result[i])) {
      throw std::runtime_error(
              std::string("home pose field must be finite: ") + key);
    }
  }
  return result;
}

HomePose load_home_pose(
  const std::string & path,
  const std::vector<std::string> & joint_names)
{
  const YAML::Node root = YAML::LoadFile(resolve_uri(path));
  const YAML::Node base = root["base"];
  const YAML::Node positions = root["joint_positions"];
  if (!base || !base.IsMap()) {
    throw std::runtime_error("home pose yaml must contain map field: base");
  }
  if (!positions || !positions.IsMap() || positions.size() != joint_names.size()) {
    throw std::runtime_error(
            "home pose joint_positions must match the ros2_control joint set");
  }

  HomePose pose;
  pose.base_position = read_finite_array<3>(base, "position");
  pose.base_orientation_wxyz =
    read_finite_array<4>(base, "orientation_wxyz");
  double quaternion_norm_squared = 0.0;
  for (const double value : pose.base_orientation_wxyz) {
    quaternion_norm_squared += value * value;
  }
  if (quaternion_norm_squared <= 1.0e-24) {
    throw std::runtime_error("home pose quaternion must be non-zero");
  }
  const double quaternion_norm = std::sqrt(quaternion_norm_squared);
  for (double & value : pose.base_orientation_wxyz) {
    value /= quaternion_norm;
  }

  pose.joint_positions.reserve(joint_names.size());
  for (const auto & joint_name : joint_names) {
    const YAML::Node value = positions[joint_name];
    if (!value) {
      throw std::runtime_error("home pose missing joint: " + joint_name);
    }
    const double position = value.as<double>();
    if (!std::isfinite(position)) {
      throw std::runtime_error(
              "home pose joint must be finite: " + joint_name);
    }
    pose.joint_positions.push_back(position);
  }
  return pose;
}

double parse_finite_double(const std::string & value, const char * name)
{
  std::size_t parsed = 0;
  const double result = std::stod(value, &parsed);
  if (parsed != value.size() || !std::isfinite(result)) {
    throw std::runtime_error(std::string(name) + " must be finite");
  }
  return result;
}

template<typename InterfaceContainer, std::size_t Size>
bool has_exact_interfaces(
  const InterfaceContainer & interfaces,
  const std::array<const char *, Size> & expected)
{
  if (interfaces.size() != expected.size()) {
    return false;
  }
  std::unordered_set<std::string> names;
  for (const auto & interface : interfaces) {
    names.insert(interface.name);
  }
  if (names.size() != expected.size()) {
    return false;
  }
  for (const char * name : expected) {
    if (names.count(name) == 0) {
      return false;
    }
  }
  return true;
}
}  // namespace

MujocoSystemHardware::~MujocoSystemHardware()
{
  if (node_executor_) {
    node_executor_->cancel();
  }
  if (node_spin_thread_.joinable()) {
    node_spin_thread_.join();
  }
}

hardware_interface::CallbackReturn MujocoSystemHardware::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
    hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }
  n_ = info_.joints.size();
  if (n_ != kJointCount) {
    RCLCPP_ERROR(
      rclcpp::get_logger(kLogger),
      "Expected exactly %zu logical joints, got %zu", kJointCount, n_);
    return hardware_interface::CallbackReturn::ERROR;
  }
  std::unordered_set<std::string> unique_joint_names;
  for (const auto & joint : info_.joints) {
    if (!unique_joint_names.insert(joint.name).second) {
      RCLCPP_ERROR(
        rclcpp::get_logger(kLogger), "Joint names must be unique");
      return hardware_interface::CallbackReturn::ERROR;
    }
    if (!has_exact_interfaces(
        joint.command_interfaces, kJointCommandInterfaceNames) ||
      !has_exact_interfaces(
        joint.state_interfaces, kJointStateInterfaceNames))
    {
      RCLCPP_ERROR(
        rclcpp::get_logger(kLogger),
        "Joint '%s' must expose position/velocity/effort/kp/kd commands "
        "and position/velocity/effort states",
        joint.name.c_str());
      return hardware_interface::CallbackReturn::ERROR;
    }
  }

  const auto param = [&](const std::string & key, const std::string & def) {
    auto it = info_.hardware_parameters.find(key);
    return it != info_.hardware_parameters.end() ? it->second : def;
  };
  model_path_ = param("model_path", "");
  initial_pose_path_ = param("initial_pose_path", "");
  floating_base_joint_ = param("floating_base_joint", "");
  qos_config_path_ = param("qos_config", "");
  try {
    sim_rate_hz_ = parse_finite_double(
      param("sim_rate_hz", ""), "sim_rate_hz");
  } catch (const std::exception & e) {
    RCLCPP_ERROR(
      rclcpp::get_logger(kLogger),
      "Invalid MuJoCo hardware parameter: %s", e.what());
    return hardware_interface::CallbackReturn::ERROR;
  }

  hw_pos_.assign(n_, 0.0);
  hw_vel_.assign(n_, 0.0);
  hw_eff_.assign(n_, 0.0);
  cmd_pos_.assign(n_, std::numeric_limits<double>::quiet_NaN());
  cmd_vel_.assign(n_, 0.0);
  cmd_eff_.assign(n_, 0.0);
  cmd_kp_.assign(n_, 0.0);
  cmd_kd_.assign(n_, 0.0);

  mujoco_joint_names_.clear();
  mujoco_joint_names_.reserve(n_);
  std::unordered_set<std::string> unique_mujoco_joint_names;
  for (std::size_t i = 0; i < n_; ++i) {
    const auto & joint = info_.joints[i];
    const auto & p = info_.joints[i].parameters;
    const auto it = p.find("mujoco_joint");
    const std::string mujoco_name =
      it == p.end() ? std::string() : it->second;
    if (joint.name == kUnmodeledJointName) {
      if (!mujoco_name.empty()) {
        RCLCPP_ERROR(
          rclcpp::get_logger(kLogger),
          "%s must remain absent from the 22-DoF MuJoCo plant",
          kUnmodeledJointName);
        return hardware_interface::CallbackReturn::ERROR;
      }
    } else if (mujoco_name.empty()) {
      RCLCPP_ERROR(
        rclcpp::get_logger(kLogger),
        "Joint '%s' has no mujoco_joint mapping",
        joint.name.c_str());
      return hardware_interface::CallbackReturn::ERROR;
    } else if (!unique_mujoco_joint_names.insert(mujoco_name).second) {
      RCLCPP_ERROR(
        rclcpp::get_logger(kLogger),
        "MuJoCo joint mappings must be unique: %s",
        mujoco_name.c_str());
      return hardware_interface::CallbackReturn::ERROR;
    }
    mujoco_joint_names_.push_back(mujoco_name);
  }
  if (unique_joint_names.count(kUnmodeledJointName) == 0) {
    RCLCPP_ERROR(
      rclcpp::get_logger(kLogger),
      "Missing required logical joint '%s'", kUnmodeledJointName);
    return hardware_interface::CallbackReturn::ERROR;
  }

  if (info_.sensors.size() != 1 ||
    info_.sensors.front().name != kImuSensorName)
  {
    RCLCPP_ERROR(
      rclcpp::get_logger(kLogger),
      "Expected exactly one '%s' sensor", kImuSensorName);
    return hardware_interface::CallbackReturn::ERROR;
  }
  imu_iface_names_.clear();
  std::unordered_set<std::string> unique_imu_interfaces;
  for (const auto & interface : info_.sensors.front().state_interfaces) {
    imu_iface_names_.push_back(interface.name);
    unique_imu_interfaces.insert(interface.name);
  }
  if (unique_imu_interfaces.size() != kImuInterfaceNames.size()) {
    RCLCPP_ERROR(
      rclcpp::get_logger(kLogger),
      "%s must expose 10 unique interfaces", kImuSensorName);
    return hardware_interface::CallbackReturn::ERROR;
  }
  for (const char * interface_name : kImuInterfaceNames) {
    if (unique_imu_interfaces.count(interface_name) == 0) {
      RCLCPP_ERROR(
        rclcpp::get_logger(kLogger),
        "%s is missing interface '%s'",
        kImuSensorName, interface_name);
      return hardware_interface::CallbackReturn::ERROR;
    }
  }
  imu_states_.assign(imu_iface_names_.size(), 0.0);
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn MujocoSystemHardware::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  try {
    std::vector<std::string> joint_names;
    joint_names.reserve(n_);
    for (const auto & joint : info_.joints) {
      joint_names.push_back(joint.name);
    }
    const HomePose home_pose = load_home_pose(initial_pose_path_, joint_names);
    home_positions_ = home_pose.joint_positions;

    const std::string resolved_model_path = resolve_uri(model_path_);
    MujocoEngineConfig cfg;
    cfg.model_path = resolved_model_path;
    cfg.floating_base_joint_name = floating_base_joint_;
    cfg.sim_rate_hz = sim_rate_hz_;
    cfg.mujoco_joint_names = mujoco_joint_names_;
    cfg.initial_positions = home_pose.joint_positions;
    cfg.initial_base_position = home_pose.base_position;
    cfg.initial_base_orientation_wxyz = home_pose.base_orientation_wxyz;
    engine_ = std::make_unique<MujocoEngine>(std::move(cfg));
    rope_available_.store(engine_->has_safety_rope());
    rope_command_.store(RopeCommand::kNone);
    engine_->read(hw_pos_, hw_vel_, hw_eff_);
    start_ros_interfaces();
    RCLCPP_INFO(
      rclcpp::get_logger(kLogger),
      "MuJoCo engine loaded model '%s' (%zu joints).",
      resolved_model_path.c_str(), n_);
  } catch (const std::exception & e) {
    RCLCPP_ERROR(
      rclcpp::get_logger(kLogger),
      "Failed to initialize MuJoCo engine: %s", e.what());
    return hardware_interface::CallbackReturn::ERROR;
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

void MujocoSystemHardware::start_ros_interfaces()
{
  if (!node_) {
    rclcpp::NodeOptions node_options;
    if (!qos_config_path_.empty()) {
      node_options.arguments(
        {"--ros-args", "--params-file", qos_config_path_});
    }
    node_ = rclcpp::Node::make_shared(
      "mujoco_system_hardware", node_options);
  }
  base_pose_pub_ = node_->create_publisher<geometry_msgs::msg::PoseStamped>(
    "/mujoco/base_pose", rclcpp::QoS(1), publisher_options());
  rope_length_pub_ = node_->create_publisher<std_msgs::msg::Float64>(
    "/mujoco/rope/length", rclcpp::SensorDataQoS(), publisher_options());
  rope_raise_service_ = node_->create_service<std_srvs::srv::Trigger>(
    "/mujoco/rope/raise",
    [this](const std::shared_ptr<std_srvs::srv::Trigger::Request>,
    std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
      response->success = rope_available_.load();
      response->message = response->success ?
      "safety rope is rewinding to the held length" :
      "the selected MuJoCo scene has no safety_rope tendon";
      if (response->success) {
        rope_command_.store(RopeCommand::kRaise);
      }
    });
  rope_release_service_ = node_->create_service<std_srvs::srv::Trigger>(
    "/mujoco/rope/release",
    [this](const std::shared_ptr<std_srvs::srv::Trigger::Request>,
    std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
      response->success = rope_available_.load();
      response->message = response->success ?
      "safety rope released to the slack fall-arrest length" :
      "the selected MuJoCo scene has no safety_rope tendon";
      if (response->success) {
        rope_command_.store(RopeCommand::kRelease);
      }
    });
  if (!node_executor_) {
    node_executor_ =
      std::make_unique<rclcpp::executors::SingleThreadedExecutor>();
    node_executor_->add_node(node_);
    node_spin_thread_ = std::thread([this]() {node_executor_->spin();});
  }
}

void MujocoSystemHardware::publish_telemetry(const rclcpp::Time & time)
{
  const auto snap = engine_->base_pose();
  geometry_msgs::msg::PoseStamped msg;
  msg.header.stamp = time;
  msg.header.frame_id = "world";
  msg.pose.position.x = snap.position[0];
  msg.pose.position.y = snap.position[1];
  msg.pose.position.z = snap.position[2];
  msg.pose.orientation.w = snap.orientation_wxyz[0];
  msg.pose.orientation.x = snap.orientation_wxyz[1];
  msg.pose.orientation.y = snap.orientation_wxyz[2];
  msg.pose.orientation.z = snap.orientation_wxyz[3];
  base_pose_pub_->publish(msg);

  if (engine_->has_safety_rope()) {
    std_msgs::msg::Float64 rope_length;
    rope_length.data = engine_->safety_rope_length();
    rope_length_pub_->publish(rope_length);
  }
}

std::vector<hardware_interface::StateInterface>
MujocoSystemHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> ifaces;
  for (std::size_t i = 0; i < n_; ++i) {
    const auto & name = info_.joints[i].name;
    ifaces.emplace_back(name, hardware_interface::HW_IF_POSITION, &hw_pos_[i]);
    ifaces.emplace_back(name, hardware_interface::HW_IF_VELOCITY, &hw_vel_[i]);
    ifaces.emplace_back(name, hardware_interface::HW_IF_EFFORT, &hw_eff_[i]);
  }
  std::size_t k = 0;
  for (const auto & sensor : info_.sensors) {
    for (const auto & si : sensor.state_interfaces) {
      ifaces.emplace_back(sensor.name, si.name, &imu_states_[k++]);
    }
  }
  return ifaces;
}

std::vector<hardware_interface::CommandInterface>
MujocoSystemHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> ifaces;
  for (std::size_t i = 0; i < n_; ++i) {
    const auto & name = info_.joints[i].name;
    ifaces.emplace_back(name, hardware_interface::HW_IF_POSITION, &cmd_pos_[i]);
    ifaces.emplace_back(name, hardware_interface::HW_IF_VELOCITY, &cmd_vel_[i]);
    ifaces.emplace_back(name, hardware_interface::HW_IF_EFFORT, &cmd_eff_[i]);
    ifaces.emplace_back(name, "kp", &cmd_kp_[i]);
    ifaces.emplace_back(name, "kd", &cmd_kd_[i]);
  }
  return ifaces;
}

hardware_interface::CallbackReturn MujocoSystemHardware::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  engine_->reset();
  rope_command_.store(RopeCommand::kNone);
  engine_->read(hw_pos_, hw_vel_, hw_eff_);

  // Start passive. Governance must explicitly activate a motion controller.
  for (std::size_t i = 0; i < n_; ++i) {
    cmd_pos_[i] = home_positions_[i];
    cmd_vel_[i] = 0.0;
    cmd_eff_[i] = 0.0;
    cmd_kp_[i] = 0.0;
    cmd_kd_[i] = 0.0;
  }

  active_ = true;
  RCLCPP_INFO(
    rclcpp::get_logger(kLogger),
    "Hardware activated at canonical home pose. MuJoCo physics is running.");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn MujocoSystemHardware::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  active_ = false;
  return hardware_interface::CallbackReturn::SUCCESS;
}

void MujocoSystemHardware::set_imu_state(const std::string & iface, double value)
{
  for (std::size_t k = 0; k < imu_iface_names_.size(); ++k) {
    if (imu_iface_names_[k] == iface) {imu_states_[k] = value;}
  }
}

hardware_interface::return_type MujocoSystemHardware::read(
  const rclcpp::Time & time, const rclcpp::Duration & /*period*/)
{
  engine_->read(hw_pos_, hw_vel_, hw_eff_);
  const auto imu = engine_->imu();
  set_imu_state("orientation.w", imu.orientation_wxyz[0]);
  set_imu_state("orientation.x", imu.orientation_wxyz[1]);
  set_imu_state("orientation.y", imu.orientation_wxyz[2]);
  set_imu_state("orientation.z", imu.orientation_wxyz[3]);
  set_imu_state("angular_velocity.x", imu.angular_velocity[0]);
  set_imu_state("angular_velocity.y", imu.angular_velocity[1]);
  set_imu_state("angular_velocity.z", imu.angular_velocity[2]);
  set_imu_state("linear_acceleration.x", imu.linear_acceleration[0]);
  set_imu_state("linear_acceleration.y", imu.linear_acceleration[1]);
  set_imu_state("linear_acceleration.z", imu.linear_acceleration[2]);
  publish_telemetry(time);
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type MujocoSystemHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  if (!active_) {return hardware_interface::return_type::OK;}

  const RopeCommand rope_command =
    rope_command_.exchange(RopeCommand::kNone);
  if (rope_command == RopeCommand::kRaise) {
    engine_->raise_safety_rope();
  } else if (rope_command == RopeCommand::kRelease) {
    engine_->release_safety_rope();
  }
  engine_->step(cmd_pos_, cmd_vel_, cmd_eff_, cmd_kp_, cmd_kd_);
  return hardware_interface::return_type::OK;
}

}  // namespace hhros2_sim

PLUGINLIB_EXPORT_CLASS(
  hhros2_sim::MujocoSystemHardware, hardware_interface::SystemInterface)
