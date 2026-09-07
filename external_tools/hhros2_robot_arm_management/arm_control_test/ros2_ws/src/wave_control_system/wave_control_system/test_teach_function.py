#!/usr/bin/env python3
"""Automatic end-to-end smoke test for the isolated legacy teaching simulation."""

import time

import rclpy
from rclpy.node import Node

from wave_control_msgs.msg import WaveCommand, WaveStatus
from wave_control_msgs.srv import TeachControl, WaveControl


class TeachFunctionTester(Node):
    def __init__(self):
        super().__init__("teach_function_tester")
        self.teach_client = self.create_client(TeachControl, "teach_control")
        self.wave_client = self.create_client(WaveControl, "wave_control")
        self.feedback_count = 0
        self.create_subscription(WaveStatus, "sensor_feedback", self.feedback_callback, 10)

    def feedback_callback(self, _message):
        self.feedback_count += 1

    def wait_for_services(self):
        for client, name in (
            (self.teach_client, "teach_control"),
            (self.wave_client, "wave_control"),
        ):
            while not client.wait_for_service(timeout_sec=1.0):
                self.get_logger().info(f"Waiting for {name}...")

    def call_teach(self, command, **kwargs):
        request = TeachControl.Request()
        request.command = command
        request.trajectory_id = kwargs.get("trajectory_id", "")
        request.description = kwargs.get("description", "")
        request.record_frequency = kwargs.get("record_frequency", 0.0)
        request.play_speed = kwargs.get("play_speed", 0.0)
        request.loop_playback = kwargs.get("loop_playback", False)
        return self._call(self.teach_client, request)

    def call_wave(self, operation_type, wave_part="left_arm"):
        request = WaveControl.Request()
        request.command = WaveCommand()
        request.command.robot_id = "LEGACY_SIM"
        request.command.operation_type = operation_type
        request.command.wave_part = wave_part
        request.command.amplitude = 25.0
        request.command.frequency = 0.5
        return self._call(self.wave_client, request)

    def _call(self, client, request):
        future = client.call_async(request)
        rclpy.spin_until_future_complete(self, future)
        response = future.result()
        if response is None:
            raise RuntimeError("Service returned no response")
        if not response.success:
            raise RuntimeError(response.message)
        self.get_logger().info(response.message)
        return response

    def spin_for(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

    def run(self):
        self.wait_for_services()
        self.spin_for(0.5)
        if self.feedback_count == 0:
            raise RuntimeError("No simulated joint feedback received")

        self.call_wave("wave_hello", "left_arm")
        self.call_teach(
            "start_record",
            description="Automatic eight-joint simulated teaching smoke test",
            record_frequency=20.0,
        )
        self.spin_for(3.0)
        self.call_teach("stop_record")
        self.call_teach("save_trajectory", trajectory_id="legacy_sim_smoke")
        self.call_wave("stop")
        self.call_teach(
            "play_trajectory", trajectory_id="legacy_sim_smoke", play_speed=1.0
        )
        self.spin_for(4.0)
        self.get_logger().info("Legacy eight-joint teaching smoke test completed.")


def main(args=None):
    rclpy.init(args=args)
    node = TeachFunctionTester()
    try:
        node.run()
    except Exception as error:
        node.get_logger().error(f"Legacy teaching smoke test failed: {error}")
        raise
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
