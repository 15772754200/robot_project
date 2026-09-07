#!/usr/bin/env python3
"""Interactive recorder client for the current-stack arm manager."""

import time

import rclpy
from rclpy.node import Node

from wave_control_msgs.msg import TeachRecord
from wave_control_msgs.srv import TeachControl


class TeachRealTester(Node):
    def __init__(self) -> None:
        super().__init__("teach_real_tester")
        self.teach_client = self.create_client(TeachControl, "teach_control_real")
        self.record_status_sub = self.create_subscription(
            TeachRecord,
            "teach_record_status",
            self._record_status_callback,
            10,
        )
        self.last_record_status = None

    def wait_for_service(self) -> None:
        while not self.teach_client.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for teach_control_real service...")

    def _record_status_callback(self, msg: TeachRecord) -> None:
        self.last_record_status = msg
        self.get_logger().info(
            f"Record state={msg.state}, frames={msg.frame_count}, duration={msg.duration:.2f}s"
        )

    def send_teach_command(self, command: str, **kwargs):
        request = TeachControl.Request()
        request.command = command
        request.trajectory_id = kwargs.get("trajectory_id", "")
        request.description = kwargs.get("description", "")
        request.record_frequency = float(kwargs.get("record_frequency", 0.0))
        request.play_speed = float(kwargs.get("play_speed", 0.0))
        request.loop_playback = bool(kwargs.get("loop_playback", False))

        future = self.teach_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=10.0)
        if not future.done():
            self.get_logger().error(f"Timed out waiting for {command}")
            return None
        try:
            return future.result()
        except Exception as exc:
            self.get_logger().error(f"{command} failed: {exc}")
            return None

    def run_test(self) -> None:
        self.wait_for_service()
        input(
            "Confirm the robot is stable in MODE_STAND and ready for manual arm teaching. "
            "Press Enter to start recording."
        )
        response = self.send_teach_command(
            "start_record",
            description="Manual 8-joint arm recording",
            record_frequency=50.0,
        )
        if response is None or not response.success:
            self.get_logger().error(
                f"Could not start recording: {response.message if response else 'no response'}"
            )
            return

        input("Move only the arms as intended, then press Enter to stop recording.")
        response = self.send_teach_command("stop_record")
        if response is None or not response.success:
            self.get_logger().error(
                f"Could not stop recording: {response.message if response else 'no response'}"
            )
            return

        trajectory_id = input("Trajectory name: ").strip()
        if not trajectory_id:
            trajectory_id = f"arm_record_{time.strftime('%Y%m%d_%H%M%S')}"
        response = self.send_teach_command(
            "save_trajectory",
            trajectory_id=trajectory_id,
            description="Manual 8-joint arm recording",
        )
        if response is None or not response.success:
            self.get_logger().error(
                f"Could not save trajectory: {response.message if response else 'no response'}"
            )
            return
        self.get_logger().info(response.message)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = TeachRealTester()
    try:
        node.run_test()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
