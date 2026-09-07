#!/usr/bin/env python3
"""Eight-joint state simulator for the isolated legacy teaching demo."""

import math
import time

import rclpy
from rclpy.node import Node

from wave_control_system.legacy_sim_model import ARM_JOINTS, JOINT_COUNT
from wave_control_msgs.msg import MotorCommand, WaveStatus


class MotorDriverSim(Node):
    UPDATE_RATE_HZ = 50.0
    DEFAULT_VELOCITY = 1.0

    def __init__(self):
        super().__init__("motor_driver_sim")
        self.positions = [0.0] * JOINT_COUNT
        self.targets = [0.0] * JOINT_COUNT
        self.command_velocities = [self.DEFAULT_VELOCITY] * JOINT_COUNT
        self.velocities = [0.0] * JOINT_COUNT
        self.last_action = "stopped"
        self.last_update_time = time.monotonic()
        self.create_subscription(
            MotorCommand, "motor_commands", self.motor_command_callback, 10
        )
        self.feedback_pub = self.create_publisher(WaveStatus, "sensor_feedback", 10)
        self.feedback_timer = self.create_timer(
            1.0 / self.UPDATE_RATE_HZ, self.update_and_publish_feedback
        )
        self.get_logger().info("Legacy eight-joint motor simulation ready.")

    def motor_command_callback(self, msg):
        if not (
            len(msg.motor_ids) == len(msg.positions) == len(msg.velocities)
            and msg.motor_ids
        ):
            self.get_logger().warn("Ignored malformed legacy motor command")
            return
        for motor_id, position, velocity in zip(
            msg.motor_ids, msg.positions, msg.velocities
        ):
            if (
                motor_id < 1
                or motor_id > JOINT_COUNT
                or not math.isfinite(position)
                or not math.isfinite(velocity)
            ):
                self.get_logger().warn(
                    "Ignored legacy command with invalid motor id, position, or velocity"
                )
                return

        for motor_id, position, velocity in zip(
            msg.motor_ids, msg.positions, msg.velocities
        ):
            index = motor_id - 1
            self.targets[index] = float(position)
            self.command_velocities[index] = max(abs(float(velocity)), self.DEFAULT_VELOCITY)
        self.last_action = "tracking"

    def update_and_publish_feedback(self):
        now = time.monotonic()
        dt = max(0.0, min(now - self.last_update_time, 0.1))
        self.last_update_time = now

        moving = False
        for index in range(JOINT_COUNT):
            error = self.targets[index] - self.positions[index]
            max_step = self.command_velocities[index] * dt
            step = max(-max_step, min(error, max_step))
            self.positions[index] += step
            self.velocities[index] = step / dt if dt > 1e-6 else 0.0
            moving = moving or abs(error) > 1e-5

        if not moving:
            self.last_action = "holding"

        feedback = WaveStatus()
        feedback.current_action = self.last_action
        feedback.current_amplitude = max(abs(value) for value in self.positions)
        feedback.current_frequency = 0.0
        feedback.wave_part = "simulated"
        feedback.joint_names = ARM_JOINTS
        feedback.positions = self.positions
        feedback.velocities = self.velocities
        self.feedback_pub.publish(feedback)


def main(args=None):
    rclpy.init(args=args)
    node = MotorDriverSim()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
