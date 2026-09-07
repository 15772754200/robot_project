// tmny edit
// Entry point for the hhros2_core safety governor daemon.
#include "hhros2_core/safety_governor_node.hpp"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::NodeOptions options;
    auto node = std::make_shared<hhros2_core::SafetyGovernorNode>(options);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
