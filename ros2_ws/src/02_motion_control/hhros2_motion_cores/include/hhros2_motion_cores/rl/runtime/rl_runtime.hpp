#ifndef HHROS2_MOTION_CORES_RL_RUNTIME_HPP_
#define HHROS2_MOTION_CORES_RL_RUNTIME_HPP_

#include <memory>
#include <vector>

#include "hhros2_motion_cores/rl/inference/inference_backend.hpp"
#include "hhros2_motion_cores/rl/observation/observation_builder.hpp"
#include "hhros2_motion_cores/rl/observation/observation_config.hpp"
#include "hhros2_motion_cores/rl/runtime/action_mapper.hpp"
#include "hhros2_motion_cores/rl/runtime/runtime_types.hpp"

namespace hhros2_motion_cores::rl_runtime {

class RlRuntime {
 public:
  RlRuntime(
      const rl_observation::ObservationConfig& config,
      std::unique_ptr<rl_inference::InferenceBackend> backend,
      std::vector<std::optional<JointLimit>> joint_limits = {});

  RuntimeOutput Step(
      const RuntimeInput& input,
      const std::vector<double>& fallback_pose,
      double fallback_kp,
      double fallback_kd);

  RuntimeOutput DefaultOutput(
      const std::vector<double>& fallback_pose,
      double fallback_kp,
      double fallback_kd) const;

  void Reset();
  bool Ready() const;

 private:
  rl_observation::ObservationInput BuildObservationInput(
      const RuntimeInput& input) const;

  rl_observation::ObservationConfig config_;
  rl_observation::ObservationBuilder observation_builder_;
  ActionMapper action_mapper_;
  std::unique_ptr<rl_inference::InferenceBackend> backend_; // 推理后端
  std::vector<std::optional<JointLimit>> joint_limits_;
  std::vector<float> last_action_;
};

}  // namespace hhros2_motion_cores::rl_runtime

#endif  // HHROS2_MOTION_CORES_RL_RUNTIME_HPP_
