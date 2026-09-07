# Hybrid per-joint command/reference for the 23-DoF humanoid.
#
# Canonical hybrid command exchanged by motion cores and the base controller.
#
# Command path: position = desired angle (rad), velocity = desired rate (rad/s),
# effort = feed-forward torque (Nm), kp/kd = per-joint impedance gains.
std_msgs/Header header
string[23] joint_names
float64[23] kp
float64[23] kd
float64[23] position
float64[23] velocity
float64[23] effort
