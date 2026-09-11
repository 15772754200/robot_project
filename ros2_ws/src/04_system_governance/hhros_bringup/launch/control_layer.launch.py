"""ros2_control layer, state estimation, and motion-core component container."""

import atexit
from pathlib import Path
from tempfile import NamedTemporaryFile

import yaml
from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, RegisterEventHandler
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.substitutions import (
    Command,
    FindExecutable,
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode, ParameterValue
from launch_ros.substitutions import FindPackageShare


class NoAliasDumper(yaml.SafeDumper):
    def ignore_aliases(self, data) -> bool:
        return True


def load_canonical_joint_names() -> list[str]:
    path = (
        Path(get_package_share_directory("hhros2_description"))
        / "config"
        / "joint_order.yaml"
    )
    with path.open("r", encoding="utf-8") as stream:
        root = yaml.safe_load(stream)

    names = root.get("joint_names") if isinstance(root, dict) else None
    if (
        not isinstance(names, list)
        or len(names) != 23
        or not all(isinstance(name, str) and name for name in names)
        or len(names) != len(set(names))
    ):
        raise ValueError(f"Expected 23 unique joint_names in {path}")
    return names


def load_canonical_home_pose(joint_names: list[str]) -> list[float]:
    path = (
        Path(get_package_share_directory("hhros2_description"))
        / "config"
        / "home_pose.yaml"
    )
    with path.open("r", encoding="utf-8") as stream:
        root = yaml.safe_load(stream)

    positions = root.get("joint_positions") if isinstance(root, dict) else None
    if not isinstance(positions, dict) or set(positions) != set(joint_names):
        raise ValueError(f"home_pose joint set does not match {path}")
    return [float(positions[name]) for name in joint_names]


def write_controller_joint_params(joint_names: list[str]) -> Path:
    params = {
        controller: {"ros__parameters": {"joints": list(joint_names)}}
        for controller in (
            "joint_state_broadcaster",
            "humanoid_base_controller",
            "damping_controller",
        )
    }
    with NamedTemporaryFile(
        mode="w",
        prefix="hhros2_controller_joints_",
        suffix=".yaml",
        delete=False,
        encoding="utf-8",
    ) as stream:
        yaml.dump(
            params,
            stream,
            Dumper=NoAliasDumper,
            sort_keys=False,
            default_flow_style=False,
        )
        path = Path(stream.name)
    atexit.register(path.unlink, missing_ok=True)
    return path


def generate_launch_description() -> LaunchDescription:
    hardware = LaunchConfiguration("hardware")
    shm_name = LaunchConfiguration("shm_name")
    mujoco_model = LaunchConfiguration("mujoco_model")
    enable_imu_broadcaster = LaunchConfiguration("enable_imu_broadcaster")
    motion_reference_topic = LaunchConfiguration("motion_reference_topic")

    joint_names = load_canonical_joint_names()
    home_pose = load_canonical_home_pose(joint_names)
    controller_joint_params = write_controller_joint_params(joint_names)

    description_pkg = FindPackageShare("hhros2_description")
    controllers_yaml = PathJoinSubstitution(
        [FindPackageShare("hhros2_controllers"), "config", "controllers.yaml"]
    )
    motion_cores_yaml = PathJoinSubstitution(
        [FindPackageShare("hhros_bringup"), "config", "motion_cores.yaml"]
    )
    qos_config = PathJoinSubstitution(
        [FindPackageShare("hhros_bringup"), "config", "qos_overrides.yaml"]
    )
    joint_limits_yaml = PathJoinSubstitution(
        [description_pkg, "config", "joint_limits.yaml"]
    )
    motion_cores_share = Path(
        get_package_share_directory("hhros2_motion_cores")
    )
    rl_assets = {
        "stand": {
            "policy_path": str(
                # motion_cores_share / "model/rl/yd_stand/stand_qing_4_7.onnx"
                motion_cores_share / "model/rl/yd_stand/stand_back.onnx"
            ),
            "observation_config_path": str(
                motion_cores_share / "config/rl/yd_rl_stand/config.yaml"
            ),
        },
        "walk": {
            "policy_path": str(motion_cores_share / "model/rl/yd_walk/walk_5_15.onnx"),
            "observation_config_path": str(
                motion_cores_share / "config/rl/yd_rl_walk/config.yaml"
            ),
        },
        "run": {
            "policy_path": str(motion_cores_share / "model/rl/yd_run/6.291.onnx"),
            "observation_config_path": str(
                motion_cores_share / "config/rl/yd_rl_run/config.yaml"
            ),
        },
    }

    robot_description = {
        "robot_description": ParameterValue(
            Command([
                FindExecutable(name="xacro"),
                " ",
                PathJoinSubstitution(
                    [description_pkg, "urdf", "yidong.urdf.xacro"]
                ),
                " hardware:=",
                hardware,
                " shm_name:=",
                shm_name,
                " mujoco_model:=",
                mujoco_model,
                " qos_config:=",
                qos_config,
            ]),
            value_type=str,
        )
    }

    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[robot_description, qos_config],
    )
    control_node = Node(
        package="controller_manager",
        executable="ros2_control_node",
        output="screen",
        parameters=[controllers_yaml, str(controller_joint_params), qos_config],
        remappings=[
            ("~/robot_description", "/robot_description"),
            ("/imu_sensor_broadcaster/imu", "/imu/data"),
        ],
    )

    def spawner(name, *extra, condition=None):
        return Node(
            package="controller_manager",
            executable="spawner",
            output="screen",
            arguments=[
                name,
                "-c",
                "/controller_manager",
                "--controller-manager-timeout",
                "60",
                "--param-file",
                qos_config,
                *extra,
            ],
            condition=condition,
        )

    joint_state_broadcaster = spawner("joint_state_broadcaster")
    imu_sensor_broadcaster = spawner(
        "imu_sensor_broadcaster",
        condition=IfCondition(enable_imu_broadcaster),
    )
    base_controller = spawner("humanoid_base_controller", "--inactive")
    damping_controller = spawner("damping_controller", "--inactive")
    spawn_rest_after_joint_state = RegisterEventHandler(
        OnProcessExit(
            target_action=joint_state_broadcaster,
            on_exit=[
                imu_sensor_broadcaster,
                base_controller,
                damping_controller,
            ],
        )
    )

    def motion_core(plugin, name, parameters, remappings=None):
        return ComposableNode(
            package="hhros2_motion_cores",
            plugin=plugin,
            name=name,
            parameters=parameters,
            remappings=remappings or [("reference", motion_reference_topic)],
            extra_arguments=[{"use_intra_process_comms": True}],
        )

    control_container = ComposableNodeContainer(
        name="humanoid_control_container",
        namespace="",
        package="rclcpp_components",
        executable="component_container_mt",
        output="screen",
        composable_node_descriptions=[
            ComposableNode(
                package="hhros2_estimation",
                plugin="hhros2_estimation::StateEstimatorComponent",
                name="state_estimator",
                parameters=[qos_config],
                extra_arguments=[{"use_intra_process_comms": True}],
            ),
            motion_core(
                "hhros2_motion_cores::RlStandCore",
                "rl_stand_core",
                [
                    qos_config,
                    motion_cores_yaml,
                    {
                        "joints": joint_names,
                        "joint_limits_path": joint_limits_yaml,
                        "default_pose": home_pose,
                        **rl_assets["stand"],
                    },
                ],
            ),
            motion_core(
                "hhros2_motion_cores::RlWalkCore",
                "rl_walk_core",
                [
                    qos_config,
                    motion_cores_yaml,
                    {
                        "joints": joint_names,
                        "joint_limits_path": joint_limits_yaml,
                        "default_pose": home_pose,
                        **rl_assets["walk"],
                    },
                ],
            ),
            motion_core(
                "hhros2_motion_cores::RlRunCore",
                "rl_run_core",
                [
                    qos_config,
                    motion_cores_yaml,
                    {
                        "joints": joint_names,
                        "joint_limits_path": joint_limits_yaml,
                        "default_pose": home_pose,
                        **rl_assets["run"],
                    },
                ],
            ),
            motion_core(
                "hhros2_motion_cores::WbcCore",
                "wbc_core",
                [
                    qos_config,
                    motion_cores_yaml,
                    {
                        "joints": joint_names,
                        "stand_pose": home_pose,
                    },
                ],
            ),
        ],
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
        DeclareLaunchArgument(
            "enable_imu_broadcaster",
            default_value="false",
            description="Spawn imu_sensor_broadcaster when no external IMU is used.",
        ),
        DeclareLaunchArgument(
            "motion_reference_topic",
            default_value="/humanoid_base_controller/reference",
            description="Topic used by RL and WBC reference publishers.",
        ),
        robot_state_publisher,
        control_node,
        joint_state_broadcaster,
        spawn_rest_after_joint_state,
        control_container,
    ])
