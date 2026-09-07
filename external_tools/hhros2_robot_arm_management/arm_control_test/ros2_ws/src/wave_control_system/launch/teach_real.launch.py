#!/usr/bin/env python3

from pathlib import Path

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    trajectory_dir = LaunchConfiguration("trajectory_dir")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "trajectory_dir",
                default_value=str(Path.home() / "hhros2_arm_trajectories"),
            ),
            Node(
                package="wave_control_system",
                executable="teach_manager_real",
                name="teach_manager_real",
                output="screen",
                parameters=[
                    {
                        "joint_state_topic": "/joint_states",
                        "command_topic": "/humanoid_base_controller/reference",
                        "arbitration_topic": "/humanoid/arbitration_mode",
                        "control_mode_service": "/hhros2_core/set_control_mode",
                        "trajectory_dir": trajectory_dir,
                        "record_frequency": 50.0,
                        "playback_frequency": 50.0,
                        "arm_kp": 50.0,
                        "arm_kd": 2.0,
                        "require_stand_mode": True,
                        "feedback_timeout_sec": 0.25,
                        "max_start_position_error_rad": 0.25,
                        "max_trajectory_velocity_rad_s": 1.0,
                        "max_trajectory_acceleration_rad_s2": 5.0,
                        "require_exclusive_command_topic": True,
                    }
                ],
            ),
        ]
    )
