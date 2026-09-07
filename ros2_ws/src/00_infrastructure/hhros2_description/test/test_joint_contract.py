from pathlib import Path
from xml.etree import ElementTree

import xacro
import yaml
from ament_index_python.packages import get_package_share_directory


def _description_share() -> Path:
    return Path(get_package_share_directory("hhros2_description"))


def _load_yaml(filename: str):
    with (_description_share() / "config" / filename).open(
        encoding="utf-8"
    ) as stream:
        return yaml.safe_load(stream)


def test_logical_joint_contract_is_complete_and_ordered():
    joint_names = _load_yaml("joint_order.yaml")["joint_names"]
    limits = _load_yaml("joint_limits.yaml")["joint_limits"]
    home = _load_yaml("home_pose.yaml")["joint_positions"]

    assert len(joint_names) == 23
    assert len(set(joint_names)) == 23
    assert list(limits) == joint_names
    assert list(home) == joint_names
    assert all(name.endswith("_joint") for name in joint_names)


def test_real_mujoco_and_mock_export_the_same_logical_resources():
    joint_names = _load_yaml("joint_order.yaml")["joint_names"]
    xacro_path = _description_share() / "urdf" / "yidong.urdf.xacro"

    for hardware in ("real", "mujoco", "mock"):
        document = xacro.process_file(
            str(xacro_path), mappings={"hardware": hardware}
        )
        root = ElementTree.fromstring(document.toxml())
        body_joints = [
            joint.attrib["name"]
            for joint in root.findall("./joint")
            if joint.attrib.get("type") != "fixed"
        ]
        control = root.find("./ros2_control")
        assert control is not None
        control_joints = [
            joint.attrib["name"] for joint in control.findall("./joint")
        ]
        assert body_joints == joint_names
        assert control_joints == joint_names
