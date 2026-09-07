#include "hhros2_motion_cores/rl/runtime/rl_runtime.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "hhros2_motion_cores/rl/gait/gait_utils.hpp"
#include "hhros2_motion_cores/rl/math/math_utils.hpp"

namespace hhros2_motion_cores::rl_runtime {

RlRuntime::RlRuntime(
    const rl_observation::ObservationConfig& config,
    std::unique_ptr<rl_inference::InferenceBackend> backend,
    std::vector<std::optional<JointLimit>> joint_limits)
: config_(config),
  observation_builder_(config),
  action_mapper_(config),
  backend_(std::move(backend)),
  joint_limits_(std::move(joint_limits)),
  last_action_(config.num_joints, 0.0f)
{
}

bool RlRuntime::Ready() const
{
  return backend_ != nullptr;
}

void RlRuntime::Reset()
{
  observation_builder_.Reset();
  last_action_.assign(config_.num_joints, 0.0f);
}

RuntimeOutput RlRuntime::DefaultOutput(
    const std::vector<double>& fallback_pose,
    double fallback_kp,
    double fallback_kd) const
{
  return action_mapper_.DefaultOutput(
      fallback_pose, fallback_kp, fallback_kd, joint_limits_);
}

rl_observation::ObservationInput RlRuntime::BuildObservationInput(
    const RuntimeInput& input) const
{
  namespace math = rl_policy_math_utils;

  const auto projected_gravity = math::QuatRotateInverse(
      input.base_quat_xyzw, std::array<double, 3>{0.0, 0.0, -1.0});

  rl_observation::ObservationInput observation;
  observation.base_ang_vel = {
      static_cast<float>(input.base_ang_vel_body[0]),
      static_cast<float>(input.base_ang_vel_body[1]),
      static_cast<float>(input.base_ang_vel_body[2])};
  observation.projected_gravity = {
      static_cast<float>(projected_gravity[0]),
      static_cast<float>(projected_gravity[1]),
      static_cast<float>(projected_gravity[2])};
  observation.command = {
      static_cast<float>(input.command[0]),
      static_cast<float>(input.command[1]),
      static_cast<float>(input.command[2])};
  observation.joint_pos = input.joint_pos;
  observation.joint_vel = input.joint_vel;
  observation.last_action = last_action_;

  const auto gait_phase =
      rl_gait::MakeGaitPhase(input.time_sec, config_.cycle_time);
  const auto phase = rl_gait::PhaseObservation(gait_phase);
  const auto period = rl_gait::PeriodObservation(gait_phase);
  observation.phase = {phase[0], phase[1]};
  observation.period = {period[0]};

  return observation;
}

RuntimeOutput RlRuntime::Step(
    const RuntimeInput& input,
    const std::vector<double>& fallback_pose,
    double fallback_kp,
    double fallback_kd)
{
  if (!backend_) {
    throw std::runtime_error("RL runtime backend is not ready.");
  }

  const auto observation_input = BuildObservationInput(input);  // 把原始的输入构建观测输入
  const auto observation = observation_builder_.Build(observation_input);
  const auto action = backend_->Infer(observation);
  auto output = action_mapper_.Map(
      action, fallback_pose, fallback_kp, fallback_kd, joint_limits_);
  last_action_ = output.clipped_action;
  return output;
}

}  // namespace hhros2_motion_cores::rl_runtime
