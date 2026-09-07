"""Legacy keyboard entrypoint; top-level bringup owns teleop selection."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    keyboard_node = Node(
        package='hhros2_teleop',
        executable='key_publisher',
        name='key_publisher',
        output='screen',
        emulate_tty=True,
    )

    return LaunchDescription([keyboard_node])
