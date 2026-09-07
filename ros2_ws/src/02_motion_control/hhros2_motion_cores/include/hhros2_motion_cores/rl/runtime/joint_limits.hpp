#ifndef HHROS2_MOTION_CORES_RL_JOINT_LIMITS_HPP_
#define HHROS2_MOTION_CORES_RL_JOINT_LIMITS_HPP_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hhros2_motion_cores::rl_runtime {

struct JointLimit {
  bool has_position_limits{false};
  double min_position{0.0};
  double max_position{0.0};
};

class JointLimitTable {
 public:
  static JointLimitTable LoadFromYaml(const std::string& path);

  bool Empty() const;
  void SetLimit(const std::string& joint_name, const JointLimit& limit);
  std::optional<JointLimit> Find(const std::string& joint_name) const;
  double ClampPosition(const std::string& joint_name, double position) const;

 private:
  std::unordered_map<std::string, JointLimit> limits_;
};

std::vector<std::optional<JointLimit>> BuildOrderedJointLimits(
    const std::vector<std::string>& joint_names,
    const JointLimitTable& limits);

double ClampPosition(
    const std::vector<std::optional<JointLimit>>& ordered_limits,
    std::size_t index,
    double position);

}  // namespace hhros2_motion_cores::rl_runtime

#endif  // HHROS2_MOTION_CORES_RL_JOINT_LIMITS_HPP_
