#!/usr/bin/env python3
"""Real-robot arm teach and playback adapter for the current hhros2 stack."""

from __future__ import annotations

import json
import math
import os
import threading
import time
from datetime import datetime, timezone
from typing import Any

import rclpy
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy

from hhros2_interfaces.msg import ArbitrationMode, JointMotor
from hhros2_interfaces.srv import SetControlMode
from sensor_msgs.msg import JointState
from wave_control_msgs.msg import TeachRecord
from wave_control_msgs.srv import TeachControl


ARM_JOINTS = [
    "left_shoulder_pitch_joint",
    "left_shoulder_roll_joint",
    "left_shoulder_yaw_joint",
    "left_elbow_joint",
    "right_shoulder_pitch_joint",
    "right_shoulder_roll_joint",
    "right_shoulder_yaw_joint",
    "right_elbow_joint",
]

JOINT_NAMES = [
    "left_hip_pitch_joint",
    "left_hip_roll_joint",
    "left_hip_yaw_joint",
    "left_knee_joint",
    "left_ankle_pitch_joint",
    "left_ankle_roll_joint",
    "right_hip_pitch_joint",
    "right_hip_roll_joint",
    "right_hip_yaw_joint",
    "right_knee_joint",
    "right_ankle_pitch_joint",
    "right_ankle_roll_joint",
    "waist_yaw_joint",
    "waist_pitch_joint",
    *ARM_JOINTS,
]
ARM_INDEXES = [JOINT_NAMES.index(name) for name in ARM_JOINTS]

# Kept local so this external adapter does not acquire a runtime dependency on
# the main workspace. Values mirror hhros2_description/config/joint_limits.yaml.
ARM_POSITION_LIMITS = {
    "left_shoulder_pitch_joint": (-2.8798, 2.3562),
    "left_shoulder_roll_joint": (-1.4486, 1.5708),
    "left_shoulder_yaw_joint": (-3.1416, 3.1416),
    "left_elbow_joint": (-0.8727, 2.3562),
    "right_shoulder_pitch_joint": (-2.3562, 2.8798),
    "right_shoulder_roll_joint": (-1.5708, 1.4486),
    "right_shoulder_yaw_joint": (-3.1416, 3.1416),
    "right_elbow_joint": (-2.3562, 0.8727),
}


def _finite_vector(values: list[float], expected_size: int) -> bool:
    return len(values) == expected_size and all(math.isfinite(float(value)) for value in values)


class TeachManagerReal(Node):
    """Keep the legacy TeachControl API while using the current control path."""

    def __init__(self) -> None:
        super().__init__("teach_manager_real")

        self.declare_parameter("joint_state_topic", "/joint_states")
        self.declare_parameter("command_topic", "/humanoid_base_controller/reference")
        self.declare_parameter("arbitration_topic", "/humanoid/arbitration_mode")
        self.declare_parameter("control_mode_service", "/hhros2_core/set_control_mode")
        self.declare_parameter("trajectory_dir", "trajectories")
        self.declare_parameter("record_frequency", 50.0)
        self.declare_parameter("playback_frequency", 50.0)
        self.declare_parameter("arm_kp", 50.0)
        self.declare_parameter("arm_kd", 2.0)
        self.declare_parameter("require_stand_mode", True)
        self.declare_parameter("feedback_timeout_sec", 0.25)
        self.declare_parameter("max_start_position_error_rad", 0.25)
        self.declare_parameter("max_trajectory_velocity_rad_s", 1.0)
        self.declare_parameter("max_trajectory_acceleration_rad_s2", 5.0)
        self.declare_parameter("require_exclusive_command_topic", True)

        self.joint_state_topic = str(self.get_parameter("joint_state_topic").value)
        self.command_topic = str(self.get_parameter("command_topic").value)
        self.arbitration_topic = str(self.get_parameter("arbitration_topic").value)
        self.control_mode_service = str(self.get_parameter("control_mode_service").value)
        self.trajectory_dir = os.path.abspath(str(self.get_parameter("trajectory_dir").value))
        self.default_record_frequency = float(self.get_parameter("record_frequency").value)
        self.playback_frequency = max(10.0, float(self.get_parameter("playback_frequency").value))
        self.arm_kp = float(self.get_parameter("arm_kp").value)
        self.arm_kd = float(self.get_parameter("arm_kd").value)
        self.require_stand_mode = bool(self.get_parameter("require_stand_mode").value)
        self.feedback_timeout_sec = max(
            0.01, float(self.get_parameter("feedback_timeout_sec").value)
        )
        self.max_start_position_error_rad = max(
            0.0, float(self.get_parameter("max_start_position_error_rad").value)
        )
        self.max_trajectory_velocity_rad_s = max(
            0.0, float(self.get_parameter("max_trajectory_velocity_rad_s").value)
        )
        self.max_trajectory_acceleration_rad_s2 = max(
            0.0, float(self.get_parameter("max_trajectory_acceleration_rad_s2").value)
        )
        self.require_exclusive_command_topic = bool(
            self.get_parameter("require_exclusive_command_topic").value
        )
        os.makedirs(self.trajectory_dir, exist_ok=True)

        self.callback_group = ReentrantCallbackGroup()
        self.lock = threading.RLock()
        self.recording_state = "idle"
        self.playback_state = "idle"
        self.trajectory_data: list[dict[str, Any]] = []
        self.current_trajectory: list[dict[str, Any]] = []
        self.current_record_id = ""
        self.record_start_time = 0.0
        self.record_start_monotonic = 0.0
        self.playback_start_time = 0.0
        self.record_frequency = self.default_record_frequency
        self.play_speed = 1.0
        self.loop_playback = False
        self.last_record_time = 0.0
        self.latest_positions: dict[str, float] = {}
        self.latest_velocities: dict[str, float] = {}
        self.latest_efforts: dict[str, float] = {}
        self.last_joint_state_monotonic: float | None = None
        self.active_mode: int | None = None
        self.playback_timer = None
        self.mode_wait_timer = None

        sensor_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
        )
        self.joint_state_sub = self.create_subscription(
            JointState,
            self.joint_state_topic,
            self._joint_state_callback,
            sensor_qos,
            callback_group=self.callback_group,
        )
        self.mode_sub = self.create_subscription(
            ArbitrationMode,
            self.arbitration_topic,
            self._mode_callback,
            10,
            callback_group=self.callback_group,
        )
        self.command_pub = None
        self.mode_client = self.create_client(SetControlMode, self.control_mode_service)
        self.teach_service = self.create_service(
            TeachControl,
            "teach_control_real",
            self._handle_teach_control,
            callback_group=self.callback_group,
        )
        self.record_status_pub = self.create_publisher(TeachRecord, "teach_record_status", 10)
        self.get_logger().info(
            f"Current-stack arm manager ready: feedback={self.joint_state_topic}, "
            f"command={self.command_topic}, arms={len(ARM_JOINTS)}"
        )

    def _mode_callback(self, msg: ArbitrationMode) -> None:
        self.active_mode = int(msg.active_mode)
        if (
            self.playback_state == "waiting_mode"
            and self.active_mode == ArbitrationMode.MODE_MOTION
        ):
            self._start_playback_timer()

    def _joint_state_callback(self, msg: JointState) -> None:
        names = list(msg.name)
        positions = list(msg.position)
        if len(names) != len(positions):
            return
        velocities = list(msg.velocity) if len(msg.velocity) == len(names) else [0.0] * len(names)
        efforts = list(msg.effort) if len(msg.effort) == len(names) else [0.0] * len(names)

        with self.lock:
            self.latest_positions = {
                name: float(positions[index])
                for index, name in enumerate(names)
                if math.isfinite(float(positions[index]))
            }
            self.latest_velocities = {
                name: float(velocities[index])
                for index, name in enumerate(names)
                if math.isfinite(float(velocities[index]))
            }
            self.latest_efforts = {
                name: float(efforts[index])
                for index, name in enumerate(names)
                if math.isfinite(float(efforts[index]))
            }
            self.last_joint_state_monotonic = time.monotonic()
            if self.recording_state != "recording":
                return
            now = self.last_joint_state_monotonic
            if now - self.last_record_time < 1.0 / self.record_frequency:
                return
            if not all(name in self.latest_positions for name in ARM_JOINTS):
                return
            if not self._positions_within_arm_limits(
                [self.latest_positions[name] for name in ARM_JOINTS]
            ):
                self.get_logger().warn("Skipped out-of-limit arm feedback while recording")
                return
            self.trajectory_data.append(
                {
                    "time": now - self.record_start_monotonic,
                    "positions": [self.latest_positions[name] for name in ARM_JOINTS],
                    "velocities": [self.latest_velocities.get(name, 0.0) for name in ARM_JOINTS],
                    "efforts": [self.latest_efforts.get(name, 0.0) for name in ARM_JOINTS],
                }
            )
            self.last_record_time = now

    def _handle_teach_control(self, request: TeachControl.Request, response: TeachControl.Response):
        try:
            handlers = {
                "start_record": self._start_recording,
                "stop_record": self._stop_recording,
                "pause_record": self._pause_recording,
                "resume_record": self._resume_recording,
                "save_trajectory": self._save_trajectory,
                "load_trajectory": self._load_trajectory,
                "play_trajectory": self._start_playback,
                "stop_playback": self._stop_playback,
            }
            handler = handlers.get(request.command)
            if handler is None:
                response.success = False
                response.message = f"Unknown command: {request.command}"
            else:
                handler(request, response)
        except Exception as exc:
            response.success = False
            response.message = f"Error processing command: {exc}"
            self.get_logger().error(response.message)
        response.current_state = f"record:{self.recording_state}, playback:{self.playback_state}"
        response.frame_count = len(self.trajectory_data)
        response.progress = self._progress()
        return response

    def _stand_required(self) -> bool:
        return not self.require_stand_mode or self.active_mode == ArbitrationMode.MODE_STAND

    def _feedback_error(self, required_names: list[str]) -> str | None:
        with self.lock:
            last_update = self.last_joint_state_monotonic
            missing = [name for name in required_names if name not in self.latest_positions]
        if last_update is None:
            return "No /joint_states feedback has been received"
        age = time.monotonic() - last_update
        if age > self.feedback_timeout_sec:
            return (
                f"/joint_states feedback is stale ({age:.3f}s > "
                f"{self.feedback_timeout_sec:.3f}s)"
            )
        if missing:
            return f"/joint_states feedback is missing: {', '.join(missing)}"
        return None

    @staticmethod
    def _positions_within_arm_limits(positions: list[float]) -> bool:
        return all(
            lower <= value <= upper
            for name, value in zip(ARM_JOINTS, positions)
            for lower, upper in [ARM_POSITION_LIMITS[name]]
        )

    def _exclusive_command_topic_error(self) -> str | None:
        if not self.require_exclusive_command_topic:
            return None
        publisher_count = self.count_publishers(self.command_topic)
        if publisher_count:
            return (
                f"Refusing playback: {self.command_topic} already has "
                f"{publisher_count} publisher(s)"
            )
        return None

    def _start_recording(self, request, response) -> None:
        if self.recording_state == "recording" or self.playback_state != "idle":
            response.success = False
            response.message = "Cannot record while recording or playing"
            return
        if not self._stand_required():
            response.success = False
            response.message = "Recording requires MODE_STAND"
            return
        feedback_error = self._feedback_error(ARM_JOINTS)
        if feedback_error:
            response.success = False
            response.message = feedback_error
            return
        with self.lock:
            arm_positions = [self.latest_positions[name] for name in ARM_JOINTS]
        if not self._positions_within_arm_limits(arm_positions):
            response.success = False
            response.message = "Current arm feedback is outside configured joint limits"
            return
        with self.lock:
            self.trajectory_data = []
            self.current_record_id = f"record_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            self.record_start_time = time.time()
            self.record_start_monotonic = time.monotonic()
            self.record_frequency = max(
                1.0,
                min(100.0, float(request.record_frequency or self.default_record_frequency)),
            )
            self.last_record_time = 0.0
            self.recording_state = "recording"
        self._publish_status()
        response.success = True
        response.message = (
            f"Started recording {len(ARM_JOINTS)} arm joints at "
            f"{self.record_frequency:.1f}Hz"
        )

    def _stop_recording(self, _request, response) -> None:
        if self.recording_state not in ("recording", "paused"):
            response.success = False
            response.message = "Not currently recording"
            return
        self.recording_state = "completed"
        self._publish_status()
        response.success = True
        response.message = f"Stopped recording. Recorded {len(self.trajectory_data)} frames"

    def _pause_recording(self, _request, response) -> None:
        if self.recording_state != "recording":
            response.success = False
            response.message = "Not currently recording"
            return
        self.recording_state = "paused"
        self._publish_status()
        response.success = True
        response.message = "Recording paused"

    def _resume_recording(self, _request, response) -> None:
        if self.recording_state != "paused":
            response.success = False
            response.message = "Recording is not paused"
            return
        if not self._stand_required():
            response.success = False
            response.message = "Recording requires MODE_STAND"
            return
        feedback_error = self._feedback_error(ARM_JOINTS)
        if feedback_error:
            response.success = False
            response.message = feedback_error
            return
        self.recording_state = "recording"
        self.last_record_time = 0.0
        self._publish_status()
        response.success = True
        response.message = "Recording resumed"

    def _trajectory_file(self, trajectory_id: str) -> str:
        if not trajectory_id or os.path.basename(trajectory_id) != trajectory_id:
            raise ValueError("trajectory_id must not contain a path")
        if not trajectory_id.endswith(".json"):
            trajectory_id += ".json"
        return os.path.join(self.trajectory_dir, trajectory_id)

    def _normalise_frames(self, data: Any) -> list[dict[str, Any]]:
        frames = data.get("frames", []) if isinstance(data, dict) else data
        joint_names = data.get("joint_names", []) if isinstance(data, dict) else ARM_JOINTS
        if joint_names != ARM_JOINTS:
            raise ValueError("trajectory joint_names must be the canonical 8 arm joints")
        if not isinstance(frames, list) or len(frames) < 2:
            raise ValueError("trajectory must contain at least two frames")

        result: list[dict[str, Any]] = []
        first_timestamp = float(frames[0].get("timestamp", 0.0))
        last_time: float | None = None
        segment_velocities: list[list[float]] = []
        for frame_index, frame in enumerate(frames):
            positions = [float(value) for value in frame.get("positions", [])]
            if not _finite_vector(positions, len(ARM_JOINTS)):
                raise ValueError("trajectory contains invalid arm positions")
            if not self._positions_within_arm_limits(positions):
                raise ValueError(
                    f"trajectory frame {frame_index} exceeds configured arm joint limits"
                )
            frame_time = float(frame.get("time", frame.get("timestamp", 0.0) - first_timestamp))
            if not math.isfinite(frame_time):
                raise ValueError("trajectory frame times must be finite")
            if last_time is not None and frame_time <= last_time:
                raise ValueError("trajectory frame times must be strictly increasing")
            if result:
                delta_time = frame_time - result[-1]["time"]
                velocity = [
                    (position - previous) / delta_time
                    for position, previous in zip(positions, result[-1]["positions"])
                ]
                if any(abs(value) > self.max_trajectory_velocity_rad_s for value in velocity):
                    raise ValueError(
                        f"trajectory segment {frame_index - 1}->{frame_index} exceeds "
                        "the configured velocity limit"
                    )
                if segment_velocities:
                    previous_velocity = segment_velocities[-1]
                    previous_delta_time = result[-1]["time"] - result[-2]["time"]
                    acceleration_time = 0.5 * (delta_time + previous_delta_time)
                    acceleration = [
                        (value - previous) / acceleration_time
                        for value, previous in zip(velocity, previous_velocity)
                    ]
                    if any(
                        abs(value) > self.max_trajectory_acceleration_rad_s2
                        for value in acceleration
                    ):
                        raise ValueError(
                            f"trajectory segment {frame_index - 1}->{frame_index} exceeds "
                            "the configured acceleration limit"
                        )
                segment_velocities.append(velocity)
            result.append({"time": frame_time, "positions": positions})
            last_time = frame_time
        if result[-1]["time"] <= result[0]["time"]:
            raise ValueError("trajectory duration must be positive")
        return result

    def _load_frames(self, trajectory_id: str) -> list[dict[str, Any]]:
        with open(self._trajectory_file(trajectory_id), "r", encoding="utf-8") as stream:
            return self._normalise_frames(json.load(stream))

    def _save_trajectory(self, request, response) -> None:
        if len(self.trajectory_data) < 2:
            response.success = False
            response.message = "At least two recorded frames are required"
            return
        try:
            checked_frames = self._normalise_frames(self.trajectory_data)
        except ValueError as exc:
            response.success = False
            response.message = f"Recorded trajectory rejected: {exc}"
            return
        trajectory_id = request.trajectory_id or self.current_record_id
        path = self._trajectory_file(trajectory_id)
        payload = {
            "id": trajectory_id.removesuffix(".json"),
            "description": request.description or "Recorded 8-joint arm motion",
            "joint_names": ARM_JOINTS,
            "num_joints": len(ARM_JOINTS),
            "record_frequency": self.record_frequency,
            "record_time": datetime.now(timezone.utc).isoformat(),
            "data_source": "sensor_msgs/JointState:/joint_states",
            "frames": checked_frames,
        }
        with open(path, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2)
        response.success = True
        response.message = f"Trajectory saved to {path}"

    def _load_trajectory(self, request, response) -> None:
        self.current_trajectory = self._load_frames(request.trajectory_id)
        self.trajectory_data = list(self.current_trajectory)
        response.success = True
        response.message = f"Loaded {len(self.current_trajectory)} frames"

    def _start_playback(self, request, response) -> None:
        if self.playback_state != "idle" or self.recording_state == "recording":
            response.success = False
            response.message = "Cannot play while recording or already playing"
            return
        feedback_error = self._feedback_error(JOINT_NAMES)
        if feedback_error:
            response.success = False
            response.message = feedback_error
            return

        frames = (
            self._load_frames(request.trajectory_id)
            if request.trajectory_id
            else self._normalise_frames(self.trajectory_data)
        )
        with self.lock:
            arm_feedback = [self.latest_positions[name] for name in ARM_JOINTS]
        start_error = max(
            abs(target - actual)
            for target, actual in zip(frames[0]["positions"], arm_feedback)
        )
        if start_error > self.max_start_position_error_rad:
            response.success = False
            response.message = (
                f"Trajectory first frame differs from arm feedback by {start_error:.3f}rad "
                f"(limit {self.max_start_position_error_rad:.3f}rad)"
            )
            return
        topic_error = self._exclusive_command_topic_error()
        if topic_error:
            response.success = False
            response.message = topic_error
            return
        self.current_trajectory = frames
        self.play_speed = max(0.1, min(10.0, float(request.play_speed or 1.0)))
        self.loop_playback = bool(request.loop_playback)
        self.playback_state = "starting"
        if not self.mode_client.wait_for_service(timeout_sec=1.0):
            self.playback_state = "idle"
            response.success = False
            response.message = "set_control_mode service unavailable"
            return

        mode_request = SetControlMode.Request()
        mode_request.mode = ArbitrationMode.MODE_MOTION
        future = self.mode_client.call_async(mode_request)
        future.add_done_callback(self._motion_mode_result)
        response.success = True
        response.message = "Playback accepted; requesting MODE_MOTION"

    def _motion_mode_result(self, future) -> None:
        try:
            result = future.result()
        except Exception as exc:
            self._abort_playback(f"MODE_MOTION request failed: {exc}")
            return
        if self.playback_state != "starting":
            return
        if not result.success:
            self._abort_playback(f"MODE_MOTION refused: {result.message}")
            return
        self.playback_state = "waiting_mode"
        self.mode_wait_timer = self.create_timer(
            2.0,
            self._mode_wait_timeout,
            callback_group=self.callback_group,
        )
        if self.active_mode == ArbitrationMode.MODE_MOTION:
            self._start_playback_timer()

    def _mode_wait_timeout(self) -> None:
        self._cancel_mode_wait_timer()
        if self.playback_state == "waiting_mode":
            self._abort_playback("MODE_MOTION arbitration update was not received")

    def _start_playback_timer(self) -> None:
        if self.playback_state != "waiting_mode":
            return
        feedback_error = self._feedback_error(JOINT_NAMES)
        if feedback_error:
            self._abort_playback(feedback_error)
            return
        topic_error = self._exclusive_command_topic_error()
        if topic_error:
            self._abort_playback(topic_error)
            return
        self._cancel_mode_wait_timer()
        self.playback_state = "playing"
        self.playback_start_time = time.monotonic()
        self.command_pub = self.create_publisher(JointMotor, self.command_topic, 10)
        self.playback_timer = self.create_timer(
            1.0 / self.playback_frequency,
            self._execute_playback,
            callback_group=self.callback_group,
        )

    def _execute_playback(self) -> None:
        if self.playback_state != "playing":
            return
        duration = self.current_trajectory[-1]["time"] - self.current_trajectory[0]["time"]
        elapsed = (time.monotonic() - self.playback_start_time) * self.play_speed
        if elapsed >= duration:
            if self.loop_playback:
                self.playback_start_time = time.monotonic()
                elapsed = 0.0
            else:
                self._publish_command(self.current_trajectory[-1]["positions"])
                self._finish_playback()
                return
        self._publish_command(self._interpolate(self.current_trajectory[0]["time"] + elapsed))

    def _interpolate(self, target_time: float) -> list[float]:
        frames = self.current_trajectory
        if target_time <= frames[0]["time"]:
            return list(frames[0]["positions"])
        for previous, following in zip(frames, frames[1:]):
            if target_time <= following["time"]:
                span = following["time"] - previous["time"]
                alpha = (target_time - previous["time"]) / span if span > 0.0 else 0.0
                return [
                    start + alpha * (finish - start)
                    for start, finish in zip(previous["positions"], following["positions"])
                ]
        return list(frames[-1]["positions"])

    def _publish_command(self, arm_positions: list[float]) -> None:
        feedback_error = self._feedback_error(JOINT_NAMES)
        if feedback_error:
            self._abort_playback(feedback_error)
            return
        with self.lock:
            positions = dict(self.latest_positions)
        if not self._positions_within_arm_limits(arm_positions):
            self._abort_playback("Interpolated arm target is outside configured joint limits")
            return
        command = JointMotor()
        command.header.stamp = self.get_clock().now().to_msg()
        command.joint_names = JOINT_NAMES
        command.position = [positions[name] for name in JOINT_NAMES]
        command.velocity = [0.0] * len(JOINT_NAMES)
        command.effort = [0.0] * len(JOINT_NAMES)
        command.kp = [0.0] * len(JOINT_NAMES)
        command.kd = [0.0] * len(JOINT_NAMES)
        for arm_index, joint_index in enumerate(ARM_INDEXES):
            command.position[joint_index] = arm_positions[arm_index]
            command.kp[joint_index] = self.arm_kp
            command.kd[joint_index] = self.arm_kd
        if self.command_pub is not None:
            self.command_pub.publish(command)

    def _finish_playback(self) -> None:
        self._cancel_playback_timer()
        self._cancel_mode_wait_timer()
        self.playback_state = "idle"
        self._request_stand()

    def _abort_playback(self, reason: str) -> None:
        self.get_logger().error(f"Arm playback aborted: {reason}")
        self._cancel_playback_timer()
        self._cancel_mode_wait_timer()
        self.playback_state = "idle"
        self._request_stand()

    def _stop_playback(self, _request, response) -> None:
        if self.playback_state not in ("starting", "waiting_mode", "playing"):
            response.success = False
            response.message = "Not currently playing"
            return
        self._finish_playback()
        response.success = True
        response.message = "Playback stopped; requesting MODE_STAND"

    def _request_stand(self) -> None:
        if not self.mode_client.service_is_ready():
            return
        request = SetControlMode.Request()
        request.mode = ArbitrationMode.MODE_STAND
        self.mode_client.call_async(request)

    def _cancel_playback_timer(self) -> None:
        if self.playback_timer is not None:
            self.playback_timer.cancel()
            self.playback_timer = None
        if self.command_pub is not None:
            self.destroy_publisher(self.command_pub)
            self.command_pub = None

    def _cancel_mode_wait_timer(self) -> None:
        if self.mode_wait_timer is not None:
            self.mode_wait_timer.cancel()
            self.mode_wait_timer = None

    def _progress(self) -> float:
        if self.playback_state != "playing" or not self.current_trajectory:
            return 0.0
        duration = self.current_trajectory[-1]["time"] - self.current_trajectory[0]["time"]
        if duration <= 0.0:
            return 0.0
        elapsed = (time.monotonic() - self.playback_start_time) * self.play_speed
        return min(1.0, max(0.0, elapsed / duration))

    def _publish_status(self) -> None:
        status = TeachRecord()
        status.record_id = self.current_record_id
        status.state = self.recording_state
        status.frame_count = len(self.trajectory_data)
        status.duration = float(time.time() - self.record_start_time) if self.record_start_time else 0.0
        status.frequency = float(self.record_frequency)
        status.description = "Current-stack 8-joint arm recording"
        self.record_status_pub.publish(status)

    def destroy_node(self):
        if self.playback_state != "idle":
            self._request_stand()
        self._cancel_playback_timer()
        self._cancel_mode_wait_timer()
        return super().destroy_node()


def main(args=None) -> None:
    rclpy.init(args=args)
    node = TeachManagerReal()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
