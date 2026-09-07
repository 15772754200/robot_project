#include "hhros2_motion_cores/rl/runtime/action_mapper.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hhros2_motion_cores::rl_runtime {
namespace {

constexpr std::size_t kDefaultJointMotorSize = 23;

void ResizeOutput(RuntimeOutput& output, std::size_t size)
{// 把输出量都resize成同样长度，并且全部初始化为0
  output.target_position.assign(size, 0.0);
  output.target_velocity.assign(size, 0.0);
  output.target_effort.assign(size, 0.0);
  output.kp.assign(size, 0.0);
  output.kd.assign(size, 0.0);
}

}  // namespace

ActionMapper::ActionMapper(const rl_observation::ObservationConfig& config)
: config_(config) // 接收一个配置，然后保存到成员变量
{
}

std::size_t ActionMapper::OutputSize() const  // 计算输出数组应该多大
{
  std::size_t size = kDefaultJointMotorSize;
  size = std::max(size, config_.kp.size());
  size = std::max(size, config_.kd.size()); // 三个变量取最大的

  for (const int index : config_.joint_mapping) { // joint_mapping表示rl模型输出的第i个action，要写到机器人输出数组的哪个逻辑关节位置
    if (index >= 0) {
      size = std::max(size, static_cast<std::size_t>(index) + 1);
    }
  }
  for (const int index : config_.joint_mapping_others) {  // 这个表示其他逻辑关节在输出数组的位置
    if (index >= 0) {
      size = std::max(size, static_cast<std::size_t>(index) + 1);
    }
  }
  return size;
}

RuntimeOutput ActionMapper::DefaultOutput(  // 生成默认输出
    const std::vector<double>& fallback_pose,
    double fallback_kp,
    double fallback_kd,
    const std::vector<std::optional<JointLimit>>& joint_limits) const
{
  RuntimeOutput output;
  const std::size_t out_size = OutputSize();  // 计算长度
  ResizeOutput(output, out_size);        // 同一长度

  for (std::size_t i = 0; i < out_size; ++i) {
    output.target_position[i] =
        ClampPosition(
            joint_limits, i,  // 如果fallback_pose有第i个只，就用它
            (i < fallback_pose.size()) ? fallback_pose[i] : 0.0);
    output.kp[i] = (i < config_.kp.size()) ? config_.kp[i] : fallback_kp;
    output.kd[i] = (i < config_.kd.size()) ? config_.kd[i] : fallback_kd;
  }

  for (std::size_t i = 0;
       i < config_.joint_mapping.size() &&
       i < config_.default_joint_pos.size();
       ++i) {
    const int target_index = config_.joint_mapping[i];
    if (target_index >= 0 &&
        static_cast<std::size_t>(target_index) < out_size) {
      // rl的第一个输出是默认初始姿态
      output.target_position[target_index] = config_.default_joint_pos[i];
      output.target_position[target_index] =
          ClampPosition(
              joint_limits,
              static_cast<std::size_t>(target_index),
              output.target_position[target_index]);
    }
  }

  for (std::size_t i = 0;
       i < config_.joint_mapping_others.size() &&
       i < config_.default_joint_pos_others.size();
       ++i) {
    const int target_index = config_.joint_mapping_others[i]; // 非rl关节的索引
    if (target_index >= 0 &&
        static_cast<std::size_t>(target_index) < out_size) {
      // 非rl关节索引按照配置更改
      output.target_position[target_index] =
          config_.default_joint_pos_others[i];
      output.target_position[target_index] =
          ClampPosition(
              joint_limits,
              static_cast<std::size_t>(target_index),
              output.target_position[target_index]);
    }
  }
  return output;
}
// 把policy action转成最终的控制输出
RuntimeOutput ActionMapper::Map(
    const std::vector<float>& action,
    const std::vector<double>& fallback_pose,
    double fallback_kp,
    double fallback_kd,
    const std::vector<std::optional<JointLimit>>& joint_limits) const
{
  const std::size_t n = config_.num_joints;
  if (action.size() < n) {
    throw std::runtime_error("Policy action size is smaller than num_of_dofs.");
  }

  // 先生成默认输出
  RuntimeOutput output =
      DefaultOutput(fallback_pose, fallback_kp, fallback_kd, joint_limits);
  output.raw_action.assign(action.begin(), action.begin() + n);
  output.clipped_action.assign(n, 0.0f);

  for (std::size_t i = 0; i < n; ++i) {
    float clipped_action = action[i];
    if (!std::isfinite(clipped_action)) {
      clipped_action = 0.0f;
    }
    if (!config_.clip_actions_lower.empty()) {
      clipped_action = std::max(clipped_action, config_.clip_actions_lower[i]);
    }
    if (!config_.clip_actions_upper.empty()) {
      clipped_action = std::min(clipped_action, config_.clip_actions_upper[i]);
    }
    output.clipped_action[i] = clipped_action;

    const int target_index = config_.joint_mapping[i];
    if (target_index < 0 ||
        static_cast<std::size_t>(target_index) >= output.target_position.size()) {
      continue;
    }

    output.target_position[target_index] =
        config_.default_joint_pos[i] + clipped_action * config_.action_scale[i];
    output.target_position[target_index] =
        ClampPosition(
            joint_limits,
            static_cast<std::size_t>(target_index),
            output.target_position[target_index]);
    output.target_velocity[target_index] = 0.0;
    output.target_effort[target_index] = 0.0;
  }

  return output;
}

}  // namespace hhros2_motion_cores::rl_runtime
