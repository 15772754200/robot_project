"""Launch the isolated legacy wave simulation."""

from launch import LaunchDescription
from launch.actions import LogInfo
from launch_ros.actions import Node, PushRosNamespace


def generate_launch_description():
    return LaunchDescription(
        [
            PushRosNamespace("legacy_wave_sim"),
            Node(
                package="wave_control_system",
                executable="wave_controller",
                name="wave_controller",
                output="screen",
            ),
            Node(
                package="wave_control_system",
                executable="motor_driver_sim",
                name="motor_driver_sim",
                output="screen",
            ),
            LogInfo(
                msg=(
                    "Legacy wave simulation started in /legacy_wave_sim; "
                    "it cannot drive the robot."
                )
            ),
        ]
    )
