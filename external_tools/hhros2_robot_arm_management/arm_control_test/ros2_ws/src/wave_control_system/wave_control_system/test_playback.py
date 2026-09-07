#!/usr/bin/env python3
"""Interactive playback client for the current-stack arm manager."""

import rclpy
from rclpy.node import Node

from wave_control_msgs.srv import TeachControl


class TrajectoryPlaybackTester(Node):
    def __init__(self) -> None:
        super().__init__("trajectory_playback_tester")
        self.teach_client = self.create_client(TeachControl, "teach_control_real")
        self.playback_requested = False

    def wait_for_service(self) -> None:
        while not self.teach_client.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for teach_control_real service...")

    def send_teach_command(self, command: str, **kwargs):
        request = TeachControl.Request()
        request.command = command
        request.trajectory_id = kwargs.get("trajectory_id", "")
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

    def stop_playback(self) -> None:
        if not self.playback_requested:
            return
        response = self.send_teach_command("stop_playback")
        if response is not None:
            self.get_logger().info(response.message)
        self.playback_requested = False

    def run_playback(self) -> None:
        self.wait_for_service()
        trajectory_id = input("Trajectory name: ").strip()
        if not trajectory_id:
            self.get_logger().error("A trajectory name is required.")
            return

        speed_text = input("Playback speed (default 1.0): ").strip()
        try:
            play_speed = float(speed_text) if speed_text else 1.0
        except ValueError:
            play_speed = 1.0
        loop_playback = input("Loop playback? (y/N): ").strip().lower() == "y"

        input(
            "Confirm the robot is stable, the command topic has no unexpected publishers, "
            "and the playback area is clear. Press Enter to request MODE_MOTION."
        )
        response = self.send_teach_command(
            "play_trajectory",
            trajectory_id=trajectory_id,
            play_speed=play_speed,
            loop_playback=loop_playback,
        )
        if response is None or not response.success:
            self.get_logger().error(
                f"Could not start playback: {response.message if response else 'no response'}"
            )
            return

        self.playback_requested = True
        self.get_logger().info(response.message)
        if loop_playback:
            input("Looping. Press Enter to stop playback and request MODE_STAND.")
            self.stop_playback()
        else:
            self.get_logger().info(
                "Playback was accepted. The manager returns to MODE_STAND when the trajectory ends."
            )


def main(args=None) -> None:
    rclpy.init(args=args)
    node = TrajectoryPlaybackTester()
    try:
        node.run_playback()
    except KeyboardInterrupt:
        node.stop_playback()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
