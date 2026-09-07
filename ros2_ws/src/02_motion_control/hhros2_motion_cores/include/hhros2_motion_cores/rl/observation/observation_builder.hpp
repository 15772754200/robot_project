#ifndef HHROS2_MOTION_CORES_RL_OBSERVATION_BUILDER_HPP_
#define HHROS2_MOTION_CORES_RL_OBSERVATION_BUILDER_HPP_

#include <deque>
#include <vector>

#include "hhros2_motion_cores/rl/observation/observation_config.hpp"

namespace hhros2_motion_cores::rl_observation {

struct ObservationInput{
  std::vector<float> base_ang_vel;            // 3
  std::vector<float> projected_gravity;       // 3
  std::vector<float> command;                 // 3
  std::vector<float> joint_pos;                // 12
  std::vector<float> joint_vel;                // 12
  std::vector<float> last_action;              // 12
  std::vector<float> phase;                    // 2
  std::vector<float> period;                   // 1
};

class ObservationBuilder {
  public:
    explicit ObservationBuilder(const ObservationConfig& config);

    std::vector<float> BuildSingle(const ObservationInput& input) const;
    std::vector<float> Build(const ObservationInput& input);
    void Reset();

  private:
    ObservationConfig config_;
    std::deque<std::vector<float>> observation_history_;
    std::vector<std::deque<std::vector<float>>> observation_term_history_;
};

}  // namespace hhros2_motion_cores::rl_observation

#endif  // HHROS2_MOTION_CORES_RL_OBSERVATION_BUILDER_HPP_
