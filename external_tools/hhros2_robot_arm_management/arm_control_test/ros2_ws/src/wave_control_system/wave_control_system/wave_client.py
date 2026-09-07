#!/usr/bin/env python3
"""Small automatic client for the isolated legacy wave simulation."""

import time

import rclpy
from rclpy.node import Node

from wave_control_msgs.msg import WaveCommand
from wave_control_msgs.srv import WaveControl


class WaveClient(Node):
    def __init__(self):
        super().__init__("wave_client")
        self.client = self.create_client(WaveControl, "wave_control")
        while not self.client.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for legacy wave simulation service...")

    def send_wave_command(
        self, operation_type, wave_part="right_arm", amplitude=45.0, frequency=1.0
    ):
        request = WaveControl.Request()
        request.command = WaveCommand()
        request.command.robot_id = "LEGACY_SIM"
        request.command.operation_type = operation_type
        request.command.wave_part = wave_part
        request.command.amplitude = amplitude
        request.command.frequency = frequency

        future = self.client.call_async(request)
        rclpy.spin_until_future_complete(self, future)
        response = future.result()
        if response.success:
            self.get_logger().info(f"Command successful: {response.message}")
        else:
            self.get_logger().error(f"Command failed: {response.message}")
        return response


def main(args=None):
    rclpy.init(args=args)
    client = WaveClient()
    client.get_logger().warn("Running legacy simulation only; no real-robot topic is used.")
    client.send_wave_command("wave_hello", "right_arm", 45.0, 1.0)
    time.sleep(5)
    client.send_wave_command("change_state", "left_arm", 60.0, 2.0)
    time.sleep(5)
    client.send_wave_command("stop")
    client.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
