#include "hhros2_motion_cores/core/rl_policy_core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "hhros2_motion_cores/rl/policy/policy_resolver.hpp"
#include "hhros2_motion_cores/rl/policy/policy_utils.hpp"
#include "rclcpp_components/register_node_macro.hpp"

namespace hhros2_motion_cores
{
namespace
{

constexpr std::size_t kMissingJointStateIndex =
    std::numeric_limits<std::size_t>::max();

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

bool HasJointFeedbackIndex(
    const std::vector<double> & platform_pos,
    const std::vector<double> & platform_vel,
    int index)
{
    return index >= 0 &&
        static_cast<std::size_t>(index) < platform_pos.size() &&
        static_cast<std::size_t>(index) < platform_vel.size() &&
        std::isfinite(platform_pos[static_cast<std::size_t>(index)]) &&
        std::isfinite(platform_vel[static_cast<std::size_t>(index)]);
}

bool HasValidBaseOrientation(const rl_runtime::RuntimeInput & input)
{
    const auto & q = input.base_quat_xyzw;
    if (!std::all_of(q.begin(), q.end(), [](double value) {
            return std::isfinite(value);
        }))
    {
        return false;
    }
    const double norm_sq =
        q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    return norm_sq > 0.25 && norm_sq < 2.25;
}

bool FindJointStateIndex(
    const sensor_msgs::msg::JointState & msg,
    const std::string & joint_name,
    std::size_t & index)
{
    for (std::size_t i = 0; i < msg.name.size(); ++i)
    {
        if (msg.name[i] == joint_name)
        {
            index = i;
            return true;
        }
    }
    return false;
}

bool BuildJointStateIndexCache(
    const sensor_msgs::msg::JointState & msg,
    const std::vector<std::string> & configured_joints,
    std::vector<std::size_t> & indices)
{
    indices.assign(configured_joints.size(), kMissingJointStateIndex);
    bool complete = true;
    for (std::size_t platform_index = 0;
        platform_index < configured_joints.size(); ++platform_index)
    {
        std::size_t msg_index = 0;
        if (FindJointStateIndex(
                msg, configured_joints[platform_index], msg_index))
        {
            indices[platform_index] = msg_index;
        }
        else
        {
            complete = false;
        }
    }
    return complete;
}

}  // namespace

RlPolicyCore::RlPolicyCore(
    const std::string & node_name,
    const rclcpp::NodeOptions & options)
: MotionCoreBase(node_name, options)
{
    policy_path_ = declare_parameter<std::string>("policy_path", "");
    observation_config_path_ =
        declare_parameter<std::string>("observation_config_path", "");
    policy_model_dir_ =
        declare_parameter<std::string>("policy_model_dir", "");
    feedback_topic_ =
        declare_parameter<std::string>("feedback_topic", "/joint_states");
    joint_limits_path_ =
        declare_parameter<std::string>(
            "joint_limits_path",
            "src/00_infrastructure/hhros2_description/config/joint_limits.yaml");
    kp_ = declare_parameter<double>("default_kp", 40.0);
    kd_ = declare_parameter<double>("default_kd", 1.0);
    damping_kd_ = declare_parameter<double>("damping_kd", 2.0);
    prepare_duration_sec_ =
        declare_parameter<double>("prepare_duration_sec", 0.0);
    default_pose_ =
        declare_parameter<std::vector<double>>("default_pose", std::vector<double>{});
    load_observation_config(observation_config_path_);
    load_joint_limits(joint_limits_path_);
    joint_pos_.assign(observation_config_.num_joints, 0.0f);
    joint_vel_.assign(observation_config_.num_joints, 0.0f);
    policy_start_time_ = now();
    joint_feedback_sub_ =
        create_subscription<sensor_msgs::msg::JointState>(
            feedback_topic_, rclcpp::QoS(1),
            std::bind(&RlPolicyCore::on_joint_feedback, this, std::placeholders::_1),
            subscription_options());
    policy_loaded_ =
        load_policy(rl_policy::ResolvePolicyPath(
            policy_path_,
            policy_model_dir_,
            observation_config_path_,
            observation_config_));

    // 读取基座状态数据源参数
    base_state_source_ = declare_parameter<std::string>("base_state_source", "estimator");

    if (base_state_source_ == "mujoco") {
        RCLCPP_INFO(get_logger(), "Base state source: MuJoCo pose from /mujoco/base_pose");
        base_pose_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "/mujoco/base_pose", rclcpp::QoS(1),
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                latest_base_pose_ = msg;
            },
            subscription_options());
    } else {
        RCLCPP_INFO(get_logger(), "Base state source: state_estimator (default)");
    }
}

bool RlPolicyCore::load_joint_limits(const std::string & path)
{
    if (path.empty())
    {
        ordered_joint_limits_.clear();
        return true;
    }

    try
    {
        joint_limit_table_ = rl_runtime::JointLimitTable::LoadFromYaml(path);
        ordered_joint_limits_ =
            rl_runtime::BuildOrderedJointLimits(joint_names(), joint_limit_table_);
    }
    catch (const std::exception & e)
    {
        RCLCPP_WARN(
            get_logger(), "Failed to load joint limits '%s': %s",
            path.c_str(), e.what());
        ordered_joint_limits_.clear();
        return false;
    }

    RCLCPP_INFO(
        get_logger(), "Loaded joint limits for %zu configured joints.",
        ordered_joint_limits_.size());
    return true;
}

bool RlPolicyCore::load_observation_config(const std::string & path)
{
    try
    {
        observation_config_ = path.empty()
            ? rl_observation::ObservationConfig{}
            : rl_observation::LoadObservationConfigFromYaml(path);
        runtime_.reset();
    }
    catch (const std::exception & e)
    {
        RCLCPP_ERROR(
            get_logger(), "Failed to load observation config '%s': %s",
            path.c_str(), e.what());
        observation_config_ = rl_observation::ObservationConfig{};
        runtime_.reset();
        return false;
    }

    RCLCPP_INFO(
        get_logger(), "Observation config ready: %zu items, %zu joints.",
        observation_config_.observations.size(), observation_config_.num_joints);
    return true;
}

void RlPolicyCore::on_joint_feedback(
    const sensor_msgs::msg::JointState::SharedPtr msg)
{
    if (!is_active())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(joint_feedback_mutex_);
    const auto current_mode_generation = mode_generation();
    const std::size_t n = observation_config_.num_joints;
    if (joint_pos_.size() != n) {
        joint_pos_.assign(n, 0.0f);
        joint_vel_.assign(n, 0.0f);
    }

    const auto & configured_joints = joint_names();
    if (!joint_state_indices_ready_)
    {
        joint_state_indices_ready_ =
            BuildJointStateIndexCache(
                *msg, configured_joints, joint_state_indices_);
        if (!joint_state_indices_ready_)
        {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), 2000,
                "JointState feedback on '%s' is missing configured joint names; waiting for a complete name set.",
                feedback_topic_.c_str());
        }
    }

    if (platform_pos_.size() != configured_joints.size())
    {
        platform_pos_.resize(configured_joints.size());
        platform_vel_.resize(configured_joints.size());
    }
    std::fill(
        platform_pos_.begin(), platform_pos_.end(),
        std::numeric_limits<double>::quiet_NaN());
    std::fill(
        platform_vel_.begin(), platform_vel_.end(),
        std::numeric_limits<double>::quiet_NaN());

    for (std::size_t platform_index = 0;
        platform_index < configured_joints.size(); ++platform_index)
    {
        if (platform_index >= joint_state_indices_.size())
        {
            continue;
        }
        const std::size_t msg_index = joint_state_indices_[platform_index];
        if (msg_index == kMissingJointStateIndex)
        {
            continue;
        }
        if (msg_index < msg->position.size() &&
            std::isfinite(msg->position[msg_index]))
        {
            platform_pos_[platform_index] = msg->position[msg_index];
        }
        if (msg_index < msg->velocity.size() &&
            std::isfinite(msg->velocity[msg_index]))
        {
            platform_vel_[platform_index] = msg->velocity[msg_index];
        }
    }

    std::size_t updated_policy_joints = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (i >= observation_config_.state_joint_mapping.size())
        {
            continue;
        }
        const int source_index = observation_config_.state_joint_mapping[i];
        if (!HasJointFeedbackIndex(platform_pos_, platform_vel_, source_index))
        {
            continue;
        }
        const auto platform_index = static_cast<std::size_t>(source_index);
        joint_pos_[i] = static_cast<float>(platform_pos_[platform_index]);
        joint_vel_[i] = static_cast<float>(platform_vel_[platform_index]);
        ++updated_policy_joints;
    }

    if (updated_policy_joints == n)
    {
        have_joint_feedback_ = true;
        feedback_mode_generation_ = current_mode_generation;
        return;
    }

    RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "JointState feedback on '%s' updated %zu/%zu policy joints; waiting for a full policy feedback frame.",
        feedback_topic_.c_str(), updated_policy_joints, n);
}

bool RlPolicyCore::load_policy(const std::string & path)
{
    if (path.empty())
    {
        RCLCPP_WARN(get_logger(), "policy_path empty; holding default pose.");
        return false;
    }

    try
    {
        auto backend = rl_policy::CreateInferenceBackendForPolicyPath(path);
        if (!backend->LoadModel(path))
        {
            RCLCPP_WARN(
                get_logger(), "Failed to load policy '%s'; holding default pose.",
                path.c_str());
            runtime_.reset();
            return false;
        }
        runtime_ =
            std::make_unique<rl_runtime::RlRuntime>(
                observation_config_, std::move(backend), ordered_joint_limits_);
    }
    catch (const std::exception & e)
    {
        RCLCPP_ERROR(
            get_logger(), "Failed to create inference backend for '%s': %s",
            path.c_str(), e.what());
        runtime_.reset();
        return false;
    }

    RCLCPP_INFO(get_logger(), "Loaded policy '%s'.", path.c_str());
    return true;
}

// rl_runtime::RuntimeInput RlPolicyCore::make_runtime_input(
//     const geometry_msgs::msg::Twist & cmd_vel,
//     const hhros2_interfaces::msg::BaseState & state) const
// {
//     rl_runtime::RuntimeInput input;
//     input.base_quat_xyzw = {
//         state.pose.orientation.x,
//         state.pose.orientation.y,
//         state.pose.orientation.z,
//         state.pose.orientation.w};
//     input.base_ang_vel_world = {
//         state.twist.angular.x,
//         state.twist.angular.y,
//         state.twist.angular.z};
//     input.command = {
//         cmd_vel.linear.x,
//         cmd_vel.linear.y,
//         cmd_vel.angular.z};
//     input.joint_pos = joint_pos_;
//     input.joint_vel = joint_vel_;
//     input.time_sec = (now() - policy_start_time_).seconds();
//     return input;
// }

rl_runtime::RuntimeInput RlPolicyCore::make_runtime_input(
    const geometry_msgs::msg::Twist & cmd_vel,
    const hhros2_interfaces::msg::BaseState & state) const
{
    rl_runtime::RuntimeInput input;

    if (base_state_source_ == "mujoco") {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (latest_base_pose_) {
            input.base_quat_xyzw = {
                latest_base_pose_->pose.orientation.x,
                latest_base_pose_->pose.orientation.y,
                latest_base_pose_->pose.orientation.z,
                latest_base_pose_->pose.orientation.w
            };
        } else {
            // 回退到 state_estimator
            input.base_quat_xyzw = {
                state.pose.orientation.x,
                state.pose.orientation.y,
                state.pose.orientation.z,
                state.pose.orientation.w
            };
            RCLCPP_WARN(get_logger(), "MuJoCo base pose not received; using state_estimator fallback");
        }
        // The estimator forwards IMU angular velocity in the body frame.
        input.base_ang_vel_body = {
            state.twist.angular.x,
            state.twist.angular.y,
            state.twist.angular.z
        };
    } else {
        // 硬件模式：完全使用 state_estimator
        input.base_quat_xyzw = {
            state.pose.orientation.x,
            state.pose.orientation.y,
            state.pose.orientation.z,
            state.pose.orientation.w
        };
        input.base_ang_vel_body = {
            state.twist.angular.x,
            state.twist.angular.y,
            state.twist.angular.z
        };
    }

    input.command = {
        cmd_vel.linear.x,
        cmd_vel.linear.y,
        cmd_vel.angular.z
    };
    input.joint_pos = joint_pos_;
    input.joint_vel = joint_vel_;
    input.time_sec = (now() - policy_start_time_).seconds();
    return input;
}

void RlPolicyCore::fill_joint_motor(
    const rl_runtime::RuntimeOutput & runtime_output,
    hhros2_interfaces::msg::JointMotor & out) const
{
    const std::size_t n =
        std::min(out.position.size(), runtime_output.target_position.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        out.position[i] = runtime_output.target_position[i];
        out.velocity[i] = (i < runtime_output.target_velocity.size())
            ? runtime_output.target_velocity[i] : 0.0;
        out.effort[i] = (i < runtime_output.target_effort.size())
            ? runtime_output.target_effort[i] : 0.0;
        out.kp[i] = (i < runtime_output.kp.size()) ? runtime_output.kp[i] : kp_;
        out.kd[i] = (i < runtime_output.kd.size()) ? runtime_output.kd[i] : kd_;
    }
}

void RlPolicyCore::apply_default_command(
    hhros2_interfaces::msg::JointMotor & out) const
{
    rl_runtime::ActionMapper mapper(observation_config_);
    fill_joint_motor(
        mapper.DefaultOutput(default_pose_, kp_, kd_, ordered_joint_limits_),
        out);
}

void RlPolicyCore::apply_damping_command(
    hhros2_interfaces::msg::JointMotor & out) const
{
    const std::size_t n = std::min(n_joints(), out.position.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        out.position[i] =
            i < platform_pos_.size() && std::isfinite(platform_pos_[i])
            ? platform_pos_[i] : 0.0;
        out.velocity[i] = 0.0;
        out.effort[i] = 0.0;
        out.kp[i] = 0.0;
        out.kd[i] = damping_kd_;
    }
}

void RlPolicyCore::apply_prepare_command(
    double progress,
    hhros2_interfaces::msg::JointMotor & out) const
{
    apply_default_command(out);
    const double alpha = std::clamp(progress, 0.0, 1.0);
    const std::size_t n = std::min(
        prepare_start_position_.size(), out.position.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        if (std::isfinite(prepare_start_position_[i]))
        {
            out.position[i] =
                (1.0 - alpha) * prepare_start_position_[i] +
                alpha * out.position[i];
        }
        out.kp[i] *= alpha;
        out.kd[i] =
            (1.0 - alpha) * damping_kd_ + alpha * out.kd[i];
    }
}

bool RlPolicyCore::compute(
    const geometry_msgs::msg::Twist & cmd_vel,
    const hhros2_interfaces::msg::BaseState & state,
    hhros2_interfaces::msg::JointMotor & out)
{
    std::lock_guard<std::mutex> lock(joint_feedback_mutex_);
    if (!policy_loaded_ || !runtime_ || !runtime_->Ready())
    {
        apply_damping_command(out);
        return true;
    }
    if (!have_joint_feedback_ ||
        feedback_mode_generation_ != mode_generation())
    {
        RCLCPP_WARN_THROTTLE(
            get_logger(), *get_clock(), 2000,
            "No joint feedback on '%s'; holding default pose.",
            feedback_topic_.c_str());
        apply_damping_command(out);
        return true;
    }

    auto input = make_runtime_input(cmd_vel, state);
    if (!HasValidBaseOrientation(input))
    {
        RCLCPP_WARN_THROTTLE(
            get_logger(), *get_clock(), 2000,
            "No valid base orientation yet; holding default pose.");
        apply_damping_command(out);
        return true;
    }

    const auto current_mode_generation = mode_generation();
    if (!runtime_mode_generation_ready_ ||
        runtime_mode_generation_ != current_mode_generation)
    {
        // === 切换模型时打印信息 ===
        if (!runtime_mode_generation_ready_)
        {
            RCLCPP_INFO(
                get_logger(),
                "[Policy Switch] Node '%s' first activation, "
                "mode_generation=%u, policy_path='%s'",
                get_name(),
                static_cast<unsigned>(current_mode_generation),
                policy_path_.c_str());
        }
        else
        {
            RCLCPP_INFO(
                get_logger(),
                "[Policy Switch] Node '%s' switching mode: "
                "mode_generation %u -> %u, policy_path='%s'",
                get_name(),
                static_cast<unsigned>(runtime_mode_generation_),
                static_cast<unsigned>(current_mode_generation),
                policy_path_.c_str());
        }
        // === 打印结束 ===
        runtime_->Reset();
        policy_start_time_ = now();
        prepare_start_time_ = policy_start_time_;
        prepare_start_position_ = platform_pos_;
        preparing_ = prepare_duration_sec_ > 0.0;
        runtime_mode_generation_ = current_mode_generation;
        runtime_mode_generation_ready_ = true;
        input.time_sec = 0.0;
    }

    if (preparing_)
    {
        const double elapsed = (now() - prepare_start_time_).seconds();
        if (elapsed < prepare_duration_sec_)
        {
            apply_prepare_command(elapsed / prepare_duration_sec_, out);
            return true;
        }
        runtime_->Reset();
        policy_start_time_ = now();
        preparing_ = false;
        input = make_runtime_input(cmd_vel, state);
        input.time_sec = 0.0;
    }

    try
    {
        const auto runtime_output =
            runtime_->Step(input, default_pose_, kp_, kd_);
        fill_joint_motor(runtime_output, out);
    }
    catch (const std::exception & e)
    {
        RCLCPP_WARN_THROTTLE(
            get_logger(), *get_clock(), 2000,
            "RL policy inference failed: %s; holding default pose.", e.what());
        apply_damping_command(out);
    }
    return true;
}

RlStandCore::RlStandCore(const rclcpp::NodeOptions & options)
: RlPolicyCore("rl_stand_core", options)
{
}

RlWalkCore::RlWalkCore(const rclcpp::NodeOptions & options)
: RlPolicyCore("rl_walk_core", options)
{
}

RlRunCore::RlRunCore(const rclcpp::NodeOptions & options)
: RlPolicyCore("rl_run_core", options)
{
}
	
}  // namespace hhros2_motion_cores

RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::RlStandCore)
RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::RlWalkCore)
RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::RlRunCore)
