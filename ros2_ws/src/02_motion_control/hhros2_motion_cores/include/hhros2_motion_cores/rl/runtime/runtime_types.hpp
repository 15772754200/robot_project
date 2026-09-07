#ifndef HHROS2_MOTION_CORES_RL_RUNTIME_TYPES_HPP_
#define HHROS2_MOTION_CORES_RL_RUNTIME_TYPES_HPP_

#include <array>
#include <vector>

namespace hhros2_motion_cores::rl_runtime {

struct RuntimeInput {
  std::array<double, 4> base_quat_xyzw{0.0, 0.0, 0.0, 1.0};
  // sensor_msgs/Imu angular_velocity is already expressed in the body frame.
  std::array<double, 3> base_ang_vel_body{0.0, 0.0, 0.0};
  std::array<double, 3> command{0.0, 0.0, 0.0};
  std::vector<float> joint_pos;
  std::vector<float> joint_vel;
  double time_sec{0.0};
};

struct RuntimeOutput {
  std::vector<float> raw_action;
  std::vector<float> clipped_action;
  std::vector<double> target_position;
  std::vector<double> target_velocity;
  std::vector<double> target_effort;
  std::vector<double> kp;
  std::vector<double> kd;
};

}  // namespace hhros2_motion_cores::rl_runtime

#endif  // HHROS2_MOTION_CORES_RL_RUNTIME_TYPES_HPP_
