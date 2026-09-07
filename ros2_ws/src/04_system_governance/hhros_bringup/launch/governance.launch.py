"""Governance layer: the hhros2_core safety governor daemon."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    safety_yaml = PathJoinSubstitution(
        [FindPackageShare("hhros2_core"), "config", "safety.yaml"]
    )
    qos_config = PathJoinSubstitution(
        [FindPackageShare("hhros_bringup"), "config", "qos_overrides.yaml"]
    )
    core = Node(
        package="hhros2_core",
        executable="hhros2_core",
        name="hhros2_core",
        output="screen",
        parameters=[
            qos_config,
            safety_yaml,
            {"shm_name": LaunchConfiguration("shm_name")},
        ],
    )
    return LaunchDescription(
        [
            DeclareLaunchArgument("shm_name", default_value="hhros2_motor_shm"),
            core,
        ]
    )
