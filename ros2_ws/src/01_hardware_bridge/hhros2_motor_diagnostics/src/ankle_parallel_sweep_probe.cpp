#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <hhros2_interfaces/msg/joint_motor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/bool.hpp>
#include <yaml-cpp/yaml.h>

namespace
{

constexpr std::size_t kJointCount = 23;
constexpr std::size_t kLeftAnklePitch = 4;
constexpr std::size_t kLeftAnkleRoll = 5;
constexpr std::size_t kRightAnklePitch = 10;
constexpr std::size_t kRightAnkleRoll = 11;
constexpr double kPi = 3.14159265358979323846;
constexpr double kPhysicalAnkleLimitRad = 0.7854;

using JointMotor = hhros2_interfaces::msg::JointMotor;
using JointState = sensor_msgs::msg::JointState;

std::array<std::string, kJointCount> load_canonical_joint_names()
{
  const std::string path =
    ament_index_cpp::get_package_share_directory("hhros2_description") +
    "/config/joint_order.yaml";
  const YAML::Node names_node = YAML::LoadFile(path)["joint_names"];
  if (!names_node.IsSequence() || names_node.size() != kJointCount) {
    throw std::runtime_error(path + " must define exactly 23 joint_names");
  }

  std::array<std::string, kJointCount> names;
  std::unordered_set<std::string> unique_names;
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (!names_node[index].IsScalar()) {
      throw std::runtime_error(path + " contains a non-string joint name");
    }
    names[index] = names_node[index].as<std::string>();
    if (names[index].empty() || !unique_names.insert(names[index]).second) {
      throw std::runtime_error(path + " contains an empty or duplicate joint name");
    }
  }
  return names;
}

class AnkleParallelSweepProbe final : public rclcpp::Node
{
public:
  AnkleParallelSweepProbe()
  : Node("ankle_parallel_sweep_probe")
  {
    command_topic_ = declare_parameter<std::string>(
      "command_topic", "/humanoid_base_controller/reference");
    feedback_topic_ = declare_parameter<std::string>(
      "feedback_topic", "/joint_states");
    enable_topic_ = declare_parameter<std::string>(
      "enable_topic", "/motor_control_enable");
    side_ = declare_parameter<std::string>("side", "left");
    axis_ = declare_parameter<std::string>("axis", "pitch");
    armed_ = declare_parameter<bool>("armed", false);
    send_enable_ = declare_parameter<bool>("send_enable", false);
    amplitude_rad_ = declare_parameter<double>("amplitude_rad", 0.01);
    frequency_hz_ = declare_parameter<double>("frequency_hz", 0.1);
    ramp_time_sec_ = declare_parameter<double>("ramp_time_sec", 2.0);
    duration_sec_ = declare_parameter<double>("duration_sec", 10.0);
    command_period_ms_ = declare_parameter<double>("command_period_ms", 5.0);
    kp_ = declare_parameter<double>("kp", 10.0);
    kd_ = declare_parameter<double>("kd", 1.0);
    joint_limit_rad_ = declare_parameter<double>("joint_limit_rad", 0.70);
    max_tracking_error_rad_ = declare_parameter<double>(
      "max_tracking_error_rad", 0.15);
    feedback_timeout_sec_ = declare_parameter<double>(
      "feedback_timeout_sec", 0.1);
    csv_path_ = declare_parameter<std::string>("csv_path", "");
    joint_names_ = load_canonical_joint_names();

    validate_parameters();
    configure_side_indices();
    open_csv();

    command_pub_ = create_publisher<JointMotor>(
      command_topic_, rclcpp::QoS(1).reliable().durability_volatile());
    enable_pub_ = create_publisher<std_msgs::msg::Bool>(
      enable_topic_, rclcpp::QoS(1).reliable().durability_volatile());
    feedback_sub_ = create_subscription<JointState>(
      feedback_topic_,
      rclcpp::QoS(rclcpp::KeepLast(1)).best_effort().durability_volatile(),
      [this](const JointState::SharedPtr msg) { on_feedback(*msg); });

    const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double, std::milli>(command_period_ms_));
    timer_ = create_wall_timer(period, [this]() { on_timer(); });

    RCLCPP_INFO(
      get_logger(),
      "Parallel ankle sweep configured for %s leg: virtual joint indices "
      "pitch=%zu roll=%zu. armed=%s",
      side_.c_str(), pitch_index_, roll_index_, armed_ ? "true" : "false");
  }

private:
  void validate_parameters()
  {
    if (side_ != "left" && side_ != "right") {
      throw std::runtime_error("side must be exactly 'left' or 'right'");
    }
    if (axis_ != "pitch" && axis_ != "roll") {
      throw std::runtime_error("axis must be exactly 'pitch' or 'roll'");
    }
    if (amplitude_rad_ <= 0.0 || amplitude_rad_ > 0.05) {
      throw std::runtime_error("amplitude_rad must be in (0, 0.05]");
    }
    if (frequency_hz_ <= 0.0 || frequency_hz_ > 0.5) {
      throw std::runtime_error("frequency_hz must be in (0, 0.5]");
    }
    if (ramp_time_sec_ < 0.0 || duration_sec_ <= 0.0 ||
      command_period_ms_ <= 0.0 || kp_ < 0.0 || kd_ < 0.0 ||
      joint_limit_rad_ <= 0.0 ||
      joint_limit_rad_ > kPhysicalAnkleLimitRad ||
      max_tracking_error_rad_ <= 0.0 || feedback_timeout_sec_ <= 0.0)
    {
      throw std::runtime_error("invalid sweep timing, gain, or safety parameter");
    }
  }

  void configure_side_indices()
  {
    if (side_ == "left") {
      pitch_index_ = kLeftAnklePitch;
      roll_index_ = kLeftAnkleRoll;
    } else {
      pitch_index_ = kRightAnklePitch;
      roll_index_ = kRightAnkleRoll;
    }
  }

  void on_feedback(const JointState & msg)
  {
    std::array<double, kJointCount> positions{};

    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
      const auto it = std::find(
        msg.name.begin(), msg.name.end(), joint_names_[joint]);
      if (it == msg.name.end()) {
        return;
      }
      const std::size_t message_index =
        static_cast<std::size_t>(it - msg.name.begin());
      if (message_index >= msg.position.size() ||
        !std::isfinite(msg.position[message_index]))
      {
        return;
      }
      positions[joint] = msg.position[message_index];
    }

    latest_positions_ = positions;
    last_feedback_time_ = steady_clock_.now();
    have_feedback_ = true;

    if (!have_baseline_) {
      baseline_positions_ = positions;
      const double pitch_margin =
        axis_ == "pitch" ? amplitude_rad_ : 0.0;
      const double roll_margin =
        axis_ == "roll" ? amplitude_rad_ : 0.0;
      if (
        std::fabs(baseline_positions_[pitch_index_]) + pitch_margin >
          joint_limit_rad_ ||
        std::fabs(baseline_positions_[roll_index_]) + roll_margin >
          joint_limit_rad_)
      {
        RCLCPP_ERROR(
          get_logger(),
          "Selected %s ankle baseline is too close to its limit: "
          "pitch=%.6f roll=%.6f limit=%.6f",
          side_.c_str(),
          baseline_positions_[pitch_index_],
          baseline_positions_[roll_index_],
          joint_limit_rad_);
        return;
      }
      have_baseline_ = true;
      start_time_ = steady_clock_.now();
      RCLCPP_INFO(
        get_logger(),
        "Captured %s ankle baseline: pitch=%.6f roll=%.6f",
        side_.c_str(),
        baseline_positions_[pitch_index_],
        baseline_positions_[roll_index_]);
    }
  }

  double elapsed_sec()
  {
    return (steady_clock_.now() - start_time_).seconds();
  }

double ramp_scale(double elapsed) const
  {
    if (ramp_time_sec_ <= 0.0) {
      return 1.0;
    }
    const double scale = elapsed / ramp_time_sec_;
    return std::max(0.0, std::min(scale, 1.0));
  }

  JointMotor make_command(
    double pitch_target,
    double roll_target,
    double pitch_velocity,
    double roll_velocity) const
  {
    JointMotor command;
    command.header.stamp = now();
    command.header.frame_id = "ankle_parallel_sweep_probe";
    command.joint_names = joint_names_;
    const double unset = std::numeric_limits<double>::quiet_NaN();
    command.position.fill(unset);
    command.velocity.fill(unset);
    command.effort.fill(unset);
    command.kp.fill(unset);
    command.kd.fill(unset);

    command.position[pitch_index_] = pitch_target;
    command.position[roll_index_] = roll_target;
    command.velocity[pitch_index_] = pitch_velocity;
    command.velocity[roll_index_] = roll_velocity;
    command.effort[pitch_index_] = 0.0f;
    command.effort[roll_index_] = 0.0f;
    command.kp[pitch_index_] = kp_;
    command.kp[roll_index_] = kp_;
    command.kd[pitch_index_] = kd_;
    command.kd[roll_index_] = kd_;
    return command;
  }

  void publish_enable()
  {
    if (!send_enable_) {
      return;
    }
    std_msgs::msg::Bool msg;
    msg.data = true;
    enable_pub_->publish(msg);
  }

  void write_csv(
    double elapsed,
    double target_pitch,
    double target_roll,
    double measured_pitch,
    double measured_roll)
  {
    if (!csv_) {
      return;
    }
    csv_ << elapsed << "," << target_pitch << "," << target_roll << ","
         << measured_pitch << "," << measured_roll << ","
         << target_pitch - measured_pitch << ","
         << target_roll - measured_roll << "\n";
  }

  void on_timer()
  {
    if (!have_feedback_ || !have_baseline_) {
      return;
    }

    if (!armed_) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "armed=false: sweep is monitoring only and will not publish commands.");
      return;
    }

    if ((steady_clock_.now() - last_feedback_time_).seconds() > feedback_timeout_sec_) {
      if (!aborted_) {
        RCLCPP_ERROR(
          get_logger(), "Feedback timeout; returning the selected ankle to baseline.");
        aborted_ = true;
      }
      publish_enable();
      command_pub_->publish(make_command(
        baseline_positions_[pitch_index_],
        baseline_positions_[roll_index_],
        0.0,
        0.0));
      return;
    }

    double elapsed = elapsed_sec();
    const bool completed = elapsed >= duration_sec_;
    const double phase = 2.0 * kPi * frequency_hz_ * std::min(elapsed, duration_sec_);
    const double scale = completed ? 0.0 : ramp_scale(elapsed);
    const double scale_rate =
      !completed && ramp_time_sec_ > 0.0 && elapsed < ramp_time_sec_
        ? 1.0 / ramp_time_sec_
        : 0.0;
    const double wave = scale * amplitude_rad_ * std::sin(phase);
    const double wave_velocity = completed ? 0.0 :
      amplitude_rad_ * (
        scale_rate * std::sin(phase) +
        scale * 2.0 * kPi * frequency_hz_ * std::cos(phase));

    double target_pitch = baseline_positions_[pitch_index_];
    double target_roll = baseline_positions_[roll_index_];
    double target_pitch_velocity = 0.0;
    double target_roll_velocity = 0.0;
    if (axis_ == "pitch") {
      target_pitch += wave;
      target_pitch_velocity = wave_velocity;
    } else {
      target_roll += wave;
      target_roll_velocity = wave_velocity;
    }

    const double measured_pitch = latest_positions_[pitch_index_];
    const double measured_roll = latest_positions_[roll_index_];
    const double pitch_error = std::fabs(target_pitch - measured_pitch);
    const double roll_error = std::fabs(target_roll - measured_roll);
    const double error = std::max(pitch_error, roll_error);
    if (
      !completed && !aborted_ && elapsed > ramp_time_sec_ &&
      error > max_tracking_error_rad_)
    {
      RCLCPP_ERROR(
        get_logger(),
        "Ankle tracking error exceeds limit: pitch=%.6f roll=%.6f limit=%.6f. "
        "Returning to baseline.",
        pitch_error, roll_error, max_tracking_error_rad_);
      aborted_ = true;
    }

    if (aborted_) {
      target_pitch = baseline_positions_[pitch_index_];
      target_roll = baseline_positions_[roll_index_];
      target_pitch_velocity = 0.0;
      target_roll_velocity = 0.0;
    }

    publish_enable();
    command_pub_->publish(make_command(
      target_pitch, target_roll, target_pitch_velocity, target_roll_velocity));
    write_csv(
      elapsed, target_pitch, target_roll, measured_pitch, measured_roll);

    if (completed && !completed_reported_) {
      RCLCPP_INFO(
        get_logger(),
        "Sweep complete. Holding the selected %s ankle at its captured baseline.",
        side_.c_str());
      completed_reported_ = true;
    }
  }

  void open_csv()
  {
    if (csv_path_.empty()) {
      return;
    }
    csv_.open(csv_path_, std::ios::out | std::ios::trunc);
    if (!csv_) {
      throw std::runtime_error("failed to open csv_path");
    }
    csv_.precision(9);
    csv_ << "elapsed_sec,target_pitch_rad,target_roll_rad,"
         << "measured_pitch_rad,measured_roll_rad,"
         << "pitch_error_rad,roll_error_rad\n";
  }

  std::string command_topic_;
  std::string feedback_topic_;
  std::string enable_topic_;
  std::string side_;
  std::string axis_;
  std::string csv_path_;
  bool armed_{false};
  bool send_enable_{false};
  bool have_feedback_{false};
  bool have_baseline_{false};
  bool aborted_{false};
  bool completed_reported_{false};
  double amplitude_rad_{0.01};
  double frequency_hz_{0.1};
  double ramp_time_sec_{2.0};
  double duration_sec_{10.0};
  double command_period_ms_{5.0};
  double kp_{10.0};
  double kd_{1.0};
  double joint_limit_rad_{0.70};
  double max_tracking_error_rad_{0.15};
  double feedback_timeout_sec_{0.1};
  std::size_t pitch_index_{kLeftAnklePitch};
  std::size_t roll_index_{kLeftAnkleRoll};
  std::array<std::string, kJointCount> joint_names_{};
  rclcpp::Clock steady_clock_{RCL_STEADY_TIME};
  rclcpp::Time start_time_{0, 0, RCL_STEADY_TIME};
  rclcpp::Time last_feedback_time_{0, 0, RCL_STEADY_TIME};
  std::array<double, kJointCount> baseline_positions_{};
  std::array<double, kJointCount> latest_positions_{};
  std::ofstream csv_;
  rclcpp::Publisher<JointMotor>::SharedPtr command_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr enable_pub_;
  rclcpp::Subscription<JointState>::SharedPtr feedback_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<AnkleParallelSweepProbe>());
  } catch (const std::exception & exception) {
    std::fprintf(stderr, "ankle_parallel_sweep_probe failed: %s\n", exception.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
