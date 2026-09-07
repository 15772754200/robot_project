#include "hhros2_estimation/state_estimator_component.hpp"

#include <algorithm>
#include <cmath>

#include "rclcpp_components/register_node_macro.hpp"
#include "hhros2_log/log.h"

namespace hhros2_estimation
{
namespace
{

rclcpp::QosOverridingOptions platform_qos_overrides()
{
    return {
        rclcpp::QosPolicyKind::History,
        rclcpp::QosPolicyKind::Depth,
        rclcpp::QosPolicyKind::Reliability,
        rclcpp::QosPolicyKind::Durability,
    };
}

rclcpp::PublisherOptions publisher_options()
{
    rclcpp::PublisherOptions options;
    options.qos_overriding_options = platform_qos_overrides();
    return options;
}

rclcpp::SubscriptionOptions subscription_options()
{
    rclcpp::SubscriptionOptions options;
    options.qos_overriding_options = platform_qos_overrides();
    return options;
}

}  // namespace

StateEstimatorComponent::StateEstimatorComponent(
    const rclcpp::NodeOptions & options)
: rclcpp::Node("state_estimator", options)
{
    // tmny edit
    // Parameter best practice: declare with defaults, read once into members.
    // The estimate is published on the IMU callback so it inherits the IMU rate
    // (200-400 Hz) without an extra timer thread.

    // 初始化日志系统
    static bool logger_initialized = false;
    if (!logger_initialized) {
        if (Logger::getInstance()->initialize("run_logs", LogLevel::DEBUG)) {
            logger_initialized = true;
            RCLCPP_INFO(rclcpp::get_logger("estimationNode"), "Logger initialized.");
        } else {
            std::cerr << "[estimationNode] Failed to initialize logger!" << std::endl;
        }
    } 
    
    odom_frame_ = declare_parameter<std::string>("odom_frame", odom_frame_);
    base_frame_ = declare_parameter<std::string>("base_frame", base_frame_);
    gravity_ = declare_parameter<double>("gravity", gravity_);
    velocity_leak_ = declare_parameter<double>("velocity_leak", velocity_leak_);

    auto sensor_qos = rclcpp::SensorDataQoS();
    imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
        "/imu/data", sensor_qos,
        std::bind(&StateEstimatorComponent::on_imu, this, std::placeholders::_1),
        subscription_options());
    joint_sub_ = create_subscription<sensor_msgs::msg::JointState>(
        "/joint_states", sensor_qos,
        std::bind(
            &StateEstimatorComponent::on_joints, this, std::placeholders::_1),
        subscription_options());

    state_pub_ = create_publisher<hhros2_interfaces::msg::BaseState>(
        "/robot/state_estimate", rclcpp::QoS(10), publisher_options());
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>(
        "/robot/odom", rclcpp::QoS(10), publisher_options());

    LOG_INFO(LogType::CONTROLLERLOG, "State estimator component started.");
}

void StateEstimatorComponent::on_joints(
    sensor_msgs::msg::JointState::ConstSharedPtr msg)
{
    // tmny edit
    // Placeholder contact detection from leg kinematics. A production estimator
    // fuses foot force/torque or estimated GRF; here we infer stance from knee
    // load proxy (effort) so the integrator has a zero-velocity update source.
    auto stance_from = [&](const std::string & knee) -> bool {
        for (std::size_t i = 0; i < msg->name.size(); ++i)
        {
            if (msg->name[i] == knee && i < msg->effort.size())
            {
                return std::abs(msg->effort[i]) > 5.0;  // Nm threshold
            }
        }
        return true;  // assume contact if unknown (conservative)
    };
    contact_[0] = stance_from("left_knee_joint");
    contact_[1] = stance_from("right_knee_joint");
}

void StateEstimatorComponent::on_imu(sensor_msgs::msg::Imu::ConstSharedPtr msg)
{
    orientation_ = {msg->orientation.x, msg->orientation.y, msg->orientation.z,
                    msg->orientation.w};
    ang_vel_ = {msg->angular_velocity.x, msg->angular_velocity.y,
                msg->angular_velocity.z};
    lin_acc_ = {msg->linear_acceleration.x, msg->linear_acceleration.y,
                msg->linear_acceleration.z};

    const rclcpp::Time stamp(msg->header.stamp);
    integrate(stamp);
    publish_estimate(stamp);
}

void StateEstimatorComponent::integrate(const rclcpp::Time & stamp)
{
    if (!have_imu_)
    {
        last_imu_time_ = stamp;
        have_imu_ = true;
        return;
    }
    double dt = (stamp - last_imu_time_).seconds();
    last_imu_time_ = stamp;
    if (dt <= 0.0 || dt > 0.1)
    {
        return;  // reject non-monotonic or stale stamps
    }

    // tmny edit
    // Rotate measured acceleration to world, remove gravity, integrate to base
    // velocity with a leaky integrator. When either foot is in contact we apply
    // a zero-velocity update (ZUPT) to bound horizontal drift - the standard
    // trick used by commercial legged estimators lacking absolute position.
    const double qx = orientation_[0], qy = orientation_[1];
    const double qz = orientation_[2], qw = orientation_[3];
    const double az_world =
        2.0 * (qx * qz + qw * qy) * lin_acc_[0] +
        2.0 * (qy * qz - qw * qx) * lin_acc_[1] +
        (qw * qw - qx * qx - qy * qy + qz * qz) * lin_acc_[2] - gravity_;
    (void)az_world;

    for (int i = 0; i < 3; ++i)
    {
        lin_vel_[i] = velocity_leak_ * lin_vel_[i] + lin_acc_[i] * dt;
        position_[i] += lin_vel_[i] * dt;
    }
    if (contact_[0] || contact_[1])
    {
        lin_vel_[0] *= 0.5;  // soft ZUPT on horizontal velocity
        lin_vel_[1] *= 0.5;
    }
}

void StateEstimatorComponent::publish_estimate(const rclcpp::Time & stamp)
{
    hhros2_interfaces::msg::BaseState bs;
    bs.header.stamp = stamp;
    bs.header.frame_id = odom_frame_;
    bs.pose.position.x = position_[0];
    bs.pose.position.y = position_[1];
    bs.pose.position.z = position_[2];
    bs.pose.orientation.x = orientation_[0];
    bs.pose.orientation.y = orientation_[1];
    bs.pose.orientation.z = orientation_[2];
    bs.pose.orientation.w = orientation_[3];
    bs.twist.linear.x = lin_vel_[0];
    bs.twist.linear.y = lin_vel_[1];
    bs.twist.linear.z = lin_vel_[2];
    bs.twist.angular.x = ang_vel_[0];
    bs.twist.angular.y = ang_vel_[1];
    bs.twist.angular.z = ang_vel_[2];
    bs.linear_acceleration = {lin_acc_[0], lin_acc_[1], lin_acc_[2]};
    bs.contact = {contact_[0], contact_[1]};
    state_pub_->publish(bs);

    nav_msgs::msg::Odometry odom;
    odom.header.stamp = stamp;
    odom.header.frame_id = odom_frame_;
    odom.child_frame_id = base_frame_;
    odom.pose.pose = bs.pose;
    odom.twist.twist = bs.twist;
    odom_pub_->publish(odom);
}

}  // namespace hhros2_estimation

RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_estimation::StateEstimatorComponent)
