#pragma once

// Floating-base state estimator (02_motion_control). Fuses IMU orientation with  // tmny edit
// leg kinematics and foot-contact detection to produce a base pose/twist        // tmny edit
// estimate for the controllers. Built as an rclcpp component so it can share an   // tmny edit
// intra-process container with the controllers and motion cores (zero copy).     // tmny edit

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "geometry_msgs/msg/twist.hpp"
#include "hhros2_interfaces/msg/base_state.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace hhros2_estimation
{

class StateEstimatorComponent : public rclcpp::Node
{
public:
    explicit StateEstimatorComponent(const rclcpp::NodeOptions & options);

private:
    void on_imu(sensor_msgs::msg::Imu::ConstSharedPtr msg);
    void on_joints(sensor_msgs::msg::JointState::ConstSharedPtr msg);
    void publish_estimate(const rclcpp::Time & stamp);

    // Complementary base-velocity update with contact-based zero-velocity reset. // tmny edit
    void integrate(const rclcpp::Time & stamp);

    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_sub_;
    rclcpp::Publisher<hhros2_interfaces::msg::BaseState>::SharedPtr state_pub_;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;

    std::string odom_frame_ = "odom";
    std::string base_frame_ = "base_link";
    double gravity_ = 9.81;
    double velocity_leak_ = 0.98;  // leaky integrator to bound drift             // tmny edit

    // estimator state
    std::array<double, 4> orientation_{{0.0, 0.0, 0.0, 1.0}};
    std::array<double, 3> ang_vel_{{0.0, 0.0, 0.0}};
    std::array<double, 3> lin_acc_{{0.0, 0.0, 0.0}};
    std::array<double, 3> lin_vel_{{0.0, 0.0, 0.0}};
    std::array<double, 3> position_{{0.0, 0.0, 0.0}};
    std::array<bool, 2> contact_{{true, true}};

    rclcpp::Time last_imu_time_{0, 0, RCL_ROS_TIME};
    bool have_imu_ = false;
};

}  // namespace hhros2_estimation
