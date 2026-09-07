from pathlib import Path

import rclpy
import yaml
from rclpy.node import Node
from rclpy.qos import (
    DurabilityPolicy,
    HistoryPolicy,
    QoSPolicyKind,
    QoSProfile,
    ReliabilityPolicy,
)
from rclpy.qos_overriding_options import QoSOverridingOptions
from std_msgs.msg import String


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = Path(__file__).resolve().parents[5]
QOS_CONFIG = PACKAGE_ROOT / "config" / "qos_overrides.yaml"
XSENS_PARAM_DIR = (
    REPOSITORY_ROOT
    / "ros2_ws/src/01_hardware_bridge/xsens_mti_ros2_driver/param"
)
OVERRIDABLE_POLICIES = (
    QoSPolicyKind.HISTORY,
    QoSPolicyKind.DEPTH,
    QoSPolicyKind.RELIABILITY,
    QoSPolicyKind.DURABILITY,
)
REQUIRED_TOPICS = {
    "/joint_states",
    "/imu/data",
    "/robot/state_estimate",
    "/cmd_vel",
    "/humanoid_base_controller/reference",
    "/humanoid/arbitration_mode",
    "/hhros2_core/safety_status",
    "/hhros2/heartbeat",
}


def _contract():
    root = yaml.safe_load(QOS_CONFIG.read_text(encoding="utf-8"))
    return root["/**"]["ros__parameters"]["qos_overrides"]


def test_every_core_bus_topic_has_complete_endpoint_policies():
    contract = _contract()
    assert REQUIRED_TOPICS <= set(contract)
    for topic, endpoints in contract.items():
        assert set(endpoints) == {"publisher", "subscription"}, topic
        for endpoint, policies in endpoints.items():
            assert set(policies) == {
                "history",
                "depth",
                "reliability",
                "durability",
            }, f"{topic}.{endpoint}"
            assert policies["history"] == "keep_last"
            assert isinstance(policies["depth"], int)
            assert policies["depth"] > 0


def test_ros_applies_imu_contract_to_both_endpoint_kinds():
    rclpy.init(args=["--ros-args", "--params-file", str(QOS_CONFIG)])
    node = Node("qos_contract_probe")
    options = QoSOverridingOptions(policy_kinds=OVERRIDABLE_POLICIES)
    fallback = QoSProfile(depth=9)
    publisher = node.create_publisher(
        String,
        "/imu/data",
        fallback,
        qos_overriding_options=options,
    )
    subscription = node.create_subscription(
        String,
        "/imu/data",
        lambda _: None,
        fallback,
        qos_overriding_options=options,
    )
    try:
        for endpoint in (publisher, subscription):
            actual = endpoint.qos_profile
            assert actual.history == HistoryPolicy.KEEP_LAST
            assert actual.depth == 1
            assert actual.reliability == ReliabilityPolicy.BEST_EFFORT
            assert actual.durability == DurabilityPolicy.VOLATILE
    finally:
        node.destroy_node()
        rclpy.shutdown()


def test_bringup_injects_the_authoritative_file_at_every_process_boundary():
    launch_dir = PACKAGE_ROOT / "launch"
    for name in (
        "control_layer.launch.py",
        "governance.launch.py",
        "intelligence.launch.py",
        "sensors.launch.py",
    ):
        source = (launch_dir / name).read_text(encoding="utf-8")
        assert "qos_overrides.yaml" in source, name
        assert "qos_config" in source, name


def test_core_bus_endpoints_explicitly_enable_ros_qos_overrides():
    endpoint_sources = {
        "ros2_ws/src/01_hardware_bridge/xsens_mti_ros2_driver/"
        "src/messagepublishers/imupublisher.h": "QosOverridingOptions",
        "ros2_ws/src/02_motion_control/hhros2_controllers/"
        "src/humanoid_base_controller.cpp": "QosOverridingOptions",
        "ros2_ws/src/02_motion_control/hhros2_controllers/"
        "src/platform_state_broadcasters.cpp": "QosOverridingOptions",
        "ros2_ws/src/02_motion_control/hhros2_estimation/"
        "src/state_estimator_component.cpp": "QosOverridingOptions",
        "ros2_ws/src/02_motion_control/hhros2_motion_cores/"
        "src/core/motion_core_base.cpp": "QosOverridingOptions",
        "ros2_ws/src/02_motion_control/hhros2_motion_cores/"
        "src/core/rl_policy_core.cpp": "QosOverridingOptions",
        "ros2_ws/src/03_intelligence/hhros2_behavior/hhros2_behavior/"
        "behavior_node.py": "qos_overriding_options",
        "ros2_ws/src/03_intelligence/hhros2_behavior/hhros2_behavior/"
        "joy_teleop_node.py": "qos_overriding_options",
        "ros2_ws/src/03_intelligence/hhros2_behavior/hhros2_behavior/"
        "keyboard_teleop_node.py": "qos_overriding_options",
        "ros2_ws/src/03_intelligence/hhros2_perception/hhros2_perception/"
        "perception_node.py": "qos_overriding_options",
        "ros2_ws/src/04_system_governance/hhros2_core/"
        "src/safety_governor_node.cpp": "QosOverridingOptions",
        "ros2_ws/src/05_simulation_verification/hhros2_sim/"
        "src/mujoco_system_hardware.cpp": "qos_config_path_",
    }
    for relative_path, marker in endpoint_sources.items():
        source = (REPOSITORY_ROOT / relative_path).read_text(encoding="utf-8")
        assert marker in source, relative_path


def test_ros2_control_uses_project_owned_qos_aware_broadcasters():
    controller_config = yaml.safe_load(
        (
            REPOSITORY_ROOT
            / "ros2_ws/src/02_motion_control/hhros2_controllers/"
            "config/controllers.yaml"
        ).read_text(encoding="utf-8")
    )
    manager = controller_config["controller_manager"]["ros__parameters"]
    assert manager["joint_state_broadcaster"]["type"] == (
        "hhros2_controllers/PlatformJointStateBroadcaster"
    )
    assert manager["imu_sensor_broadcaster"]["type"] == (
        "hhros2_controllers/PlatformImuBroadcaster"
    )


def test_imu_publisher_owner_is_derived_from_hardware_mode():
    launch_dir = PACKAGE_ROOT / "launch"
    control_source = (launch_dir / "control_layer.launch.py").read_text(
        encoding="utf-8"
    )
    bringup_source = (launch_dir / "bringup.launch.py").read_text(
        encoding="utf-8"
    )
    real_start_source = (
        REPOSITORY_ROOT / "ros2_ws/start_robot.sh"
    ).read_text(encoding="utf-8")

    assert "enable_imu_broadcaster" not in control_source
    assert "enable_imu_broadcaster" not in bringup_source
    assert "enable_imu_broadcaster" not in real_start_source
    assert "hardware, \"' == 'mujoco'\"" in control_source
    assert "hardware,\n            \"' == 'real'" in bringup_source
    assert "enable_imu=external_imu_enabled" in bringup_source


def test_every_installed_xsens_profile_disables_device_reconfiguration():
    profiles = sorted(XSENS_PARAM_DIR.glob("*.yaml"))
    assert profiles

    for profile in profiles:
        root = yaml.safe_load(profile.read_text(encoding="utf-8"))
        parameters = root["/**"]["ros__parameters"]
        assert parameters["enable_deviceConfig"] is False, profile.name
