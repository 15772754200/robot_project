#pragma once

// Whole-body control core (core 1 of the dual-core design): a model-based
// WBC/QP solver for high-stiffness, deterministic tasks (standing balance,
// precise manipulation poses). This skeleton lays out the solve() hook where a
// QP backend (e.g. qpOASES / OSQP / Pinocchio dynamics) plugs in; the solve is
// stubbed to a balanced standing posture so the node builds standalone.

#include <vector>

#include "hhros2_motion_cores/core/motion_core_base.hpp"

namespace hhros2_motion_cores
{

class WbcCore : public MotionCoreBase
{
public:
    explicit WbcCore(const rclcpp::NodeOptions & options);

protected:
    bool compute(
        const geometry_msgs::msg::Twist & cmd_vel,
        const hhros2_interfaces::msg::BaseState & state,
        hhros2_interfaces::msg::JointMotor & out) override;

private:
    std::vector<double> stand_pose_;
    double kp_ = 120.0;  // higher stiffness than RL for deterministic tasks
    double kd_ = 3.0;
};

}  // namespace hhros2_motion_cores
