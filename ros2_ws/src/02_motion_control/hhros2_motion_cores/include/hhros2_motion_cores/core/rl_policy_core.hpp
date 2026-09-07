#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <geometry_msgs/msg/pose_stamped.hpp>

#include "hhros2_motion_cores/core/motion_core_base.hpp"
#include "hhros2_motion_cores/rl/observation/observation_config.hpp"
#include "hhros2_motion_cores/rl/runtime/rl_runtime.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace hhros2_motion_cores
{

class RlPolicyCore : public MotionCoreBase
{
protected:
    RlPolicyCore(
        const std::string & node_name,
        const rclcpp::NodeOptions & options);

    bool compute(
        const geometry_msgs::msg::Twist & cmd_vel,
        const hhros2_interfaces::msg::BaseState & state,
        hhros2_interfaces::msg::JointMotor & out) override;

private:
    bool load_policy(const std::string & path);
    bool load_observation_config(const std::string & path);
    bool load_joint_limits(const std::string & path);

    void on_joint_feedback(
        const sensor_msgs::msg::JointState::SharedPtr msg);

    rl_runtime::RuntimeInput make_runtime_input(
        const geometry_msgs::msg::Twist & cmd_vel,
        const hhros2_interfaces::msg::BaseState & state) const;

    void fill_joint_motor(
        const rl_runtime::RuntimeOutput & runtime_output,
        hhros2_interfaces::msg::JointMotor & out) const;

    void apply_default_command(
        hhros2_interfaces::msg::JointMotor & out) const;
    void apply_damping_command(
        hhros2_interfaces::msg::JointMotor & out) const;
    void apply_prepare_command(
        double progress,
        hhros2_interfaces::msg::JointMotor & out) const;

    std::string policy_path_;
    std::string observation_config_path_;
    std::string policy_model_dir_;
    std::string feedback_topic_;
    std::string joint_limits_path_;

    std::vector<double> default_pose_;
    double kp_ = 40.0;
    double kd_ = 1.0;
    double damping_kd_ = 2.0;
    double prepare_duration_sec_ = 0.0;

    bool policy_loaded_ = false;
    bool have_joint_feedback_ = false;
    std::uint64_t feedback_mode_generation_ = 0;
    std::uint64_t runtime_mode_generation_ = 0;
    bool runtime_mode_generation_ready_ = false;
    rclcpp::Time policy_start_time_;
    rclcpp::Time prepare_start_time_;
    std::vector<double> prepare_start_position_;
    bool preparing_ = false;

    rl_observation::ObservationConfig observation_config_;
    rl_runtime::JointLimitTable joint_limit_table_;
    std::vector<std::optional<rl_runtime::JointLimit>> ordered_joint_limits_;
    std::unique_ptr<rl_runtime::RlRuntime> runtime_;

    std::vector<float> joint_pos_;
    std::vector<float> joint_vel_;
    std::vector<std::size_t> joint_state_indices_;
    std::vector<double> platform_pos_;
    std::vector<double> platform_vel_;
    bool joint_state_indices_ready_ = false;
    mutable std::mutex joint_feedback_mutex_;

    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr
        joint_feedback_sub_;

    // 基座状态数据源选择
    std::string base_state_source_ = "estimator";
    
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr base_pose_sub_;
    geometry_msgs::msg::PoseStamped::ConstSharedPtr latest_base_pose_;
    mutable std::mutex state_mutex_;  // 允许在 const 成员函数中锁定
};

class RlStandCore final : public RlPolicyCore
{
public:
    explicit RlStandCore(const rclcpp::NodeOptions & options);
};

class RlWalkCore final : public RlPolicyCore
{
public:
    explicit RlWalkCore(const rclcpp::NodeOptions & options);
};

class RlRunCore final : public RlPolicyCore
{
public:
    explicit RlRunCore(const rclcpp::NodeOptions & options);
};

}  // namespace hhros2_motion_cores
