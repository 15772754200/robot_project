from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, RegisterEventHandler, TimerAction
from launch.event_handlers import OnProcessExit
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():
    default_config = PathJoinSubstitution(
        [FindPackageShare("robot_embeded_bringup"), "config", "robot_embeded.yaml"]
    )

    config_arg = DeclareLaunchArgument(
        "config",
        default_value=default_config,
        description="Path to robot_embeded ROS2 parameter file.",
    )

    rs485_pinmux_pad0 = ExecuteProcess(
        cmd=["busybox", "devmem", "0x0243d070", "w", "0x00000400"],
        output="screen",
    )

    rs485_pinmux_pad1 = ExecuteProcess(
        cmd=["busybox", "devmem", "0x0243d078", "w", "0x00000458"],
        output="screen",
    )

    spi_node = Node(
        package="robot_embeded_driver",
        executable="robot_embeded_spi_node",
        name="robot_embeded_spi_node",
        output="screen",
        parameters=[LaunchConfiguration("config")],
    )

    usb_node = Node(
        package="robot_embeded_driver",
        executable="robot_embeded_usb_node",
        name="robot_embeded_usb_node",
        output="screen",
        parameters=[LaunchConfiguration("config")],
    )

    bms_node = Node(
        package="robot_embeded_driver",
        executable="robot_embeded_bms_node",
        name="robot_embeded_bms_node",
        output="screen",
        parameters=[LaunchConfiguration("config")],
    )

    power_node = Node(
        package="robot_embeded_driver",
        executable="robot_embeded_power_node",
        name="robot_embeded_power_node",
        output="screen",
        parameters=[LaunchConfiguration("config")],
    )

    ethercat_node = Node(
        package="robot_embeded_driver",
        executable="robot_embeded_ethercat_node",
        name="robot_embeded_ethercat_node",
        output="screen",
        parameters=[LaunchConfiguration("config")],
    )

    start_nodes_after_pinmux = RegisterEventHandler(
        OnProcessExit(
            target_action=rs485_pinmux_pad1,
            on_exit=[
                TimerAction(
                    period=0.1,
                    actions=[spi_node, usb_node, bms_node, power_node, ethercat_node],
                )
            ],
        )
    )

    start_second_pinmux = RegisterEventHandler(
        OnProcessExit(
            target_action=rs485_pinmux_pad0,
            on_exit=[rs485_pinmux_pad1],
        )
    )

    return LaunchDescription([config_arg, rs485_pinmux_pad0, start_second_pinmux, start_nodes_after_pinmux])
