"""Canonical logical-joint resources for deployment simulation tools."""

import math
from pathlib import Path

import yaml
from ament_index_python.packages import get_package_share_directory


def _description_config_path(filename: str) -> Path:
    return (
        Path(get_package_share_directory("hhros2_description"))
        / "config"
        / filename
    )


def load_canonical_joint_names() -> list[str]:
    path = _description_config_path("joint_order.yaml")
    with path.open(encoding="utf-8") as stream:
        root = yaml.safe_load(stream)
    joint_names = root.get("joint_names") if isinstance(root, dict) else None
    if not isinstance(joint_names, list) or len(joint_names) != 23:
        raise RuntimeError(f"Expected 23 joint_names in {path}")
    if not all(isinstance(name, str) and name for name in joint_names):
        raise RuntimeError(f"Invalid joint name in {path}")
    if len(joint_names) != len(set(joint_names)):
        raise RuntimeError(f"Duplicate joint_names in {path}")
    return joint_names


def load_home_pose(joint_names: list[str]) -> tuple[list[float], list[float]]:
    path = _description_config_path("home_pose.yaml")
    with path.open(encoding="utf-8") as stream:
        root = yaml.safe_load(stream)
    base = root.get("base") if isinstance(root, dict) else None
    positions = root.get("joint_positions") if isinstance(root, dict) else None
    if not isinstance(base, dict):
        raise RuntimeError(f"home_pose base is invalid in {path}")
    if not isinstance(positions, dict) or set(positions) != set(joint_names):
        raise RuntimeError(f"home_pose joint set does not match {path}")
    base_position = [float(value) for value in base.get("position", [])]
    base_orientation = [
        float(value) for value in base.get("orientation_wxyz", [])
    ]
    if len(base_position) != 3 or len(base_orientation) != 4:
        raise RuntimeError(f"home_pose base dimensions are invalid in {path}")
    joint_positions = [float(positions[name]) for name in joint_names]
    values = joint_positions + base_position + base_orientation
    if not all(math.isfinite(value) for value in values):
        raise RuntimeError(f"home_pose values must be finite in {path}")
    orientation_norm = math.sqrt(
        sum(value * value for value in base_orientation)
    )
    if orientation_norm <= 1.0e-12:
        raise RuntimeError(f"home_pose orientation must be non-zero in {path}")
    base_orientation = [
        value / orientation_norm for value in base_orientation
    ]
    return (
        joint_positions,
        base_position + base_orientation,
    )


DEFAULT_SERIAL_JOINT_NAMES = load_canonical_joint_names()
DEFAULT_MUJOCO_JOINT_NAMES = list(DEFAULT_SERIAL_JOINT_NAMES)
DEFAULT_MUJOCO_JOINT_NAMES[
    DEFAULT_SERIAL_JOINT_NAMES.index("head_yaw_joint")
] = ""
DEFAULT_INITIAL_POSITIONS, DEFAULT_INITIAL_BASE_POSE = load_home_pose(
    DEFAULT_SERIAL_JOINT_NAMES
)

DEFAULT_MODEL_RESOURCE_PATH = (
    "package://hhros2_description/models/Yidong/mjcf/scene_safety_rope.xml"
)
