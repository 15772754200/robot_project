#!/usr/bin/env python3

"""
Provide an independent MuJoCo viewer for simulation feedback.

This process subscribes to standard joint and base-pose feedback and renders
the latest state in its own MuJoCo model. It does not run physics, publish
commands, or participate in the 200 Hz control feedback path.
"""

from __future__ import annotations

import math
import os
import sys
import threading
import time
from dataclasses import dataclass
from typing import Iterable, List, Optional

try:
    import mujoco  # type: ignore
    import mujoco.viewer as mujoco_viewer  # type: ignore
except ImportError:
    mujoco = None
    mujoco_viewer = None

import rclpy
from ament_index_python.packages import get_package_share_directory
from geometry_msgs.msg import PoseStamped
from rclpy.executors import ExternalShutdownException
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy
from rclpy.qos import QoSPolicyKind
from rclpy.qos import QoSProfile
from rclpy.qos import ReliabilityPolicy
from rclpy.qos_overriding_options import QoSOverridingOptions
from sensor_msgs.msg import JointState
from std_msgs.msg import Float64

from joint_model_config import DEFAULT_INITIAL_BASE_POSE
from joint_model_config import DEFAULT_INITIAL_POSITIONS
from joint_model_config import DEFAULT_MODEL_RESOURCE_PATH
from joint_model_config import DEFAULT_SERIAL_JOINT_NAMES
from joint_model_config import DEFAULT_MUJOCO_JOINT_NAMES


ROPE_VISUAL_SEGMENT_COUNT = 12
QOS_OVERRIDES = QoSOverridingOptions(policy_kinds=(
    QoSPolicyKind.HISTORY,
    QoSPolicyKind.DEPTH,
    QoSPolicyKind.RELIABILITY,
    QoSPolicyKind.DURABILITY,
))


@dataclass(frozen=True)
class JointBinding:
    """Resolved one-DoF MuJoCo joint binding for viewer state updates."""

    ros_index: int
    ros_name: str
    mujoco_name: str
    qpos_adr: int
    qvel_adr: int


@dataclass(frozen=True)
class FloatingBaseBinding:
    """Resolved MuJoCo freejoint binding for viewer base pose updates."""

    joint_name: str
    qpos_adr: int
    qvel_adr: int


@dataclass(frozen=True)
class RopeVisualBinding:
    """Mocap objects used to render the paid-out safety rope."""

    trolley_mocap_id: int
    hook_site_id: int
    harness_site_id: int
    segment_mocap_ids: List[int]
    segment_geom_ids: List[int]
    default_length: float


class MujocoStateViewer(Node):
    """Render standard joint and base feedback in a passive MuJoCo viewer."""

    def __init__(self) -> None:
        super().__init__("mujoco_state_viewer")
        self._declare_parameters()

        self.state_topic = self._string_parameter("state_topic")
        self.base_pose_topic = self._string_parameter("base_pose_topic")
        self.rope_length_topic = self._string_parameter("rope_length_topic")
        self.model_path = self._model_path_parameter()
        self.floating_base_joint_name = self._string_parameter(
            "floating_base_joint_name"
        )
        self.joint_names = list(DEFAULT_SERIAL_JOINT_NAMES)
        self.mujoco_joint_names = list(DEFAULT_MUJOCO_JOINT_NAMES)
        self.initial_positions = list(DEFAULT_INITIAL_POSITIONS)
        self.initial_base_pose = list(DEFAULT_INITIAL_BASE_POSE)
        self.viewer_rate_hz = self._positive_float_parameter("viewer_rate_hz")
        self.tracking_body = self._string_parameter("tracking_body")

        self.latest_lock = threading.Lock()
        self.latest_position: Optional[List[float]] = None
        self.latest_base_pose: Optional[List[float]] = None

        self.model = self._load_model()
        self.data = mujoco.MjData(self.model)  # type: ignore[union-attr]
        self.bindings = self._resolve_joint_bindings()
        self.floating_base_binding = self._resolve_floating_base_binding()
        self.tracking_body_id = self._resolve_tracking_body()
        self.rope_visual = self._resolve_rope_visual()
        self.latest_rope_length = (
            0.0
            if self.rope_visual is None
            else self.rope_visual.default_length
        )
        self._set_initial_state()

        state_qos = QoSProfile(depth=1)
        state_qos.reliability = ReliabilityPolicy.BEST_EFFORT
        state_qos.durability = DurabilityPolicy.VOLATILE

        self.subscription = self.create_subscription(
            JointState,
            self.state_topic,
            self._feedback_callback,
            state_qos,
            qos_overriding_options=QOS_OVERRIDES,
        )
        self.base_pose_subscription = self.create_subscription(
            PoseStamped,
            self.base_pose_topic,
            self._base_pose_callback,
            state_qos,
            qos_overriding_options=QOS_OVERRIDES,
        )
        self.rope_length_subscription = None
        if self.rope_visual is not None:
            self.rope_length_subscription = self.create_subscription(
                Float64,
                self.rope_length_topic,
                self._rope_length_callback,
                state_qos,
                qos_overriding_options=QOS_OVERRIDES,
            )

        self.get_logger().info(
            "MuJoCo state viewer ready: model=%s state=%s base_pose=%s "
            "rope_length=%s rate=%.1fHz"
            % (
                self.model_path,
                self.state_topic,
                self.base_pose_topic,
                self.rope_length_topic,
                self.viewer_rate_hz,
            )
        )

    def _declare_parameters(self) -> None:
        self.declare_parameter("state_topic", "/joint_states")
        self.declare_parameter("base_pose_topic", "/mujoco/base_pose")
        self.declare_parameter("rope_length_topic", "/mujoco/rope/length")
        self.declare_parameter("model_path", DEFAULT_MODEL_RESOURCE_PATH)
        self.declare_parameter("floating_base_joint_name", "base_free_joint")
        self.declare_parameter("viewer_rate_hz", 30.0)
        self.declare_parameter("tracking_body", "base_link")

    def _string_parameter(self, name: str) -> str:
        value = str(self.get_parameter(name).value).strip()
        if not value:
            raise ValueError("Parameter %s must not be empty" % name)
        return value

    def _model_path_parameter(self) -> str:
        value = str(self.get_parameter("model_path").value).strip()
        if not value:
            value = DEFAULT_MODEL_RESOURCE_PATH
        value = self._resolve_resource_path(value)
        if not os.path.exists(value):
            raise FileNotFoundError("MuJoCo model_path does not exist: %s" % value)
        return value

    def _resolve_resource_path(self, value: str) -> str:
        if value.startswith("package://"):
            package_uri = value[len("package://"):]
            package_name, separator, relative_path = package_uri.partition("/")
            if not package_name or not separator or not relative_path:
                raise ValueError(
                    "Parameter model_path package URI must be "
                    "package://<package>/<relative_path>"
                )
            return os.path.join(
                get_package_share_directory(package_name),
                relative_path,
            )

        if os.path.isabs(value):
            return value
        raise ValueError(
            "Resource paths must be absolute or use "
            "package://<package>/<relative_path>"
        )

    def _positive_float_parameter(self, name: str) -> float:
        value = float(self.get_parameter(name).value)
        if not math.isfinite(value) or value <= 0.0:
            raise ValueError("Parameter %s must be finite and positive" % name)
        return value

    def _load_model(self):
        if mujoco is None:
            raise RuntimeError(
                "Python package 'mujoco' is not installed. "
                "Install it with: python3 -m pip install mujoco"
            )
        if mujoco_viewer is None:
            raise RuntimeError(
                "mujoco.viewer is not available; install Python package mujoco"
            )
        return mujoco.MjModel.from_xml_path(self.model_path)

    def _resolve_joint_bindings(self) -> List[JointBinding]:
        bindings: List[JointBinding] = []
        for ros_index, (ros_name, mujoco_name) in enumerate(
            zip(self.joint_names, self.mujoco_joint_names)
        ):
            if not mujoco_name:
                continue
            joint_id = mujoco.mj_name2id(  # type: ignore[union-attr]
                self.model,
                mujoco.mjtObj.mjOBJ_JOINT,  # type: ignore[union-attr]
                mujoco_name,
            )
            if joint_id < 0:
                raise ValueError("MuJoCo joint '%s' was not found" % mujoco_name)
            if self._joint_qpos_width(joint_id) != 1:
                raise ValueError("MuJoCo joint '%s' is not 1-DoF" % mujoco_name)
            if self._joint_qvel_width(joint_id) != 1:
                raise ValueError("MuJoCo joint '%s' is not 1-DoF" % mujoco_name)
            bindings.append(
                JointBinding(
                    ros_index=ros_index,
                    ros_name=ros_name,
                    mujoco_name=mujoco_name,
                    qpos_adr=int(self.model.jnt_qposadr[joint_id]),
                    qvel_adr=int(self.model.jnt_dofadr[joint_id]),
                )
            )
        return bindings

    def _resolve_floating_base_binding(self) -> FloatingBaseBinding:
        joint_id = mujoco.mj_name2id(  # type: ignore[union-attr]
            self.model,
            mujoco.mjtObj.mjOBJ_JOINT,  # type: ignore[union-attr]
            self.floating_base_joint_name,
        )
        if joint_id < 0:
            raise ValueError(
                "MuJoCo floating base joint '%s' was not found"
                % self.floating_base_joint_name
            )
        if self._joint_qpos_width(joint_id) != 7:
            raise ValueError(
                "MuJoCo floating base joint '%s' is not a freejoint"
                % self.floating_base_joint_name
            )
        if self._joint_qvel_width(joint_id) != 6:
            raise ValueError(
                "MuJoCo floating base joint '%s' is not a freejoint"
                % self.floating_base_joint_name
            )
        return FloatingBaseBinding(
            joint_name=self.floating_base_joint_name,
            qpos_adr=int(self.model.jnt_qposadr[joint_id]),
            qvel_adr=int(self.model.jnt_dofadr[joint_id]),
        )

    def _resolve_tracking_body(self) -> int:
        body_id = mujoco.mj_name2id(  # type: ignore[union-attr]
            self.model,
            mujoco.mjtObj.mjOBJ_BODY,  # type: ignore[union-attr]
            self.tracking_body,
        )
        if body_id < 0:
            raise ValueError(
                "MuJoCo tracking body '%s' was not found" % self.tracking_body
            )
        return int(body_id)

    def _resolve_rope_visual(self) -> Optional[RopeVisualBinding]:
        tendon_id = mujoco.mj_name2id(  # type: ignore[union-attr]
            self.model,
            mujoco.mjtObj.mjOBJ_TENDON,  # type: ignore[union-attr]
            "safety_rope",
        )
        if tendon_id < 0:
            return None

        trolley_body_id = mujoco.mj_name2id(  # type: ignore[union-attr]
            self.model,
            mujoco.mjtObj.mjOBJ_BODY,  # type: ignore[union-attr]
            "safety_trolley",
        )
        hook_site_id = mujoco.mj_name2id(  # type: ignore[union-attr]
            self.model,
            mujoco.mjtObj.mjOBJ_SITE,  # type: ignore[union-attr]
            "safety_rope_hook",
        )
        harness_site_id = mujoco.mj_name2id(  # type: ignore[union-attr]
            self.model,
            mujoco.mjtObj.mjOBJ_SITE,  # type: ignore[union-attr]
            "safety_harness",
        )
        if trolley_body_id < 0 or hook_site_id < 0 or harness_site_id < 0:
            raise ValueError("Safety-rope scene is missing trolley or endpoint sites")
        trolley_mocap_id = int(self.model.body_mocapid[trolley_body_id])
        if trolley_mocap_id < 0:
            raise ValueError("Safety trolley must be a MuJoCo mocap body")

        segment_mocap_ids: List[int] = []
        segment_geom_ids: List[int] = []
        for index in range(ROPE_VISUAL_SEGMENT_COUNT):
            suffix = "%02d" % index
            body_id = mujoco.mj_name2id(  # type: ignore[union-attr]
                self.model,
                mujoco.mjtObj.mjOBJ_BODY,  # type: ignore[union-attr]
                "safety_rope_visual_" + suffix,
            )
            geom_id = mujoco.mj_name2id(  # type: ignore[union-attr]
                self.model,
                mujoco.mjtObj.mjOBJ_GEOM,  # type: ignore[union-attr]
                "safety_rope_visual_geom_" + suffix,
            )
            if body_id < 0 or geom_id < 0:
                raise ValueError("Safety-rope visual segment %s is missing" % suffix)
            mocap_id = int(self.model.body_mocapid[body_id])
            if mocap_id < 0:
                raise ValueError(
                    "Safety-rope visual segment %s must be a mocap body" % suffix
                )
            segment_mocap_ids.append(mocap_id)
            segment_geom_ids.append(int(geom_id))

        return RopeVisualBinding(
            trolley_mocap_id=trolley_mocap_id,
            hook_site_id=int(hook_site_id),
            harness_site_id=int(harness_site_id),
            segment_mocap_ids=segment_mocap_ids,
            segment_geom_ids=segment_geom_ids,
            default_length=float(self.model.tendon_lengthspring[tendon_id, 1]),
        )

    def _joint_qpos_width(self, joint_id: int) -> int:
        start = int(self.model.jnt_qposadr[joint_id])
        if joint_id + 1 < self.model.njnt:
            end = int(self.model.jnt_qposadr[joint_id + 1])
        else:
            end = int(self.model.nq)
        return end - start

    def _joint_qvel_width(self, joint_id: int) -> int:
        start = int(self.model.jnt_dofadr[joint_id])
        if joint_id + 1 < self.model.njnt:
            end = int(self.model.jnt_dofadr[joint_id + 1])
        else:
            end = int(self.model.nv)
        return end - start

    def _set_initial_state(self) -> None:
        qpos_adr = self.floating_base_binding.qpos_adr
        for index, value in enumerate(self.initial_base_pose):
            self.data.qpos[qpos_adr + index] = value
        for binding in self.bindings:
            self.data.qpos[binding.qpos_adr] = (
                self.initial_positions[binding.ros_index]
            )
            self.data.qvel[binding.qvel_adr] = 0.0
        mujoco.mj_forward(self.model, self.data)  # type: ignore[union-attr]
        self._update_rope_visual(self.latest_rope_length)

    def _feedback_callback(self, msg: JointState) -> None:
        ordered_position = self._ordered_position(msg)
        if ordered_position is None:
            self.get_logger().warning(
                "Dropped invalid JointState feedback for viewer"
            )
            return
        with self.latest_lock:
            self.latest_position = ordered_position

    def _base_pose_callback(self, msg: PoseStamped) -> None:
        base_pose = self._pose_to_qpos(msg)
        if base_pose is None:
            self.get_logger().warning(
                "Dropped invalid MuJoCo base pose for viewer"
            )
            return
        with self.latest_lock:
            self.latest_base_pose = base_pose

    def _rope_length_callback(self, msg: Float64) -> None:
        rope_length = float(msg.data)
        if not math.isfinite(rope_length) or rope_length <= 0.0:
            return
        with self.latest_lock:
            self.latest_rope_length = rope_length

    def _ordered_position(self, msg: JointState) -> Optional[List[float]]:
        if not msg.name or len(msg.name) == 0:
            return None

        incoming_names = [str(value) for value in msg.name]
        if len(incoming_names) != len(self.joint_names):
            return None
        if len(set(incoming_names)) != len(incoming_names):
            return None
        incoming_index_by_name = {
            joint_name: index
            for index, joint_name in enumerate(incoming_names)
        }
        if any(
            joint_name not in incoming_index_by_name
            for joint_name in self.joint_names
        ):
            return None

        if msg.position is None or len(msg.position) != len(incoming_names):
            return None

        position = self._finite_array(msg.position, len(self.joint_names))
        if position is None:
            return None

        return [
            position[incoming_index_by_name[joint_name]]
            for joint_name in self.joint_names
        ]

    @staticmethod
    def _finite_array(
        values: Iterable[float], expected: int
    ) -> Optional[List[float]]:
        copied = [float(value) for value in values]
        if len(copied) != expected:
            return None
        if any(not math.isfinite(value) for value in copied):
            return None
        return copied

    @staticmethod
    def _pose_to_qpos(msg: PoseStamped) -> Optional[List[float]]:
        position = [
            float(msg.pose.position.x),
            float(msg.pose.position.y),
            float(msg.pose.position.z),
        ]
        quaternion_xyzw = [
            float(msg.pose.orientation.x),
            float(msg.pose.orientation.y),
            float(msg.pose.orientation.z),
            float(msg.pose.orientation.w),
        ]
        values = position + quaternion_xyzw
        if any(not math.isfinite(value) for value in values):
            return None

        qx, qy, qz, qw = quaternion_xyzw
        norm = math.sqrt(qw * qw + qx * qx + qy * qy + qz * qz)
        if norm <= 1.0e-12:
            return None

        return [
            position[0],
            position[1],
            position[2],
            qw / norm,
            qx / norm,
            qy / norm,
            qz / norm,
        ]

    def run_viewer(self) -> None:
        sync_period_sec = 1.0 / self.viewer_rate_hz
        with mujoco_viewer.launch_passive(self.model, self.data) as viewer:
            with viewer.lock():
                viewer.cam.type = mujoco.mjtCamera.mjCAMERA_TRACKING
                viewer.cam.trackbodyid = self.tracking_body_id
                viewer.cam.lookat[:] = self.data.xpos[self.tracking_body_id]
            while rclpy.ok() and viewer.is_running():
                self._apply_latest_position()
                viewer.sync()
                time.sleep(sync_period_sec)

    def _apply_latest_position(self) -> None:
        with self.latest_lock:
            position = (
                None
                if self.latest_position is None
                else list(self.latest_position)
            )
            base_pose = (
                None
                if self.latest_base_pose is None
                else list(self.latest_base_pose)
            )
            rope_length = self.latest_rope_length

        if base_pose is not None:
            qpos_adr = self.floating_base_binding.qpos_adr
            qvel_adr = self.floating_base_binding.qvel_adr
            for index, value in enumerate(base_pose):
                self.data.qpos[qpos_adr + index] = value
            for index in range(6):
                self.data.qvel[qvel_adr + index] = 0.0

        if position is not None:
            for binding in self.bindings:
                self.data.qpos[binding.qpos_adr] = position[binding.ros_index]
                self.data.qvel[binding.qvel_adr] = 0.0
        mujoco.mj_forward(self.model, self.data)  # type: ignore[union-attr]
        self._update_rope_visual(rope_length)

    def _update_rope_visual(self, rope_length: float) -> None:
        binding = self.rope_visual
        if binding is None:
            return

        harness = [
            float(value) for value in self.data.site_xpos[binding.harness_site_id]
        ]
        trolley_pos = self.data.mocap_pos[binding.trolley_mocap_id]
        trolley_pos[0] = harness[0]
        trolley_pos[1] = harness[1]
        mujoco.mj_forward(self.model, self.data)  # type: ignore[union-attr]

        hook = [float(value) for value in self.data.site_xpos[binding.hook_site_id]]
        harness = [
            float(value) for value in self.data.site_xpos[binding.harness_site_id]
        ]
        points = self._rope_curve_points(
            hook,
            harness,
            rope_length,
            len(binding.segment_mocap_ids),
        )
        for index, (mocap_id, geom_id) in enumerate(
            zip(binding.segment_mocap_ids, binding.segment_geom_ids)
        ):
            start = points[index]
            end = points[index + 1]
            delta = [end[axis] - start[axis] for axis in range(3)]
            length = math.sqrt(sum(value * value for value in delta))
            midpoint = [(start[axis] + end[axis]) * 0.5 for axis in range(3)]
            self.data.mocap_pos[mocap_id] = midpoint
            self.data.mocap_quat[mocap_id] = self._z_to_vector_quaternion(delta)
            self.model.geom_size[geom_id, 1] = max(0.5 * length, 1.0e-5)
        mujoco.mj_forward(self.model, self.data)  # type: ignore[union-attr]

    @staticmethod
    def _rope_curve_points(
        start: List[float],
        end: List[float],
        paid_out_length: float,
        segment_count: int,
    ) -> List[List[float]]:
        delta = [end[axis] - start[axis] for axis in range(3)]
        straight_length = math.sqrt(sum(value * value for value in delta))
        target_length = max(straight_length, paid_out_length)
        horizontal_length = math.hypot(delta[0], delta[1])
        if horizontal_length > 1.0e-9:
            sag_direction = [
                -delta[1] / horizontal_length,
                delta[0] / horizontal_length,
                0.0,
            ]
        else:
            sag_direction = [0.0, 1.0, 0.0]

        def points_for(amplitude: float) -> List[List[float]]:
            points: List[List[float]] = []
            for index in range(segment_count + 1):
                ratio = index / segment_count
                bow = amplitude * math.sin(math.pi * ratio)
                points.append([
                    start[axis] + ratio * delta[axis] + bow * sag_direction[axis]
                    for axis in range(3)
                ])
            return points

        def arc_length(points: List[List[float]]) -> float:
            total = 0.0
            for index in range(segment_count):
                segment = [
                    points[index + 1][axis] - points[index][axis]
                    for axis in range(3)
                ]
                total += math.sqrt(sum(value * value for value in segment))
            return total

        if target_length - straight_length <= 1.0e-6:
            return points_for(0.0)

        low = 0.0
        high = max(target_length, 0.1)
        while arc_length(points_for(high)) < target_length:
            high *= 2.0
        for _ in range(28):
            middle = 0.5 * (low + high)
            if arc_length(points_for(middle)) < target_length:
                low = middle
            else:
                high = middle
        return points_for(0.5 * (low + high))

    @staticmethod
    def _z_to_vector_quaternion(vector: List[float]) -> List[float]:
        length = math.sqrt(sum(value * value for value in vector))
        if length <= 1.0e-12:
            return [1.0, 0.0, 0.0, 0.0]
        x, y, z = [value / length for value in vector]
        if z < -0.999999:
            return [0.0, 1.0, 0.0, 0.0]
        scale = math.sqrt(2.0 * (1.0 + z))
        return [0.5 * scale, -y / scale, x / scale, 0.0]


def main(args: Optional[List[str]] = None) -> None:
    def spin_executor(executor: SingleThreadedExecutor) -> None:
        try:
            executor.spin()
        except ExternalShutdownException:
            pass
        except KeyboardInterrupt:
            pass
        except Exception:
            if rclpy.ok():
                raise

    rclpy.init(args=args)
    node = None
    executor = None
    spin_thread = None
    try:
        node = MujocoStateViewer()
        executor = SingleThreadedExecutor()
        executor.add_node(node)
        spin_thread = threading.Thread(
            target=spin_executor,
            args=(executor,),
            daemon=True,
        )
        spin_thread.start()
        node.run_viewer()
    except KeyboardInterrupt:
        pass
    finally:
        if executor is not None:
            executor.shutdown()
        if node is not None:
            node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
        if spin_thread is not None:
            spin_thread.join(timeout=1.0)
    sys.exit(0)


if __name__ == "__main__":
    main()
