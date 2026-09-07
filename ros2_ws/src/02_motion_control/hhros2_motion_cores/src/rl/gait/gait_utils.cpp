#include "hhros2_motion_cores/rl/gait/gait_utils.hpp"

#include <cmath>

#include "hhros2_motion_cores/rl/math/math_utils.hpp"

namespace hhros2_motion_cores::rl_gait {

double WrapPhase(double phase)
{
  if (!std::isfinite(phase)) {
    return 0.0;
  }

  phase = std::fmod(phase, 1.0);
  if (phase < 0.0) {
    phase += 1.0;
  }
  return phase;
}

GaitPhase MakeGaitPhase(double time_sec, double cycle_time_sec)
{
  GaitPhase phase;
  if (std::isfinite(cycle_time_sec) && cycle_time_sec > 1e-6) {
    phase.period_sec = cycle_time_sec;
    phase.normalized = WrapPhase(time_sec / cycle_time_sec);
  }

  const double angle =
      2.0 * rl_policy_math_utils::kPi * phase.normalized;
  phase.sin = std::sin(angle);
  phase.cos = std::cos(angle);
  return phase;
}

std::array<float, 2> PhaseObservation(const GaitPhase& phase)
{
  return {
    static_cast<float>(phase.sin),
    static_cast<float>(phase.cos)};
}

std::array<float, 1> PeriodObservation(const GaitPhase& phase)
{
  return {static_cast<float>(phase.period_sec)};
}

}  // namespace hhros2_motion_cores::rl_gait
