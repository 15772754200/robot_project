# Floating-base state estimate produced by hhros2_estimation.
#
# Frame conventions:
#   - pose is expressed in the odometry frame (header.frame_id).
#   - twist and linear_acceleration are expressed in the base frame.
#   - linear_acceleration is the bias-compensated base acceleration in the
#     base frame, useful for downstream balance / WBC controllers.
#   - contact[0] = left foot, contact[1] = right foot; false when unavailable.
std_msgs/Header header
geometry_msgs/Pose pose
geometry_msgs/Twist twist
float64[3] linear_acceleration
bool[2] contact
