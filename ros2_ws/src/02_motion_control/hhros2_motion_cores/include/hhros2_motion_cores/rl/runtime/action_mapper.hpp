#ifndef HHROS2_MOTION_CORES_RL_ACTION_MAPPER_HPP_
#define HHROS2_MOTION_CORES_RL_ACTION_MAPPER_HPP_

#include <cstddef>
#include <vector>

#include "hhros2_motion_cores/rl/observation/observation_config.hpp"
#include "hhros2_motion_cores/rl/runtime/joint_limits.hpp"
#include "hhros2_motion_cores/rl/runtime/runtime_types.hpp"

namespace hhros2_motion_cores::rl_runtime {

class ActionMapper {
 public:
  explicit ActionMapper(const rl_observation::ObservationConfig& config);

  RuntimeOutput DefaultOutput(
      const std::vector<double>& fallback_pose,
      double fallback_kp,
      double fallback_kd,
      const std::vector<std::optional<JointLimit>>& joint_limits = {}) const;

  RuntimeOutput Map(
      const std::vector<float>& action,
      const std::vector<double>& fallback_pose,
      double fallback_kp,
      double fallback_kd,
      const std::vector<std::optional<JointLimit>>& joint_limits = {}) const;

 private:
  std::size_t OutputSize() const;

  rl_observation::ObservationConfig config_;
};

}  // namespace hhros2_motion_cores::rl_runtime

#endif  // HHROS2_MOTION_CORES_RL_ACTION_MAPPER_HPP_
