#!/usr/bin/env python3
"""Generate a canonical eight-joint sine trajectory for legacy_wave_sim."""

import argparse
import json
import math
from datetime import datetime
from pathlib import Path

from wave_control_system.legacy_sim_model import ARM_JOINTS, JOINT_COUNT


def generate_sine_trajectory(
    duration=10.0, frequency=50.0, amplitude=30.0, period=5.0
):
    if duration <= 0.0 or frequency <= 0.0 or period <= 0.0:
        raise ValueError("duration, frequency, and period must be positive")

    frames = []
    amplitude_rad = math.radians(amplitude)
    scales = [1.0, 0.35, 0.20, 0.50, 0.80, 0.28, 0.16, 0.40]
    omega = 2.0 * math.pi / period
    for index in range(int(duration * frequency) + 1):
        frame_time = index / frequency
        phase_offsets = [
            0.0,
            0.2,
            0.4,
            0.6,
            math.pi,
            math.pi + 0.2,
            math.pi + 0.4,
            math.pi + 0.6,
        ]
        positions = [
            amplitude_rad * scale * math.sin(omega * frame_time + phase_offset)
            for scale, phase_offset in zip(scales, phase_offsets)
        ]
        velocities = [
            amplitude_rad * scale * omega * math.cos(omega * frame_time + phase_offset)
            for scale, phase_offset in zip(scales, phase_offsets)
        ]
        frames.append(
            {
                "time": frame_time,
                "positions": positions,
                "velocities": velocities,
            }
        )

    return {
        "id": "sine_wave_test",
        "description": f"Eight-joint sine trajectory, period={period}s amplitude={amplitude}deg",
        "joint_names": ARM_JOINTS,
        "num_joints": JOINT_COUNT,
        "record_frequency": frequency,
        "record_time": datetime.now().isoformat(),
        "data_source": "generated_sine_wave",
        "frames": frames,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Generate an eight-joint sine trajectory for legacy_wave_sim."
    )
    parser.add_argument(
        "--trajectory-dir",
        default=str(Path.home() / "hhros2_legacy_arm_trajectories"),
    )
    parser.add_argument("--duration", type=float, default=10.0)
    parser.add_argument("--amplitude", type=float, default=30.0)
    parser.add_argument("--period", type=float, default=5.0)
    parser.add_argument("--frequency", type=float, default=50.0)
    args = parser.parse_args()

    trajectory = generate_sine_trajectory(
        duration=args.duration,
        amplitude=args.amplitude,
        period=args.period,
        frequency=args.frequency,
    )
    output_dir = Path(args.trajectory_dir).expanduser()
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / "sine_wave_test.json"
    with output_path.open("w", encoding="utf-8") as output:
        json.dump(trajectory, output, indent=2)
    print(f"Saved {len(trajectory['frames'])} eight-joint frames to {output_path}")


if __name__ == "__main__":
    main()
