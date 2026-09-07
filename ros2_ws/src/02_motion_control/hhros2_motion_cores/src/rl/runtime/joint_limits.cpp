#include "hhros2_motion_cores/rl/runtime/joint_limits.hpp"

#include <algorithm>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace hhros2_motion_cores::rl_runtime {

JointLimitTable JointLimitTable::LoadFromYaml(const std::string& path)
{
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::Exception& e) {
    throw std::runtime_error(
        "Failed to load joint limits '" + path + "': " + e.what());
  }

  const auto limits_node = root["joint_limits"];
  if (!limits_node || !limits_node.IsMap()) {
    throw std::runtime_error(
        "joint_limits yaml must contain a map named 'joint_limits'.");
  }

  JointLimitTable table;
  for (const auto& item : limits_node) {
    const auto joint_name = item.first.as<std::string>();
    const auto value = item.second;

    JointLimit limit;
    if (value["has_position_limits"]) {
      limit.has_position_limits = value["has_position_limits"].as<bool>();
    }
    if (value["min_position"]) {
      limit.min_position = value["min_position"].as<double>();
    }
    if (value["max_position"]) {
      limit.max_position = value["max_position"].as<double>();
    }

    table.SetLimit(joint_name, limit);
  }

  return table;
}

bool JointLimitTable::Empty() const
{
  return limits_.empty();
}

void JointLimitTable::SetLimit(
    const std::string& joint_name,
    const JointLimit& limit)
{
  limits_[joint_name] = limit;
}

std::optional<JointLimit> JointLimitTable::Find(
    const std::string& joint_name) const
{
  const auto it = limits_.find(joint_name);
  if (it == limits_.end()) {
    return std::nullopt;
  }
  return it->second;
}

double JointLimitTable::ClampPosition(
    const std::string& joint_name,
    double position) const
{
  const auto limit = Find(joint_name);
  if (!limit || !limit->has_position_limits) {
    return position;
  }
  return std::clamp(position, limit->min_position, limit->max_position);
}

std::vector<std::optional<JointLimit>> BuildOrderedJointLimits(
    const std::vector<std::string>& joint_names,
    const JointLimitTable& limits)
{
  std::vector<std::optional<JointLimit>> ordered_limits;
  ordered_limits.reserve(joint_names.size());
  for (const auto& joint_name : joint_names) {
    ordered_limits.push_back(limits.Find(joint_name));
  }
  return ordered_limits;
}

double ClampPosition(
    const std::vector<std::optional<JointLimit>>& ordered_limits,
    std::size_t index,
    double position)
{
  if (index >= ordered_limits.size()) {
    return position;
  }
  const auto& limit = ordered_limits[index];
  if (!limit || !limit->has_position_limits) {
    return position;
  }
  return std::clamp(position, limit->min_position, limit->max_position);
}

}  // namespace hhros2_motion_cores::rl_runtime
