#ifndef HHROS2_MOTION_CORES_RL_POLICY_RESOLVER_HPP_
#define HHROS2_MOTION_CORES_RL_POLICY_RESOLVER_HPP_

#include <string>

#include "hhros2_motion_cores/rl/observation/observation_config.hpp"

namespace hhros2_motion_cores::rl_policy {

std::string ResolvePolicyPath(
    const std::string& explicit_policy_path,
    const std::string& explicit_model_dir,
    const std::string& config_path,
    const rl_observation::ObservationConfig& config);

}  // namespace hhros2_motion_cores::rl_policy

#endif  // HHROS2_MOTION_CORES_RL_POLICY_RESOLVER_HPP_
