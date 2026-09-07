"""Intelligence layer: behavior command entrypoint and optional perception."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import (
    LaunchConfiguration,
    PathJoinSubstitution,
    PythonExpression,
)
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    enable_behavior = LaunchConfiguration("enable_behavior")
    enable_perception = LaunchConfiguration("enable_perception")
    enable_teleop = LaunchConfiguration("enable_teleop")
    teleop_input = LaunchConfiguration("teleop_input")
    joy_dev = LaunchConfiguration("joy_dev")
    qos_config = PathJoinSubstitution(
        [FindPackageShare("hhros_bringup"), "config", "qos_overrides.yaml"]
    )

    joy_selected = IfCondition(
        PythonExpression([
            "'", enable_teleop,
            "'.lower() in ('true', '1', 'yes', 'on') and '",
            teleop_input, "' == 'joy'",
        ]))
    keyboard_selected = IfCondition(
        PythonExpression([
            "'", enable_teleop,
            "'.lower() in ('true', '1', 'yes', 'on') and '",
            teleop_input, "' == 'keyboard'",
        ]))

    behavior = Node(
        package="hhros2_behavior",
        executable="behavior_node",
        name="hhros2_behavior",
        output="screen",
        parameters=[qos_config],
        condition=IfCondition(enable_behavior),
    )
    perception = Node(
        package="hhros2_perception",
        executable="perception_node",
        name="hhros2_perception",
        output="screen",
        parameters=[qos_config],
        condition=IfCondition(enable_perception),
    )
    joy = Node(
        package="joy",
        executable="joy_node",
        name="joy_node",
        output="screen",
        parameters=[
            qos_config,
            {
                "dev": joy_dev,
                "deadzone": 0.05,
                "autorepeat_rate": 20.0,
            },
        ],
        condition=joy_selected,
    )
    joy_teleop = Node(
        package="hhros2_behavior",
        executable="joy_teleop_node",
        name="hhros2_joy_teleop",
        output="screen",
        parameters=[qos_config],
        condition=joy_selected,
    )
    keyboard_teleop = Node(
        package="hhros2_behavior",
        executable="keyboard_teleop_node",
        name="hhros2_keyboard_teleop",
        output="screen",
        emulate_tty=True,
        parameters=[qos_config],
        condition=keyboard_selected,
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("enable_behavior", default_value="true"),
            DeclareLaunchArgument(
                "enable_perception",
                default_value="false",
                description=(
                    "Enable placeholder perception. It reports WARNING until "
                    "a point cloud arrives."
                ),
            ),
            DeclareLaunchArgument(
                "enable_teleop",
                default_value="false",
                description="Start the selected operator teleop input",
            ),
            DeclareLaunchArgument(
                "teleop_input",
                default_value="joy",
                choices=["joy", "keyboard"],
                description="Operator input adapter started when teleop is enabled",
            ),
            DeclareLaunchArgument("joy_dev", default_value="/dev/input/js0"),
            behavior,
            perception,
            joy,
            joy_teleop,
            keyboard_teleop,
        ]
    )
