#ifndef HHROS2_MOTION_CORES_RL_OBSERVATION_CONFIG_HPP_
#define HHROS2_MOTION_CORES_RL_OBSERVATION_CONFIG_HPP_

#include <cstddef>
#include <string>
#include <vector>

namespace hhros2_motion_cores::rl_observation {

struct ObservationConfig {
  std::size_t num_joints{12};
  std::size_t num_observations{0};
  std::size_t observation_stack{1};
  // 0: legacy training layout, term-major with each term repeated at reset.
  // 1: deployment layout, frame-major with older frames zero-prefilled.
  // History inside each term/frame is always oldest-to-newest.
  int observation_stack_mode{1};

  std::vector<std::string> observations;

  float joint_pos_scale{1.0f};
  float joint_vel_scale{0.05f};
  float angl_vel_scale{0.25f};
  float projected_gravity_scale{1.0f};
  float command_scale{1.0f};
  float last_action_scale{1.0f};
  float phase_scale{1.0f};
  float period_scale{1.0f};
  float clip_obs{0.0f};
  float cycle_time{0.0f};

  std::vector<float> command_scales;

  bool use_command{true};
  bool use_last_action{true};

  std::string model_name;
  std::string model_dir;

  std::vector<float> default_joint_pos;
  std::vector<float> default_joint_pos_others;
  std::vector<float> action_scale;
  std::vector<float> clip_actions_lower;
  std::vector<float> clip_actions_upper;
  std::vector<float> kp;
  std::vector<float> kd;
  std::vector<int> joint_mapping;
  std::vector<int> joint_mapping_others;
  std::vector<int> state_joint_mapping;
};

ObservationConfig LoadObservationConfigFromYaml(const std::string& path);

} // namespace hhros2_motion_cores::rl_observation
#endif  // HHROS2_MOTION_CORES_RL_OBSERVATION_CONFIG_HPP_
