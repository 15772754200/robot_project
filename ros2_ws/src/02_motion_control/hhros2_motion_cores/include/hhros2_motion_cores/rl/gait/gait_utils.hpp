#ifndef HHROS2_MOTION_CORES_RL_GAIT_UTILS_HPP_
#define HHROS2_MOTION_CORES_RL_GAIT_UTILS_HPP_

#include <array>

namespace hhros2_motion_cores::rl_gait {

struct GaitPhase {
  double normalized{0.0};
  double period_sec{0.0};
  double sin{0.0};
  double cos{1.0};
};

double WrapPhase(double phase);
GaitPhase MakeGaitPhase(double time_sec, double cycle_time_sec);
std::array<float, 2> PhaseObservation(const GaitPhase& phase);
std::array<float, 1> PeriodObservation(const GaitPhase& phase);

}  // namespace hhros2_motion_cores::rl_gait

#endif  // HHROS2_MOTION_CORES_RL_GAIT_UTILS_HPP_
