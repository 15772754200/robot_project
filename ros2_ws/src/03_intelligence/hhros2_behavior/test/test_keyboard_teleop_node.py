from types import SimpleNamespace

import pytest
import rclpy

from hhros2_behavior.keyboard_teleop_node import KeyboardTeleopNode
from hhros2_interfaces.msg import ArbitrationMode


@pytest.fixture
def keyboard_node():
    if not rclpy.ok():
        rclpy.init()
    node = KeyboardTeleopNode()
    published_commands = []
    requested_modes = []
    node._cmd_vel_publisher = SimpleNamespace(publish=published_commands.append)
    node._request_mode = requested_modes.append
    yield node, published_commands, requested_modes
    node.destroy_node()
    if rclpy.ok():
        rclpy.shutdown()


def test_mode_key_uses_governance_service_and_stops_velocity(keyboard_node):
    node, published_commands, requested_modes = keyboard_node
    node.handle_key("w")

    node.handle_key("2")

    assert requested_modes == [ArbitrationMode.MODE_STAND]
    assert published_commands[-1].linear.x == pytest.approx(0.0)
    assert published_commands[-1].linear.y == pytest.approx(0.0)
    assert published_commands[-1].angular.z == pytest.approx(0.0)


def test_velocity_keys_accumulate_and_clamp(keyboard_node):
    node, published_commands, _ = keyboard_node

    for _ in range(20):
        node.handle_key("w")
        node.handle_key("a")
        node.handle_key("q")

    command = published_commands[-1]
    assert command.linear.x == pytest.approx(0.6)
    assert command.linear.y == pytest.approx(0.3)
    assert command.angular.z == pytest.approx(1.0)


def test_stop_key_publishes_zero_velocity(keyboard_node):
    node, published_commands, _ = keyboard_node
    node.handle_key("s")
    node.handle_key("d")
    node.handle_key("e")

    node.handle_key("r")

    command = published_commands[-1]
    assert command.linear.x == pytest.approx(0.0)
    assert command.linear.y == pytest.approx(0.0)
    assert command.angular.z == pytest.approx(0.0)
