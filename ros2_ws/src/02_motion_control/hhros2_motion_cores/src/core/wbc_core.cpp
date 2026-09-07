#include "hhros2_motion_cores/core/wbc_core.hpp"

#include "rclcpp_components/register_node_macro.hpp"

namespace hhros2_motion_cores
{

WbcCore::WbcCore(const rclcpp::NodeOptions & options)
: MotionCoreBase("wbc_core", options)
{
    stand_pose_ = declare_parameter<std::vector<double>>(
        "stand_pose", std::vector<double>{});
    kp_ = declare_parameter<double>("default_kp", 120.0);
    kd_ = declare_parameter<double>("default_kd", 3.0);
}

bool WbcCore::compute(
    const geometry_msgs::msg::Twist & /*cmd_vel*/,
    const hhros2_interfaces::msg::BaseState & state,
    hhros2_interfaces::msg::JointMotor & out)
{
    // Production path: build the WBC task stack (CoM/ZMP, contact constraints,
    // posture) from the base-state estimate, solve the QP for joint torques, and
    // emit a torque-dominant command with high stiffness. Stub: hold the
    // configured standing pose. The estimate is referenced so the wiring is real.
    (void)state;
    const std::size_t n = n_joints();
    for (std::size_t i = 0; i < n; ++i)
    {
        out.position[i] = (i < stand_pose_.size()) ? stand_pose_[i] : 0.0;
        out.velocity[i] = 0.0;
        out.effort[i] = 0.0;
        out.kp[i] = kp_;
        out.kd[i] = kd_;
    }
    return true;
}

}  // namespace hhros2_motion_cores

RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::WbcCore)
