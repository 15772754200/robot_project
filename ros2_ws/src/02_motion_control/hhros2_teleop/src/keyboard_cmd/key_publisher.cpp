#include "hhros2_teleop/keyboard_cmd/key_publisher.hpp"
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <hhros2_interfaces/srv/set_control_mode.hpp>
#include "hhros2_log/log.h"

namespace hhros2_teleop {

KeyPublisherNode::KeyPublisherNode(const rclcpp::NodeOptions& options)
    : LifecycleNode("key_publisher", options) {}

// ---------- 生命周期回调 ----------
rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
KeyPublisherNode::on_configure(const rclcpp_lifecycle::State&) {
    // 初始化日志系统
    static bool logger_initialized = false;
    if (!logger_initialized) {
        if (Logger::getInstance()->initialize("run_logs", LogLevel::DEBUG)) {
            logger_initialized = true;
            RCLCPP_INFO(rclcpp::get_logger("key_publisher"), "Logger initialized.");
        } else {
            std::cerr << "[key_publisher] Failed to initialize logger!" << std::endl;
        }
    } 

    LOG_INFO(LogType::OTHER, "Configuring...");
    // 创建 ROS 发布者和客户端（可在 configure 阶段创建）
    vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", rclcpp::QoS(1));
    mode_client_ = this->create_client<hhros2_interfaces::srv::SetControlMode>("/hhros2_core/set_control_mode");
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
KeyPublisherNode::on_activate(const rclcpp_lifecycle::State&) {
    LOG_INFO(LogType::OTHER,  "Activating...");
    // 创建 KeyBoard 对象（其构造函数会启动键盘线程）
    keyboard_ = std::make_shared<KeyBoard>();
    // 设置回调
    keyboard_->setModeCallback([this](uint8_t mode) {
        this->requestMode(mode);
    });
    keyboard_->setVelCallback([this](double x, double y, double z) {
        this->publishCmdVel(x, y, z);
    });

    // 激活发布者
    vel_pub_->on_activate();
    LOG_INFO(LogType::OTHER, "KeyPublisher activated with remote control on port 8080.");
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
KeyPublisherNode::on_deactivate(const rclcpp_lifecycle::State&) {
    LOG_INFO(LogType::OTHER, "Deactivating...");
    // 销毁 KeyBoard 对象，这会停止键盘线程
    keyboard_.reset();
    // 停用发布者
    vel_pub_->on_deactivate();
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
KeyPublisherNode::on_cleanup(const rclcpp_lifecycle::State&) {
    LOG_INFO(LogType::OTHER, "Cleaning up...");
    // 释放 ROS 资源
    vel_pub_.reset();
    mode_client_.reset();
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
KeyPublisherNode::on_shutdown(const rclcpp_lifecycle::State&) {
    LOG_INFO(LogType::OTHER, "Shutting down...");
    keyboard_.reset();
    vel_pub_.reset();
    mode_client_.reset();
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

// ---------- 私有方法 ----------
void KeyPublisherNode::requestMode(uint8_t mode) {
    if (!mode_client_->wait_for_service(std::chrono::milliseconds(10))) {
        LOG_WARNING(LogType::OTHER, "SetControlMode service not available");
        return;
    }
    auto req = std::make_shared<hhros2_interfaces::srv::SetControlMode::Request>();
    req->mode = mode;
    mode_client_->async_send_request(req);
    LOG_INFO(LogType::OTHER, "Sending mode request: %d", mode);
}

void KeyPublisherNode::publishCmdVel(double x, double y, double z) {
    geometry_msgs::msg::Twist msg;
    msg.linear.x = x;
    msg.linear.y = y;
    msg.angular.z = z;
    vel_pub_->publish(msg);
    LOG_DEBUG(LogType::OTHER, "Published cmd_vel: linear=(%.2f, %.2f), angular=%.2f", x, y, z);
}

} // namespace hhros2_teleop

// ---------- main 函数 ----------
int main(int argc, char** argv) {
    rclcpp::init(argc, argv);

    rclcpp::executors::SingleThreadedExecutor executor;
    auto node = std::make_shared<hhros2_teleop::KeyPublisherNode>();

    // 手动触发生命周期转换
    rclcpp_lifecycle::State initial_state;
    auto ret = node->on_configure(initial_state);
    if (ret != rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS) {
        std::cerr << "Failed to configure key_publisher node" << std::endl;
        return 1;
    }
    ret = node->on_activate(initial_state);
    if (ret != rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS) {
        std::cerr << "Failed to activate key_publisher node" << std::endl;
        return 1;
    }

    executor.add_node(node->get_node_base_interface());
    executor.spin();
    rclcpp::shutdown();
    return 0;
}