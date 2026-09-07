// Standalone entry point for the estimator (used when not composed into a       // tmny edit
// shared intra-process container). In production it is loaded as a component
// alongside the controllers for zero-copy data sharing.
#include "hhros2_estimation/state_estimator_component.hpp"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::NodeOptions options;
    options.use_intra_process_comms(true);
    auto node =
        std::make_shared<hhros2_estimation::StateEstimatorComponent>(options);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
