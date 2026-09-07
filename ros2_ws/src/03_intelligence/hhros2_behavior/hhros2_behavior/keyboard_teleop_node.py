"""Terminal keyboard adapter for the project-native motion command interface."""

from __future__ import annotations

import os
import select
import sys
import termios
import tty
from typing import Optional

import rclpy
from geometry_msgs.msg import Twist
from hhros2_interfaces.msg import ArbitrationMode
from hhros2_interfaces.srv import SetControlMode
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import QoSPolicyKind
from rclpy.qos_overriding_options import QoSOverridingOptions


MODE_KEYS = {
    "0": (ArbitrationMode.MODE_PASSIVE, "passive"),
    "1": (ArbitrationMode.MODE_DAMPING, "damping"),
    "2": (ArbitrationMode.MODE_STAND, "stand"),
    "3": (ArbitrationMode.MODE_WALK, "walk"),
    "4": (ArbitrationMode.MODE_RUN, "run"),
    "5": (ArbitrationMode.MODE_WBC, "wbc"),
    "6": (ArbitrationMode.MODE_MOTION, "motion"),
}

MOVING_MODES = {
    ArbitrationMode.MODE_WALK,
    ArbitrationMode.MODE_RUN,
}
MODE_NAMES = {mode: name for mode, name in MODE_KEYS.values()}
QOS_OVERRIDES = QoSOverridingOptions(policy_kinds=(
    QoSPolicyKind.HISTORY,
    QoSPolicyKind.DEPTH,
    QoSPolicyKind.RELIABILITY,
    QoSPolicyKind.DURABILITY,
))


class KeyboardTeleopNode(Node):
    """Translate terminal keys into mode requests and base velocity commands."""

    def __init__(self) -> None:
        super().__init__("hhros2_keyboard_teleop")
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter(
            "control_mode_service", "/hhros2_core/set_control_mode")
        self.declare_parameter("publish_rate_hz", 20.0)
        self.declare_parameter("linear_step", 0.05)
        self.declare_parameter("lateral_step", 0.05)
        self.declare_parameter("angular_step", 0.1)
        self.declare_parameter("max_linear", 0.6)
        self.declare_parameter("max_lateral", 0.3)
        self.declare_parameter("max_angular", 1.0)

        self._cmd_vel_topic = str(self.get_parameter("cmd_vel_topic").value)
        self._mode_service = str(
            self.get_parameter("control_mode_service").value)
        self._publish_rate_hz = self._positive_parameter("publish_rate_hz")
        self._linear_step = self._positive_parameter("linear_step")
        self._lateral_step = self._positive_parameter("lateral_step")
        self._angular_step = self._positive_parameter("angular_step")
        self._max_linear = self._positive_parameter("max_linear")
        self._max_lateral = self._positive_parameter("max_lateral")
        self._max_angular = self._positive_parameter("max_angular")

        self._cmd_vel_publisher = self.create_publisher(
            Twist, self._cmd_vel_topic, 10,
            qos_overriding_options=QOS_OVERRIDES)
        self._mode_client = self.create_client(SetControlMode, self._mode_service)
        self._vx = 0.0
        self._vy = 0.0
        self._wz = 0.0
        self.create_timer(1.0 / self._publish_rate_hz, self._publish_velocity)

        self.get_logger().info(
            "keyboard teleop ready: 0 passive, 1 damping, 2 stand, 3 walk, "
            "4 run, 5 wbc, 6 motion; "
            "W/S x, A/D y, Q/E yaw, R stop, Ctrl-C quit")

    def _positive_parameter(self, name: str) -> float:
        value = float(self.get_parameter(name).value)
        if value <= 0.0:
            raise ValueError("Parameter %s must be positive" % name)
        return value

    def handle_key(self, key: str) -> None:
        """Apply one key received from the terminal."""
        if key == "\x03":
            raise KeyboardInterrupt

        if key in MODE_KEYS:
            mode, label = MODE_KEYS[key]
            self._request_mode(mode)
            if mode not in MOVING_MODES:
                self.stop()
            self.get_logger().info("requested mode=%s (%d)" % (label, mode))
            return

        lower = key.lower()
        if lower == "w":
            self._vx = min(self._max_linear, self._vx + self._linear_step)
        elif lower == "s":
            self._vx = max(-self._max_linear, self._vx - self._linear_step)
        elif lower == "a":
            self._vy = min(self._max_lateral, self._vy + self._lateral_step)
        elif lower == "d":
            self._vy = max(-self._max_lateral, self._vy - self._lateral_step)
        elif lower == "q":
            self._wz = min(self._max_angular, self._wz + self._angular_step)
        elif lower == "e":
            self._wz = max(-self._max_angular, self._wz - self._angular_step)
        elif lower == "r":
            self.stop()
            return
        else:
            return

        self.get_logger().info(
            "cmd_vel x=%.3f y=%.3f yaw=%.3f" %
            (self._vx, self._vy, self._wz))
        self._publish_velocity()

    def stop(self) -> None:
        """Publish an explicit zero velocity and clear accumulated input."""
        self._vx = 0.0
        self._vy = 0.0
        self._wz = 0.0
        self._publish_velocity()

    def _request_mode(self, mode: int) -> None:
        if not self._mode_client.service_is_ready():
            self.get_logger().warning(
                "%s service is not ready" % self._mode_service)
            return
        request = SetControlMode.Request()
        request.mode = mode
        future = self._mode_client.call_async(request)
        future.add_done_callback(
            lambda result: self._handle_mode_response(mode, result))

    def _handle_mode_response(self, requested_mode: int, future) -> None:
        try:
            response = future.result()
        except Exception as exc:  # noqa: BLE001
            self.get_logger().error(
                "mode request %s failed: %s" %
                (self._mode_name(requested_mode), exc))
            return
        log = self.get_logger().info if response.success \
            else self.get_logger().warning
        log(
            "mode request %s -> active %s: %s" %
            (self._mode_name(requested_mode),
             self._mode_name(response.active_mode), response.message))

    @staticmethod
    def _mode_name(mode: int) -> str:
        return MODE_NAMES.get(mode, "mode_%d" % mode)

    def _publish_velocity(self) -> None:
        command = Twist()
        command.linear.x = self._vx
        command.linear.y = self._vy
        command.angular.z = self._wz
        self._cmd_vel_publisher.publish(command)


class RawTerminal:
    """Own and restore the controlling terminal while reading raw keys."""

    def __init__(self) -> None:
        self._fd: Optional[int] = None
        self._owns_fd = False
        self._saved_settings = None

    def __enter__(self) -> "RawTerminal":
        try:
            self._fd = os.open("/dev/tty", os.O_RDWR | os.O_NOCTTY)
            self._owns_fd = True
        except OSError:
            if not sys.stdin.isatty():
                raise RuntimeError(
                    "keyboard teleop requires an interactive terminal")
            self._fd = sys.stdin.fileno()

        self._saved_settings = termios.tcgetattr(self._fd)
        tty.setraw(self._fd)
        return self

    def __exit__(self, *_args) -> None:
        if self._fd is not None and self._saved_settings is not None:
            termios.tcsetattr(self._fd, termios.TCSADRAIN, self._saved_settings)
        if self._owns_fd and self._fd is not None:
            os.close(self._fd)
        self._fd = None
        self._owns_fd = False
        self._saved_settings = None

    def read_key(self, timeout_sec: float) -> Optional[str]:
        if self._fd is None:
            return None
        readable, _, _ = select.select([self._fd], [], [], timeout_sec)
        if not readable:
            return None
        data = os.read(self._fd, 1)
        return data.decode(errors="ignore") if data else None


def main(args: list[str] | None = None) -> None:
    rclpy.init(args=args)
    node = KeyboardTeleopNode()
    try:
        with RawTerminal() as terminal:
            while rclpy.ok():
                rclpy.spin_once(node, timeout_sec=0.02)
                key = terminal.read_key(0.0)
                if key:
                    node.handle_key(key)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        if rclpy.ok():
            node.stop()
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
