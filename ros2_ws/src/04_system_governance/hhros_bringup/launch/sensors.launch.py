"""External sensor processes: the real Xsens driver and optional NTRIP client.

Top-level bringup permits this IMU publisher only for ``hardware=real``. MuJoCo
owns its IMU through ros2_control, while mock has no implicit IMU source.
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    enable_imu = LaunchConfiguration("enable_imu")
    enable_ntrip = LaunchConfiguration("enable_ntrip")
    qos_config = PathJoinSubstitution(
        [FindPackageShare("hhros_bringup"), "config", "qos_overrides.yaml"]
    )
    xsens_config = PathJoinSubstitution(
        [
            FindPackageShare("xsens_mti_ros2_driver"),
            "param",
            "xsens_mti_node.yaml",
        ]
    )

    # Launch the imported driver at the first-party composition boundary so the
    # authoritative platform QoS contract is injected into its endpoints.
    imu = Node(
        package="xsens_mti_ros2_driver",
        executable="xsens_mti_node",
        name="xsens_mti_node",
        output="screen",
        parameters=[xsens_config, qos_config],
        condition=IfCondition(enable_imu),
    )
    ntrip = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("ntrip"), "launch", "ntrip_launch.py"]
            )
        ),
        condition=IfCondition(enable_ntrip),
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("enable_imu", default_value="false"),
            DeclareLaunchArgument("enable_ntrip", default_value="false"),
            SetEnvironmentVariable("RCUTILS_LOGGING_USE_STDOUT", "1"),
            SetEnvironmentVariable("RCUTILS_LOGGING_BUFFERED_STREAM", "1"),
            imu,
            ntrip,
        ]
    )
