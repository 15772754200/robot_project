"""Top-level humanoid bringup with staged startup and command-topic isolation."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, TimerAction
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, PythonExpression
from launch_ros.substitutions import FindPackageShare


def _include(name, condition=None, **launch_args):
    source = PythonLaunchDescriptionSource(
        PathJoinSubstitution([FindPackageShare("hhros_bringup"), "launch", name])
    )
    return IncludeLaunchDescription(
        source,
        launch_arguments=launch_args.items(),
        condition=condition,
    )


def generate_launch_description() -> LaunchDescription:
    hardware = LaunchConfiguration("hardware")
    shm_name = LaunchConfiguration("shm_name")
    mujoco_model = LaunchConfiguration("mujoco_model")
    backend = LaunchConfiguration("backend")
    enable_imu = LaunchConfiguration("enable_imu")
    enable_imu_broadcaster = LaunchConfiguration("enable_imu_broadcaster")
    motion_reference_topic = LaunchConfiguration("motion_reference_topic")
    enable_ntrip = LaunchConfiguration("enable_ntrip")
    enable_behavior = LaunchConfiguration("enable_behavior")
    enable_perception = LaunchConfiguration("enable_perception")
    enable_teleop = LaunchConfiguration("enable_teleop")
    teleop_input = LaunchConfiguration("teleop_input")
    joy_dev = LaunchConfiguration("joy_dev")

    real_hardware = IfCondition(
        PythonExpression(["'", hardware, "' == 'real'"])
    )
    external_imu_enabled = PythonExpression([
        "'", hardware, "' == 'real' and '", enable_imu,
        "'.lower() in ('true', '1', 'yes', 'on')",
    ])

    rt = _include(
        "rt_layer.launch.py",
        condition=real_hardware,
        shm_name=shm_name,
        backend=backend,
    )
    control = _include(
        "control_layer.launch.py",
        hardware=hardware,
        shm_name=shm_name,
        mujoco_model=mujoco_model,
        enable_imu_broadcaster=enable_imu_broadcaster,
        motion_reference_topic=motion_reference_topic,
    )
    sensors = _include(
        "sensors.launch.py",
        enable_imu=external_imu_enabled,
        enable_ntrip=enable_ntrip,
    )
    governance = _include("governance.launch.py", shm_name=shm_name)

    intelligence = _include(
        "intelligence.launch.py",
        enable_behavior=enable_behavior,
        enable_perception=enable_perception,
        enable_teleop=enable_teleop,
        teleop_input=teleop_input,
        joy_dev=joy_dev,
    )

    return LaunchDescription([
        DeclareLaunchArgument("hardware", default_value="real"),
        DeclareLaunchArgument("shm_name", default_value="hhros2_motor_shm"),
        DeclareLaunchArgument(
            "mujoco_model",
            default_value=(
                "package://hhros2_description/models/Yidong/mjcf/"
                "scene_safety_rope.xml"
            ),
        ),
        DeclareLaunchArgument("backend", default_value="sim"),
        DeclareLaunchArgument("enable_imu", default_value="false"),
        DeclareLaunchArgument(
            "enable_imu_broadcaster",
            default_value="false",
            description=(
                "Publish ros2_control base_imu on /imu/data. Disable when "
                "an external IMU such as Xsens is running."
            ),
        ),
        DeclareLaunchArgument(
            "motion_reference_topic",
            default_value="/humanoid_base_controller/reference",
            description=(
                "Topic used by motion cores. Set an isolated topic for "
                "direct motor diagnostics."
            ),
        ),
        DeclareLaunchArgument("enable_ntrip", default_value="false"),
        DeclareLaunchArgument("enable_behavior", default_value="true"),
        DeclareLaunchArgument("enable_perception", default_value="false"),
        DeclareLaunchArgument("enable_teleop", default_value="false"),
        DeclareLaunchArgument(
            "teleop_input", default_value="joy", choices=["joy", "keyboard"]
        ),
        DeclareLaunchArgument("joy_dev", default_value="/dev/input/js0"),
        rt,
        TimerAction(period=1.0, actions=[sensors]),
        TimerAction(period=2.0, actions=[control]),
        TimerAction(period=4.0, actions=[governance]),
        TimerAction(period=6.0, actions=[intelligence]),
    ])
