#!/usr/bin/env python3
"""Isolated legacy wave simulation; it is not connected to the real robot."""

import math
import time

import rclpy
from rclpy.node import Node

from wave_control_system.legacy_sim_model import ARM_JOINTS, JOINT_COUNT, MOTOR_IDS
from wave_control_msgs.msg import MotorCommand, WaveCommand, WaveStatus
from wave_control_msgs.srv import WaveControl


class WaveController(Node):
    """Produce continuous radian-space wave commands for the legacy simulator."""

    UPDATE_RATE_HZ = 50.0
    MOTOR_IDS = MOTOR_IDS
    ARM_PROFILE = [1.0, 0.35, 0.20, 0.50]

    def __init__(self):
        super().__init__("wave_controller")
        self.declare_parameter("default_amplitude", 45.0)
        self.declare_parameter("default_frequency", 1.0)
        self.declare_parameter("max_amplitude", 90.0)
        self.declare_parameter("min_amplitude", 10.0)

        self.is_waving = False
        self.current_wave_part = "right_arm"
        self.current_amplitude = float(self.get_parameter("default_amplitude").value)
        self.current_frequency = float(self.get_parameter("default_frequency").value)
        self.wave_start_time = time.monotonic()
        self.last_positions = [0.0] * JOINT_COUNT

        self.wave_service = self.create_service(
            WaveControl, "wave_control", self.handle_wave_control
        )
        self.motor_cmd_pub = self.create_publisher(MotorCommand, "motor_commands", 10)
        self.status_pub = self.create_publisher(WaveStatus, "wave_status", 10)
        self.create_subscription(WaveStatus, "sensor_feedback", self.sensor_feedback_callback, 10)
        self.wave_timer = self.create_timer(
            1.0 / self.UPDATE_RATE_HZ, self.execute_wave_motion
        )

        self.get_logger().info(
            "Legacy wave simulation ready. It only uses relative legacy_wave_sim topics."
        )

    def validate_parameters(self, command: WaveCommand):
        if command.operation_type == "stop":
            return True, "stop accepted"
        if command.wave_part not in self.MOTOR_IDS:
            return False, "wave_part must be left_arm, right_arm, or both"
        min_amp = float(self.get_parameter("min_amplitude").value)
        max_amp = float(self.get_parameter("max_amplitude").value)
        if not min_amp <= command.amplitude <= max_amp:
            return False, f"amplitude must be between {min_amp} and {max_amp} degrees"
        if not 0.1 <= command.frequency <= 5.0:
            return False, "frequency must be between 0.1 and 5.0 Hz"
        return True, "parameters valid"

    def handle_wave_control(self, request, response):
        command = request.command
        valid, message = self.validate_parameters(command)
        if not valid:
            response.success = False
            response.message = message
        elif command.operation_type == "wave_hello":
            response = self.start_waving(command, response)
        elif command.operation_type == "change_state":
            response = self.change_wave_state(command, response)
        elif command.operation_type == "stop":
            response = self.stop_waving(response)
        else:
            response.success = False
            response.message = "operation_type must be wave_hello, change_state, or stop"
        response.current_status = self.get_current_status()
        return response

    def start_waving(self, command, response):
        if self.is_waving:
            response.success = False
            response.message = "already waving; use change_state"
            return response
        self._set_wave(command)
        self.is_waving = True
        response.success = True
        response.message = "legacy simulation wave started"
        return response

    def change_wave_state(self, command, response):
        if not self.is_waving:
            response.success = False
            response.message = "not currently waving"
            return response
        self._set_wave(command)
        response.success = True
        response.message = "legacy simulation wave parameters updated"
        return response

    def _set_wave(self, command):
        self.current_wave_part = command.wave_part
        self.current_amplitude = float(command.amplitude)
        self.current_frequency = float(command.frequency)
        self.wave_start_time = time.monotonic()

    def stop_waving(self, response):
        if not self.is_waving:
            response.success = False
            response.message = "not currently waving"
            return response
        self.is_waving = False
        self._publish_motor_command(MOTOR_IDS["both"], self.last_positions, 0.0)
        response.success = True
        response.message = "legacy simulation wave stopped and held"
        return response

    def execute_wave_motion(self):
        if not self.is_waving:
            return
        amplitude_rad = math.radians(self.current_amplitude)
        phase = 2.0 * math.pi * self.current_frequency * (
            time.monotonic() - self.wave_start_time
        )
        target = amplitude_rad * math.sin(phase)
        target_velocity = abs(2.0 * math.pi * self.current_frequency * amplitude_rad)
        motor_ids = self.MOTOR_IDS[self.current_wave_part]
        positions = [
            target * self.ARM_PROFILE[(motor_id - 1) % len(self.ARM_PROFILE)]
            for motor_id in motor_ids
        ]
        velocities = [
            target_velocity * self.ARM_PROFILE[(motor_id - 1) % len(self.ARM_PROFILE)]
            for motor_id in motor_ids
        ]
        self._publish_motor_command(motor_ids, positions, velocities)

        status = self.get_current_status()
        status.current_action = "waving"
        self.status_pub.publish(status)

    def _publish_motor_command(self, motor_ids, positions, velocities):
        for motor_id, position in zip(motor_ids, positions):
            self.last_positions[motor_id - 1] = position
        command = MotorCommand()
        command.motor_ids = motor_ids
        command.positions = positions
        command.velocities = (
            [velocities] * len(motor_ids)
            if isinstance(velocities, (int, float))
            else velocities
        )
        self.motor_cmd_pub.publish(command)

    def sensor_feedback_callback(self, msg):
        if self.is_waving:
            self.get_logger().debug(f"Legacy simulation feedback: {msg.current_action}")

    def get_current_status(self):
        status = WaveStatus()
        status.current_action = "waving" if self.is_waving else "stopped"
        status.current_frequency = self.current_frequency
        status.current_amplitude = self.current_amplitude
        status.wave_part = self.current_wave_part
        status.joint_names = ARM_JOINTS
        status.positions = self.last_positions
        status.velocities = [0.0] * JOINT_COUNT
        return status


def main(args=None):
    rclpy.init(args=args)
    node = WaveController()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
