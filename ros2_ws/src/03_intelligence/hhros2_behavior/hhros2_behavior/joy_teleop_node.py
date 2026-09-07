"""Joystick adapter for governed locomotion intent."""

from __future__ import annotations

from math import isfinite
from time import monotonic

import rclpy
from geometry_msgs.msg import Twist
from hhros2_interfaces.msg import ArbitrationMode
from hhros2_interfaces.srv import SetControlMode
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import QoSPolicyKind
from rclpy.qos_overriding_options import QoSOverridingOptions
from sensor_msgs.msg import Joy


MOVING_MODES = {
    ArbitrationMode.MODE_WALK,
    ArbitrationMode.MODE_RUN,
}
QOS_OVERRIDES = QoSOverridingOptions(policy_kinds=(
    QoSPolicyKind.HISTORY,
    QoSPolicyKind.DEPTH,
    QoSPolicyKind.RELIABILITY,
    QoSPolicyKind.DURABILITY,
))


class JoyTeleopNode(Node):
    """Translate joystick input into mode requests and base velocity intent."""

    def __init__(self) -> None:
        super().__init__("hhros2_joy_teleop")
        self.declare_parameter("joy_topic", "/joy")
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter(
            "control_mode_service", "/hhros2_core/set_control_mode")
        self.declare_parameter("axis_linear_x", 1)
        self.declare_parameter("axis_linear_y", 0)
        self.declare_parameter("axis_angular_z", 3)
        self.declare_parameter("max_linear_x", 0.6)
        self.declare_parameter("max_linear_y", 0.3)
        self.declare_parameter("max_angular_z", 1.0)
        self.declare_parameter("deadzone", 0.08)
        self.declare_parameter("stand_button", 0)
        self.declare_parameter("damping_button", 1)
        self.declare_parameter("run_button", 2)
        self.declare_parameter("walk_button", 3)
        self.declare_parameter("passive_button", 6)
        self.declare_parameter("wbc_button", 7)
        self.declare_parameter("publish_rate_hz", 20.0)
        self.declare_parameter("input_timeout_sec", 0.5)
        self.declare_parameter("mode_request_timeout_sec", 1.0)

        joy_topic = str(self.get_parameter("joy_topic").value)
        cmd_vel_topic = str(self.get_parameter("cmd_vel_topic").value)
        self._mode_service = str(
            self.get_parameter("control_mode_service").value)
        self._axis_x = self._nonnegative_integer_parameter("axis_linear_x")
        self._axis_y = self._nonnegative_integer_parameter("axis_linear_y")
        self._axis_wz = self._nonnegative_integer_parameter("axis_angular_z")
        self._max_x = self._positive_parameter("max_linear_x")
        self._max_y = self._positive_parameter("max_linear_y")
        self._max_wz = self._positive_parameter("max_angular_z")
        self._deadzone = self._deadzone_parameter()
        self._input_timeout_sec = self._positive_parameter(
            "input_timeout_sec")
        self._mode_request_timeout_sec = self._positive_parameter(
            "mode_request_timeout_sec")

        button_modes = [
            (self._nonnegative_integer_parameter("stand_button"),
             ArbitrationMode.MODE_STAND),
            (self._nonnegative_integer_parameter("damping_button"),
             ArbitrationMode.MODE_DAMPING),
            (self._nonnegative_integer_parameter("run_button"),
             ArbitrationMode.MODE_RUN),
            (self._nonnegative_integer_parameter("walk_button"),
             ArbitrationMode.MODE_WALK),
            (self._nonnegative_integer_parameter("passive_button"),
             ArbitrationMode.MODE_PASSIVE),
            (self._nonnegative_integer_parameter("wbc_button"),
             ArbitrationMode.MODE_WBC),
        ]
        if len({button for button, _ in button_modes}) != len(button_modes):
            raise ValueError("Joystick mode buttons must be unique")
        self._button_modes = dict(button_modes)

        self._mode_client = self.create_client(
            SetControlMode, self._mode_service)
        self._cmd_pub = self.create_publisher(
            Twist, cmd_vel_topic, 10,
            qos_overriding_options=QOS_OVERRIDES)
        self._joy_sub = self.create_subscription(
            Joy, joy_topic, self._on_joy, 10,
            qos_overriding_options=QOS_OVERRIDES)
        self._last_buttons: list[int] = []
        self._latest_twist = Twist()
        self._last_joy_at: float | None = None
        self._input_stale = True
        self._motion_intent_enabled = False
        self._pending_mode: int | None = None
        self._pending_mode_requested_at: float | None = None
        publish_rate = self._positive_parameter("publish_rate_hz")
        self.create_timer(1.0 / publish_rate, self._publish_velocity)

        self.get_logger().info(
            "joy teleop ready: A=stand B=damping X=run Y=walk, "
            "Back=passive Start=WBC")

    def _positive_parameter(self, name: str) -> float:
        value = float(self.get_parameter(name).value)
        if not isfinite(value) or value <= 0.0:
            raise ValueError(
                "Parameter %s must be finite and positive" % name)
        return value

    def _nonnegative_integer_parameter(self, name: str) -> int:
        value = int(self.get_parameter(name).value)
        if value < 0:
            raise ValueError("Parameter %s must be nonnegative" % name)
        return value

    def _deadzone_parameter(self) -> float:
        value = float(self.get_parameter("deadzone").value)
        if not isfinite(value) or value < 0.0 or value >= 1.0:
            raise ValueError("Parameter deadzone must be in [0, 1)")
        return value

    def _axis(self, msg: Joy, index: int, scale: float) -> float:
        if index >= len(msg.axes):
            return 0.0
        value = float(msg.axes[index])
        if not isfinite(value) or abs(value) < self._deadzone:
            return 0.0
        return max(-1.0, min(1.0, value)) * scale

    def _on_joy(self, msg: Joy) -> None:
        self._last_joy_at = monotonic()
        self._input_stale = False

        buttons = [int(value) for value in msg.buttons]
        requested_modes = []
        for index, mode in self._button_modes.items():
            pressed = index < len(buttons) and buttons[index] != 0
            previously_pressed = (
                index < len(self._last_buttons) and
                self._last_buttons[index] != 0)
            if pressed and not previously_pressed:
                requested_modes.append(mode)
        self._last_buttons = buttons

        if len(requested_modes) > 1:
            self.get_logger().warning(
                "ignoring simultaneous control-mode button presses")
            self.stop()
            return
        if requested_modes:
            mode = requested_modes[0]
            self._request_mode(mode)
            self._latest_twist = Twist()
            self._motion_intent_enabled = False
            self._cmd_pub.publish(self._latest_twist)

        twist = Twist()
        if self._motion_intent_enabled:
            twist.linear.x = self._axis(msg, self._axis_x, self._max_x)
            twist.linear.y = self._axis(msg, self._axis_y, self._max_y)
            twist.angular.z = self._axis(msg, self._axis_wz, self._max_wz)
        self._latest_twist = twist

    def _request_mode(self, mode: int) -> bool:
        if not self._mode_client.service_is_ready():
            self.get_logger().warning(
                "%s service is not ready" % self._mode_service)
            self._pending_mode = None
            self._pending_mode_requested_at = None
            return False
        request = SetControlMode.Request()
        request.mode = mode
        future = self._mode_client.call_async(request)
        self._pending_mode = mode
        self._pending_mode_requested_at = monotonic()
        future.add_done_callback(
            lambda result: self._mode_result(mode, result))
        return True

    def _mode_result(self, mode: int, future) -> None:
        if self._pending_mode != mode:
            self.get_logger().warning(
                "ignoring stale mode %d response" % mode)
            return
        self._pending_mode = None
        self._pending_mode_requested_at = None
        try:
            response = future.result()
        except Exception as exc:  # noqa: BLE001
            self.get_logger().error(
                "mode %d request failed: %s" % (mode, exc))
            self.stop()
            return
        activated = response.success and response.active_mode == mode
        log = self.get_logger().info if activated \
            else self.get_logger().warning
        log("mode %d -> active %d: %s" %
            (mode, response.active_mode, response.message))
        if activated and mode in MOVING_MODES:
            self._motion_intent_enabled = True
            self._last_joy_at = monotonic()
            self._input_stale = False
        else:
            self.stop()

    def _publish_velocity(self) -> None:
        now = monotonic()
        if self._pending_mode_requested_at is not None and \
                now - self._pending_mode_requested_at > \
                self._mode_request_timeout_sec:
            self.get_logger().warning(
                "control-mode request timed out; publishing zero velocity")
            self.stop()
            return
        if self._last_joy_at is None or \
                now - self._last_joy_at > self._input_timeout_sec:
            if not self._input_stale:
                self.get_logger().warning(
                    "joystick input timed out; publishing zero velocity")
            self._latest_twist = Twist()
            self._input_stale = True
            self._motion_intent_enabled = False
            self._pending_mode = None
            self._pending_mode_requested_at = None
        self._cmd_pub.publish(self._latest_twist)

    def stop(self) -> None:
        """Clear joystick intent and publish an explicit zero velocity."""
        self._latest_twist = Twist()
        self._last_joy_at = None
        self._input_stale = True
        self._motion_intent_enabled = False
        self._pending_mode = None
        self._pending_mode_requested_at = None
        self._cmd_pub.publish(self._latest_twist)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = JoyTeleopNode()
    try:
        rclpy.spin(node)
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
