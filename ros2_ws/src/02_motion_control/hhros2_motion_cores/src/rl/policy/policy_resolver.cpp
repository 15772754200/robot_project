#include "hhros2_motion_cores/rl/policy/policy_resolver.hpp"

#include <filesystem>

namespace hhros2_motion_cores::rl_policy {

std::string ResolvePolicyPath(
    const std::string& explicit_policy_path,
    const std::string& explicit_model_dir,
    const std::string& config_path,
    const rl_observation::ObservationConfig& config)
{
  namespace fs = std::filesystem;

  if (!explicit_policy_path.empty()) {
    return explicit_policy_path;
  }
  if (config.model_name.empty()) {
    return {};
  }

  fs::path model_name{config.model_name};
  if (model_name.is_absolute()) {
    return model_name.string();
  }

  fs::path model_dir = !explicit_model_dir.empty()
      ? fs::path(explicit_model_dir)
      : fs::path(config.model_dir);

  if (model_dir.empty()) {
    return model_name.string();
  }

  if (model_dir.is_relative() && !config_path.empty()) {
    fs::path package_root = fs::path(config_path).parent_path();
    while (!package_root.empty() &&
           package_root.filename() != "hhros2_motion_cores") {
      package_root = package_root.parent_path();
    }
    if (!package_root.empty()) {
      model_dir = package_root / model_dir;
    }
  }

  return (model_dir / model_name).string();
}

}  // namespace hhros2_motion_cores::rl_policy
