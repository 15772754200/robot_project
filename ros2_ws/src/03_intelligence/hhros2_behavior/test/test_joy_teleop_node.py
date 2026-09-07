from time import monotonic
from types import SimpleNamespace

import pytest
import rclpy
from sensor_msgs.msg import Joy

from hhros2_behavior.joy_teleop_node import JoyTeleopNode
from hhros2_interfaces.msg import ArbitrationMode


@pytest.fixture
def joy_node():
    if not rclpy.ok():
        rclpy.init()
    node = JoyTeleopNode()
    published_commands = []
    requested_modes = []
    node._cmd_pub = SimpleNamespace(publish=published_commands.append)

    def request_mode(mode):
        requested_modes.append(mode)
        node._pending_mode = mode
        return True

    node._request_mode = request_mode
    yield node, published_commands, requested_modes
    node.destroy_node()
    if rclpy.ok():
        rclpy.shutdown()


def test_axes_and_button_edge_drive_native_contract(joy_node):
    node, _, requested_modes = joy_node
    message = Joy()
    message.axes = [0.5, 0.25, 0.0, -0.5]
    message.buttons = [0, 0, 0, 1, 0, 0, 0, 0]

    node._on_joy(message)
    response = SimpleNamespace(
        success=True,
        active_mode=ArbitrationMode.MODE_WALK,
        message="active",
    )
    node._mode_result(
        ArbitrationMode.MODE_WALK,
        SimpleNamespace(result=lambda: response),
    )
    node._on_joy(message)

    assert node._latest_twist.linear.x == pytest.approx(0.15)
    assert node._latest_twist.linear.y == pytest.approx(0.15)
    assert node._latest_twist.angular.z == pytest.approx(-0.5)
    assert requested_modes == [ArbitrationMode.MODE_WALK]


def test_axis_input_is_finite_and_bounded(joy_node):
    node, _, _ = joy_node
    message = Joy()
    message.axes = [float("nan"), 4.0, 0.0, -5.0]
    message.buttons = []
    node._motion_intent_enabled = True

    node._on_joy(message)

    assert node._latest_twist.linear.x == pytest.approx(0.6)
    assert node._latest_twist.linear.y == pytest.approx(0.0)
    assert node._latest_twist.angular.z == pytest.approx(-1.0)


def test_nonmoving_mode_clears_velocity(joy_node):
    node, published_commands, requested_modes = joy_node
    message = Joy()
    message.axes = [0.5, 0.25, 0.0, -0.5]
    message.buttons = [1, 0, 0, 0, 0, 0, 0, 0]

    node._on_joy(message)

    assert requested_modes == [ArbitrationMode.MODE_STAND]
    assert published_commands[-1].linear.x == pytest.approx(0.0)
    assert published_commands[-1].linear.y == pytest.approx(0.0)
    assert published_commands[-1].angular.z == pytest.approx(0.0)


def test_stale_joystick_input_publishes_zero(joy_node):
    node, published_commands, _ = joy_node
    message = Joy()
    message.axes = [0.5, 0.25, 0.0, -0.5]
    message.buttons = []
    node._motion_intent_enabled = True
    node._on_joy(message)
    node._last_joy_at = monotonic() - node._input_timeout_sec - 0.1

    node._publish_velocity()

    assert published_commands[-1].linear.x == pytest.approx(0.0)
    assert published_commands[-1].linear.y == pytest.approx(0.0)
    assert published_commands[-1].angular.z == pytest.approx(0.0)


def test_rejected_mode_request_clears_velocity(joy_node):
    node, published_commands, _ = joy_node
    node._motion_intent_enabled = True
    node._latest_twist.linear.x = 0.4
    node._pending_mode = ArbitrationMode.MODE_WALK
    response = SimpleNamespace(
        success=False,
        active_mode=ArbitrationMode.MODE_STAND,
        message="rejected",
    )
    future = SimpleNamespace(result=lambda: response)

    node._mode_result(ArbitrationMode.MODE_WALK, future)

    assert published_commands[-1].linear.x == pytest.approx(0.0)
    assert published_commands[-1].linear.y == pytest.approx(0.0)
    assert published_commands[-1].angular.z == pytest.approx(0.0)


def test_mode_request_timeout_clears_pending_motion(joy_node):
    node, published_commands, _ = joy_node
    node._motion_intent_enabled = True
    node._latest_twist.linear.x = 0.4
    node._pending_mode = ArbitrationMode.MODE_WALK
    node._pending_mode_requested_at = (
        monotonic() - node._mode_request_timeout_sec - 0.1)

    node._publish_velocity()

    assert node._pending_mode is None
    assert published_commands[-1].linear.x == pytest.approx(0.0)
    assert published_commands[-1].linear.y == pytest.approx(0.0)
    assert published_commands[-1].angular.z == pytest.approx(0.0)
