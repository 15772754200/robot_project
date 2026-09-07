#!/usr/bin/env python3
"""Recording and playback manager for the isolated eight-joint simulator."""

import json
import math
import threading
import time
from datetime import datetime
from pathlib import Path

import rclpy
from rclpy.node import Node

from wave_control_system.legacy_sim_model import ARM_JOINTS, JOINT_COUNT, MOTOR_IDS
from wave_control_msgs.msg import JointTrajectory, MotorCommand, TeachRecord, WaveStatus
from wave_control_msgs.srv import TeachControl


class TeachManager(Node):
    """Record simulated feedback and replay it through the legacy simulator."""

    PLAYBACK_RATE_HZ = 50.0

    def __init__(self):
        super().__init__("teach_manager")

        self.declare_parameter(
            "trajectory_dir", str(Path.home() / "hhros2_legacy_arm_trajectories")
        )
        self.declare_parameter("record_frequency", 20.0)

        self.recording_state = "idle"
        self.playback_state = "idle"
        self.current_record_id = None
        self.current_trajectory = []
        self.trajectory_data = []
        self.record_start_time = None
        self.playback_start_time = None
        self.record_frequency = self._bounded_frequency(
            self.get_parameter("record_frequency").value
        )
        self.play_speed = 1.0
        self.loop_playback = False
        self.last_record_sample_time = None
        self.latest_positions = None
        self.latest_velocities = [0.0] * JOINT_COUNT
        self.last_playback_positions = None
        self.last_playback_command_time = None
        self.record_timer = None
        self.playback_timer = None
        self.lock = threading.Lock()

        self.trajectory_dir = Path(self.get_parameter("trajectory_dir").value).expanduser()
        self.trajectory_dir.mkdir(parents=True, exist_ok=True)

        self.teach_service = self.create_service(
            TeachControl, "teach_control", self.handle_teach_control
        )
        self.trajectory_pub = self.create_publisher(
            JointTrajectory, "joint_trajectory", 10
        )
        self.record_status_pub = self.create_publisher(
            TeachRecord, "teach_record_status", 10
        )
        self.create_subscription(
            WaveStatus, "sensor_feedback", self.sensor_feedback_callback, 10
        )
        self.motor_cmd_pub = self.create_publisher(MotorCommand, "motor_commands", 10)

        self.get_logger().info(
            f"Legacy eight-joint teaching manager ready. Trajectories: {self.trajectory_dir}"
        )

    @staticmethod
    def _bounded_frequency(value):
        return max(1.0, min(100.0, float(value)))

    @staticmethod
    def _finite_vector(values):
        return len(values) == JOINT_COUNT and all(math.isfinite(value) for value in values)

    def sensor_feedback_callback(self, msg):
        if list(msg.joint_names) != ARM_JOINTS or not self._finite_vector(msg.positions):
            self.get_logger().warn(
                "Ignored incomplete simulated joint feedback; expected the canonical 8 joints."
            )
            return

        velocities = list(msg.velocities)
        if not self._finite_vector(velocities):
            velocities = [0.0] * JOINT_COUNT

        now = time.monotonic()
        with self.lock:
            self.latest_positions = list(msg.positions)
            self.latest_velocities = velocities
            if self.recording_state != "recording":
                return
            if (
                self.last_record_sample_time is not None
                and now - self.last_record_sample_time < 1.0 / self.record_frequency
            ):
                return
            self._append_record_frame(now, self.latest_positions, self.latest_velocities)

    def _append_record_frame(self, now, positions, velocities):
        if self.record_start_time is None:
            self.record_start_time = now
        self.trajectory_data.append(
            {
                "time": now - self.record_start_time,
                "positions": list(positions),
                "velocities": list(velocities),
            }
        )
        self.last_record_sample_time = now
        self.publish_record_status()

    def handle_teach_control(self, request, response):
        try:
            handlers = {
                "start_record": self.start_recording,
                "stop_record": self.stop_recording,
                "pause_record": self.pause_recording,
                "resume_record": self.resume_recording,
                "save_trajectory": self.save_trajectory,
                "load_trajectory": self.load_trajectory,
                "play_trajectory": self.play_trajectory,
                "stop_playback": self.stop_playback,
            }
            handler = handlers.get(request.command)
            if handler is None:
                response.success = False
                response.message = f"Unknown command: {request.command}"
            else:
                handler(request, response)
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.get_logger().error(f"Teach control failed: {error}")
            response.success = False
            response.message = str(error)

        response.current_state = (
            f"record:{self.recording_state}, playback:{self.playback_state}"
        )
        response.frame_count = len(self.trajectory_data)
        response.progress = self.get_progress()
        return response

    def start_recording(self, request, response):
        if self.recording_state == "recording":
            response.success = False
            response.message = "Already recording"
            return
        if self.latest_positions is None:
            response.success = False
            response.message = "No valid simulated joint feedback received yet"
            return

        self.stop_playback_internal()
        with self.lock:
            self.record_frequency = self._bounded_frequency(
                request.record_frequency or self.record_frequency
            )
            self.trajectory_data = []
            self.current_trajectory = []
            self.current_record_id = f"record_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            self.record_start_time = time.monotonic()
            self.last_record_sample_time = None
            self.recording_state = "recording"
            self._append_record_frame(
                self.record_start_time, self.latest_positions, self.latest_velocities
            )

        response.success = True
        response.message = f"Recording eight simulated joints at {self.record_frequency:.1f} Hz"

    def stop_recording(self, _request, response):
        if self.recording_state not in ("recording", "paused"):
            response.success = False
            response.message = "Not currently recording"
            return
        self.recording_state = "completed"
        self.publish_record_status()
        response.success = True
        response.message = f"Stopped recording {len(self.trajectory_data)} frames"

    def pause_recording(self, _request, response):
        if self.recording_state != "recording":
            response.success = False
            response.message = "Not currently recording"
            return
        self.recording_state = "paused"
        self.publish_record_status()
        response.success = True
        response.message = "Recording paused"

    def resume_recording(self, _request, response):
        if self.recording_state != "paused":
            response.success = False
            response.message = "Recording is not paused"
            return
        self.recording_state = "recording"
        self.last_record_sample_time = None
        self.publish_record_status()
        response.success = True
        response.message = "Recording resumed"

    def save_trajectory(self, request, response):
        if not self.trajectory_data:
            response.success = False
            response.message = "No trajectory data to save"
            return

        trajectory_id = request.trajectory_id or self.current_record_id
        if not trajectory_id:
            response.success = False
            response.message = "Trajectory ID is required"
            return
        trajectory = {
            "id": trajectory_id,
            "description": request.description or "Legacy simulated arm recording",
            "joint_names": ARM_JOINTS,
            "num_joints": JOINT_COUNT,
            "record_frequency": self.record_frequency,
            "record_time": datetime.now().isoformat(),
            "frames": self.trajectory_data,
        }
        path = self._trajectory_path(trajectory_id)
        with path.open("w", encoding="utf-8") as output:
            json.dump(trajectory, output, indent=2)
        self.current_trajectory = self.trajectory_data.copy()
        self._publish_joint_trajectory(self.current_trajectory)
        response.success = True
        response.message = f"Trajectory saved to {path}"

    def load_trajectory(self, request, response):
        if not request.trajectory_id:
            response.success = False
            response.message = "Trajectory ID is required"
            return
        path = self._trajectory_path(request.trajectory_id)
        with path.open(encoding="utf-8") as source:
            trajectory = json.load(source)
        frames = self._validate_trajectory(trajectory)
        self.current_trajectory = frames
        self.trajectory_data = frames.copy()
        self._publish_joint_trajectory(frames)
        response.success = True
        response.message = f"Loaded {len(frames)} eight-joint frames from {path}"

    def play_trajectory(self, request, response):
        if self.playback_state == "playing":
            response.success = False
            response.message = "Already playing trajectory"
            return
        if request.trajectory_id:
            path = self._trajectory_path(request.trajectory_id)
            with path.open(encoding="utf-8") as source:
                frames = self._validate_trajectory(json.load(source))
        else:
            frames = self.current_trajectory or self.trajectory_data
            frames = self._validate_trajectory(
                {"joint_names": ARM_JOINTS, "frames": frames}
            )

        self.current_trajectory = frames
        self.play_speed = max(0.1, min(10.0, float(request.play_speed or 1.0)))
        self.loop_playback = bool(request.loop_playback)
        self.playback_state = "playing"
        self.playback_start_time = time.monotonic()
        self.last_playback_positions = None
        self.last_playback_command_time = None
        self.playback_timer = self.create_timer(
            1.0 / self.PLAYBACK_RATE_HZ, self.execute_playback
        )
        response.success = True
        response.message = (
            f"Playing {len(frames)} eight-joint frames at {self.play_speed:.1f}x"
        )

    def stop_playback(self, _request, response):
        was_playing = self.playback_state == "playing"
        self.stop_playback_internal()
        response.success = was_playing
        response.message = (
            "Playback stopped and current simulated position held"
            if was_playing
            else "Not playing"
        )

    def stop_playback_internal(self):
        if self.playback_timer is not None:
            self.playback_timer.cancel()
            self.playback_timer = None
        if self.playback_state == "playing":
            self._hold_current_position()
        self.playback_state = "idle"

    def execute_playback(self):
        if self.playback_state != "playing":
            return
        duration = self.current_trajectory[-1]["time"]
        elapsed = (time.monotonic() - self.playback_start_time) * self.play_speed
        if elapsed >= duration:
            if self.loop_playback and duration > 0.0:
                self.playback_start_time = time.monotonic()
                elapsed = 0.0
            else:
                self.stop_playback_internal()
                self.get_logger().info("Simulated trajectory playback completed and held.")
                return
        self._send_motor_commands(self._interpolate(elapsed))

    def _interpolate(self, target_time):
        frames = self.current_trajectory
        if target_time <= frames[0]["time"]:
            return frames[0]["positions"]
        for previous, following in zip(frames, frames[1:]):
            if target_time <= following["time"]:
                span = following["time"] - previous["time"]
                alpha = (target_time - previous["time"]) / span
                return [
                    before + alpha * (after - before)
                    for before, after in zip(previous["positions"], following["positions"])
                ]
        return frames[-1]["positions"]

    def _send_motor_commands(self, positions):
        now = time.monotonic()
        if self.last_playback_positions is None or self.last_playback_command_time is None:
            velocities = [1.0] * JOINT_COUNT
        else:
            dt = max(now - self.last_playback_command_time, 1e-3)
            velocities = [
                max(abs(current - previous) / dt, 0.05)
                for current, previous in zip(positions, self.last_playback_positions)
            ]
        command = MotorCommand()
        command.motor_ids = MOTOR_IDS["both"]
        command.positions = list(positions)
        command.velocities = velocities
        self.motor_cmd_pub.publish(command)
        self.last_playback_positions = list(positions)
        self.last_playback_command_time = now

    def _hold_current_position(self):
        if self.latest_positions is not None:
            command = MotorCommand()
            command.motor_ids = MOTOR_IDS["both"]
            command.positions = list(self.latest_positions)
            command.velocities = [0.0] * JOINT_COUNT
            self.motor_cmd_pub.publish(command)

    def _validate_trajectory(self, trajectory):
        if trajectory.get("joint_names") != ARM_JOINTS:
            raise ValueError(
                "Trajectory joint_names must match the canonical eight-joint model"
            )
        frames = trajectory.get("frames")
        if not isinstance(frames, list) or len(frames) < 2:
            raise ValueError("Trajectory must contain at least two frames")

        normalized = []
        previous_time = None
        for frame in frames:
            frame_time = frame.get("time", frame.get("timestamp"))
            positions = frame.get("positions")
            velocities = frame.get("velocities", [0.0] * JOINT_COUNT)
            if not isinstance(frame_time, (int, float)) or not math.isfinite(frame_time):
                raise ValueError("Trajectory frame time must be finite")
            if not self._finite_vector(positions) or not self._finite_vector(velocities):
                raise ValueError(
                    "Every trajectory frame needs eight finite positions and velocities"
                )
            if previous_time is not None and frame_time <= previous_time:
                raise ValueError("Trajectory frame times must be strictly increasing")
            normalized.append(
                {
                    "time": float(frame_time),
                    "positions": [float(value) for value in positions],
                    "velocities": [float(value) for value in velocities],
                }
            )
            previous_time = frame_time

        offset = normalized[0]["time"]
        for frame in normalized:
            frame["time"] -= offset
        return normalized

    def _trajectory_path(self, trajectory_id):
        clean_id = Path(trajectory_id).name
        if clean_id != trajectory_id or not clean_id:
            raise ValueError("Trajectory ID must be a file name without path components")
        return self.trajectory_dir / f"{clean_id}.json"

    def _publish_joint_trajectory(self, frames):
        message = JointTrajectory()
        message.joint_names = ARM_JOINTS
        message.positions = [position for frame in frames for position in frame["positions"]]
        message.num_joints = JOINT_COUNT
        message.num_points = len(frames)
        message.timestamps = [frame["time"] for frame in frames]
        message.frame_id = "legacy_wave_sim"
        message.description = "Eight-joint legacy simulation trajectory"
        self.trajectory_pub.publish(message)

    def publish_record_status(self):
        message = TeachRecord()
        message.record_id = self.current_record_id or ""
        message.state = self.recording_state
        message.frame_count = len(self.trajectory_data)
        message.duration = (
            time.monotonic() - self.record_start_time
            if self.record_start_time is not None
            else 0.0
        )
        message.frequency = self.record_frequency
        message.description = "Eight-joint simulated trajectory"
        self.record_status_pub.publish(message)

    def get_progress(self):
        if self.playback_state != "playing" or not self.current_trajectory:
            return 0.0
        duration = self.current_trajectory[-1]["time"]
        if duration <= 0.0:
            return 1.0
        elapsed = (time.monotonic() - self.playback_start_time) * self.play_speed
        return min(1.0, elapsed / duration)


def main(args=None):
    rclpy.init(args=args)
    node = TeachManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
