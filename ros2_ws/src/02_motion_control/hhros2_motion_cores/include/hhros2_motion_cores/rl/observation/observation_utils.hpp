#ifndef __HHROS2_MOTION_CORES_RL_OBSERVATION_CONFIG_HPP__
#define __HHROS2_MOTION_CORES_RL_OBSERVATION_CONFIG_HPP__

#include <algorithm>
#include <vector>
#include <stdexcept>

namespace hhros2_motion_cores::rl_observation {

  inline void CheckSize(
    const std::vector<float>& data,
    const std::size_t expected_size,
    const char* name)
  {
    if (data.size() != expected_size) {
      throw std::runtime_error(
        "Invalid size for " + std::string(name) + 
        ". Expected: " + std::to_string(expected_size) + 
        ", got: " + std::to_string(data.size()));
    }
  }

  inline float Clip(float value, float min_value, float max_value)
  {
    return std::clamp(value, min_value, max_value);
  }

  inline void AppendScaled(
    std::vector<float> &dst,
    const std::vector<float>& src,
    float scale)
    {
      for (float v : src) {
        dst.push_back(v * scale);
      }
    }

  inline void AppendClipped(
    std::vector<float> &dst,
    const std::vector<float>& src,
    float min_value,
    float max_value)
  {
    for (float v : src) {
      dst.push_back(Clip(v, min_value, max_value));
    }
  }

  inline void AppendJointPosRelative(
    std::vector<float> &dst,
    const std::vector<float> &joint_pos,
    const std::vector<float> &default_joint_pos,
    float scale)
    {
      if (joint_pos.size() != default_joint_pos.size()) {
        throw std::runtime_error(
          "joint_pos and default_joint_pos size mismatch."
        );
      }

      for (std::size_t i = 0; i < joint_pos.size(); ++i) {
        dst.push_back((joint_pos[i] - default_joint_pos[i]) * scale);
      }
    }
}
#endif  // __HHROS2_MOTION_CORES_RL_OBSERVATION_CONFIG_HPP__