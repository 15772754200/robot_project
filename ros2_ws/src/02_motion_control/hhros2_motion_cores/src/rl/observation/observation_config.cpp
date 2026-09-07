#include "hhros2_motion_cores/rl/observation/observation_config.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace hhros2_motion_cores::rl_observation {
namespace {

template <typename T>
void ReadRequired(const YAML::Node& node, const char* key, T& value)
{
  if (!node[key]) {
    throw std::runtime_error(std::string("Missing required RL config field: ") + key);
  }
  value = node[key].as<T>();
}

template <typename T>
void ReadOptional(const YAML::Node& node, const char* key, T& value)
{
  if (node[key]) {
    value = node[key].as<T>();
  }
}

/* 把你yaml里的observation名字统一转化成你C++ Builder能识别的标准名字 */
std::string NormalizeObservationName(const std::string& name)
{
  static const std::unordered_map<std::string, std::string> aliases{
    {"ang_vel", "base_ang_vel"},  // 字符串对应一下
    {"gravity_vec", "projected_gravity"},
    {"commands", "command"},
    {"dof_pos", "joint_pos"},
    {"dof_vel", "joint_vel"},
    {"actions", "last_action"},
    {"phase", "phase"},
    {"period", "period"},
  };

  auto normalized = name;
  // 把normalized里面每个字符串都转成小写，然后写回normalized里面
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  const auto it = aliases.find(normalized); // 这个就是把它找一下
  if (it == aliases.end()) {
    throw std::runtime_error("Unsupported observation item in legacy config: " + name);
  }
  return it->second;  // 然后返回第二个
}

std::vector<std::string> NormalizeObservations(
    const std::vector<std::string>& observations)
{
  std::vector<std::string> normalized;
  normalized.reserve(observations.size());  // 预留大小
  for (const auto& name : observations) {
    normalized.push_back(NormalizeObservationName(name)); // 挨个找到builder对应的字符串
  }
  return normalized;
}

YAML::Node SelectLegacyPolicyNode(const YAML::Node& root)
{
  if (!root || !root.IsMap() || root.size() != 1) { // yaml最外层必须要刚好只有一个配置块
    throw std::runtime_error(
      "RL config must contain exactly one top-level policy block, e.g. rl/yd_rl_walk.");
  }
  return root.begin()->second;
}

}  // namespace

ObservationConfig LoadObservationConfigFromYaml(const std::string& path)  // 从yaml配置文件中读取RL observation/action配置的实现
{
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);  // 读取yaml文件
  } catch (const YAML::Exception& e) {
    throw std::runtime_error(
      "Failed to load observation config '" + path + "': " + e.what());
  }

  root = SelectLegacyPolicyNode(root);
  ObservationConfig config;

  // 这几个字段必须要存在，没有就报错了
  ReadRequired(root, "model_name", config.model_name);
  ReadRequired(root, "num_observations", config.num_observations);
  ReadRequired(root, "observations", config.observations);
  ReadRequired(root, "observation_stack", config.observation_stack);
  ReadRequired(root, "num_of_dofs", config.num_joints);
  ReadRequired(root, "default_dof_pos", config.default_joint_pos);
  ReadRequired(root, "action_scale", config.action_scale);
  ReadRequired(root, "joint_mapping", config.joint_mapping);
  ReadRequired(root, "state_joint_mapping", config.state_joint_mapping);

  config.observations = NormalizeObservations(config.observations); // 把旧配置里的名字转化成你代码的标准名字

  ReadOptional(root, "model_dir", config.model_dir);
  ReadOptional(root, "observation_stack_mode", config.observation_stack_mode);
  ReadOptional(root, "cycle_time", config.cycle_time);
  ReadOptional(root, "clip_obs", config.clip_obs);
  ReadOptional(root, "ang_vel_scale", config.angl_vel_scale);
  ReadOptional(root, "dof_pos_scale", config.joint_pos_scale);
  ReadOptional(root, "dof_vel_scale", config.joint_vel_scale);
  ReadOptional(root, "commands_scale", config.command_scales);
  ReadOptional(root, "clip_actions_lower", config.clip_actions_lower);
  ReadOptional(root, "clip_actions_upper", config.clip_actions_upper);
  ReadOptional(root, "kp", config.kp);
  ReadOptional(root, "kd", config.kd);
  ReadOptional(root, "default_dof_pos_others", config.default_joint_pos_others);
  ReadOptional(root, "joint_mapping_others", config.joint_mapping_others);

  if (!config.default_joint_pos.empty() &&
      config.default_joint_pos.size() != config.num_joints) {
    throw std::runtime_error(
      "default_joint_pos size must match num_joints in observation config.");
  }
  if (config.observations.empty()) {
    throw std::runtime_error("observations must not be empty.");
  }
  if (config.observation_stack == 0) {
    throw std::runtime_error("observation_stack must be greater than zero.");
  }
  if (config.observation_stack_mode != 0 && config.observation_stack_mode != 1) {
    throw std::runtime_error("observation_stack_mode must be 0 or 1.");
  }
  if (!config.command_scales.empty() && config.command_scales.size() != 3) {
    throw std::runtime_error("commands_scale must contain exactly 3 values.");
  }
  if (config.action_scale.size() != config.num_joints) {
    throw std::runtime_error("action_scale size must match num_of_dofs.");
  }
  if (config.joint_mapping.size() != config.num_joints) {
    throw std::runtime_error("joint_mapping size must match num_of_dofs.");
  }
  if (config.state_joint_mapping.size() != config.num_joints) {
    throw std::runtime_error("state_joint_mapping size must match num_of_dofs.");
  }
  if (!config.clip_actions_lower.empty() &&
      config.clip_actions_lower.size() != config.num_joints) {
    throw std::runtime_error("clip_actions_lower size must match num_of_dofs.");
  }
  if (!config.clip_actions_upper.empty() &&
      config.clip_actions_upper.size() != config.num_joints) {
    throw std::runtime_error("clip_actions_upper size must match num_of_dofs.");
  }
  if (!config.default_joint_pos_others.empty() &&
      config.default_joint_pos_others.size() != config.joint_mapping_others.size()) {
    throw std::runtime_error(
      "default_dof_pos_others size must match joint_mapping_others size.");
  }
  return config;
}

}  // namespace hhros2_motion_cores::rl_observation
