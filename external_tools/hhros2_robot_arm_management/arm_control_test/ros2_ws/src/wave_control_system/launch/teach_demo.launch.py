"""Launch the complete isolated eight-joint teaching simulation."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, LogInfo
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace


def generate_launch_description():
    trajectory_dir = LaunchConfiguration("trajectory_dir")
    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "trajectory_dir",
                default_value="~/hhros2_legacy_arm_trajectories",
                description="Directory used by the isolated simulated teaching manager.",
            ),
            PushRosNamespace("legacy_wave_sim"),
            Node(
                package="wave_control_system",
                executable="motor_driver_sim",
                name="motor_driver_sim",
                output="screen",
            ),
            Node(
                package="wave_control_system",
                executable="wave_controller",
                name="wave_controller",
                output="screen",
            ),
            Node(
                package="wave_control_system",
                executable="teach_manager",
                name="teach_manager",
                output="screen",
                parameters=[
                    {
                        "record_frequency": 20.0,
                        "trajectory_dir": trajectory_dir,
                    }
                ],
            ),
            LogInfo(
                msg=(
                    "Complete eight-joint teaching simulation started in "
                    "/legacy_wave_sim. It cannot drive the real robot."
                )
            ),
        ]
    )
