"""Contract tests for the MuJoCo viewer's logical-joint bindings."""

from pathlib import Path
from types import SimpleNamespace
import sys

from ament_index_python.packages import get_package_share_directory
import mujoco
import pytest


SCRIPT_DIR = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))

from joint_model_config import DEFAULT_MODEL_RESOURCE_PATH  # noqa: E402
from joint_model_config import DEFAULT_MUJOCO_JOINT_NAMES  # noqa: E402
from joint_model_config import DEFAULT_INITIAL_BASE_POSE  # noqa: E402
from joint_model_config import DEFAULT_INITIAL_POSITIONS  # noqa: E402
from joint_model_config import DEFAULT_SERIAL_JOINT_NAMES  # noqa: E402
from mujoco_state_viewer import MujocoStateViewer  # noqa: E402
from mujoco_state_viewer import ROPE_VISUAL_SEGMENT_COUNT  # noqa: E402


def _resolve_package_uri(resource: str) -> Path:
    package_uri = resource.removeprefix("package://")
    package_name, relative_path = package_uri.split("/", 1)
    return Path(get_package_share_directory(package_name)) / relative_path


def _binding_context(mujoco_joint_names: list[str]) -> SimpleNamespace:
    context = SimpleNamespace(
        model=mujoco.MjModel.from_xml_path(
            str(_resolve_package_uri(DEFAULT_MODEL_RESOURCE_PATH))
        ),
        joint_names=list(DEFAULT_SERIAL_JOINT_NAMES),
        mujoco_joint_names=mujoco_joint_names,
    )
    context._joint_qpos_width = lambda joint_id: (
        MujocoStateViewer._joint_qpos_width(context, joint_id)
    )
    context._joint_qvel_width = lambda joint_id: (
        MujocoStateViewer._joint_qvel_width(context, joint_id)
    )
    return context


def test_default_bindings_skip_only_unmodeled_head_joint() -> None:
    context = _binding_context(list(DEFAULT_MUJOCO_JOINT_NAMES))

    bindings = MujocoStateViewer._resolve_joint_bindings(context)

    assert DEFAULT_MUJOCO_JOINT_NAMES.count("") == 1
    head_index = DEFAULT_SERIAL_JOINT_NAMES.index("head_yaw_joint")
    assert DEFAULT_MUJOCO_JOINT_NAMES[head_index] == ""
    assert len(bindings) == 22
    assert {binding.ros_name for binding in bindings} == (
        set(DEFAULT_SERIAL_JOINT_NAMES) - {"head_yaw_joint"}
    )
    assert mujoco.mj_name2id(
        context.model,
        mujoco.mjtObj.mjOBJ_JOINT,
        "head_yaw_joint",
    ) == -1


def test_canonical_home_pose_covers_the_logical_interface() -> None:
    assert len(DEFAULT_INITIAL_POSITIONS) == len(DEFAULT_SERIAL_JOINT_NAMES)
    assert len(DEFAULT_INITIAL_BASE_POSE) == 7
    quaternion_norm = sum(
        value * value for value in DEFAULT_INITIAL_BASE_POSE[3:]
    ) ** 0.5
    assert quaternion_norm == pytest.approx(1.0)


def test_relative_resource_paths_are_rejected() -> None:
    with pytest.raises(ValueError, match="absolute"):
        MujocoStateViewer._resolve_resource_path(None, "legacy/scene.xml")


def test_missing_nonignored_joint_still_fails() -> None:
    mujoco_joint_names = list(DEFAULT_MUJOCO_JOINT_NAMES)
    mujoco_joint_names[0] = "missing_policy_joint"
    context = _binding_context(mujoco_joint_names)

    with pytest.raises(ValueError, match="missing_policy_joint"):
        MujocoStateViewer._resolve_joint_bindings(context)


def test_default_tracking_body_resolves_to_robot_base() -> None:
    context = SimpleNamespace(
        model=mujoco.MjModel.from_xml_path(
            str(_resolve_package_uri(DEFAULT_MODEL_RESOURCE_PATH))
        ),
        tracking_body="base_link",
    )

    body_id = MujocoStateViewer._resolve_tracking_body(context)

    assert body_id == mujoco.mj_name2id(
        context.model,
        mujoco.mjtObj.mjOBJ_BODY,
        "base_link",
    )


def test_missing_tracking_body_fails() -> None:
    context = SimpleNamespace(
        model=mujoco.MjModel.from_xml_path(
            str(_resolve_package_uri(DEFAULT_MODEL_RESOURCE_PATH))
        ),
        tracking_body="missing_tracking_body",
    )

    with pytest.raises(ValueError, match="missing_tracking_body"):
        MujocoStateViewer._resolve_tracking_body(context)


def test_safety_rope_scene_contains_complete_visual_chain() -> None:
    model = mujoco.MjModel.from_xml_path(
        str(_resolve_package_uri(DEFAULT_MODEL_RESOURCE_PATH))
    )

    for index in range(ROPE_VISUAL_SEGMENT_COUNT):
        suffix = f"{index:02d}"
        assert mujoco.mj_name2id(
            model,
            mujoco.mjtObj.mjOBJ_BODY,
            "safety_rope_visual_" + suffix,
        ) >= 0
        assert mujoco.mj_name2id(
            model,
            mujoco.mjtObj.mjOBJ_GEOM,
            "safety_rope_visual_geom_" + suffix,
        ) >= 0


@pytest.mark.parametrize(
    "scene_name",
    [
        "scene.xml",
        "scene_safety_rope.xml",
        "scene_slope.xml",
        "scene_stair.xml",
    ],
)
def test_supported_scenes_use_the_same_twenty_two_dof_plant(
    scene_name: str,
) -> None:
    model_path = _resolve_package_uri(DEFAULT_MODEL_RESOURCE_PATH)
    model = mujoco.MjModel.from_xml_path(str(model_path.with_name(scene_name)))

    assert model.nu == 22
    assert mujoco.mj_name2id(
        model,
        mujoco.mjtObj.mjOBJ_JOINT,
        "base_free_joint",
    ) >= 0
    assert mujoco.mj_name2id(
        model,
        mujoco.mjtObj.mjOBJ_JOINT,
        "head_yaw_joint",
    ) == -1


def test_rope_curve_represents_paid_out_slack() -> None:
    start = [0.0, 0.0, 1.0]
    end = [0.0, 0.0, 0.2]
    paid_out_length = 1.0

    points = MujocoStateViewer._rope_curve_points(
        start,
        end,
        paid_out_length,
        ROPE_VISUAL_SEGMENT_COUNT,
    )
    arc_length = sum(
        sum(
            (points[index + 1][axis] - points[index][axis]) ** 2
            for axis in range(3)
        ) ** 0.5
        for index in range(ROPE_VISUAL_SEGMENT_COUNT)
    )

    assert points[0] == pytest.approx(start)
    assert points[-1] == pytest.approx(end)
    assert arc_length == pytest.approx(paid_out_length, abs=1.0e-6)
    assert max(abs(point[1]) for point in points) > 0.0
