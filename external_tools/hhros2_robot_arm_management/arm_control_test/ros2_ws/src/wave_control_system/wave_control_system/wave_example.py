#!/usr/bin/env python3
"""Generate eight-joint example trajectories for the legacy simulator."""

import argparse
import json
import math
from datetime import datetime
from pathlib import Path

from wave_control_system.legacy_sim_model import ARM_JOINTS, JOINT_COUNT


def _trajectory(identifier, description, frames, sample_rate):
    return {
        "id": identifier,
        "description": description,
        "joint_names": ARM_JOINTS,
        "num_joints": JOINT_COUNT,
        "record_frequency": sample_rate,
        "record_time": datetime.now().isoformat(),
        "frames": frames,
    }


def generate_wave_trajectory(duration=3.0, sample_rate=20.0):
    """Generate a right-arm wave with the other arm held still."""
    frames = []
    amplitude = math.radians(35.0)
    for index in range(int(duration * sample_rate) + 1):
        frame_time = index / sample_rate
        phase = 2.0 * math.pi * frame_time
        wave = amplitude * math.sin(phase)
        frames.append(
            {
                "time": frame_time,
                "positions": [
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    wave,
                    0.35 * wave,
                    0.20 * wave,
                    0.50 * wave,
                ],
                "velocities": [0.0] * JOINT_COUNT,
            }
        )
    return _trajectory("wave_example", "Eight-joint right arm wave", frames, sample_rate)


def generate_gesture_trajectories():
    """Return small, valid eight-joint trajectories for simulation checks."""
    trajectories = {"wave_example": generate_wave_trajectory()}

    circle_frames = []
    duration = 4.0
    sample_rate = 20.0
    radius = math.radians(20.0)
    for index in range(int(duration * sample_rate) + 1):
        frame_time = index / sample_rate
        phase = 2.0 * math.pi * frame_time / duration
        circle_frames.append(
            {
                "time": frame_time,
                "positions": [
                    radius * math.cos(phase),
                    radius * math.sin(phase),
                    0.0,
                    0.25 * radius * math.sin(phase),
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                ],
                "velocities": [0.0] * JOINT_COUNT,
            }
        )
    trajectories["circle_example"] = _trajectory(
        "circle_example", "Eight-joint left arm circle", circle_frames, sample_rate
    )
    return trajectories


def save_trajectories(trajectories, output_dir):
    output_path = Path(output_dir).expanduser()
    output_path.mkdir(parents=True, exist_ok=True)
    saved_files = []
    for name, trajectory in trajectories.items():
        filename = output_path / f"{name}.json"
        with filename.open("w", encoding="utf-8") as output:
            json.dump(trajectory, output, indent=2)
        saved_files.append(filename)
    return saved_files


def main():
    parser = argparse.ArgumentParser(
        description="Generate eight-joint trajectories for legacy_wave_sim."
    )
    parser.add_argument(
        "--trajectory-dir",
        default=str(Path.home() / "hhros2_legacy_arm_trajectories"),
    )
    args = parser.parse_args()

    saved_files = save_trajectories(
        generate_gesture_trajectories(), args.trajectory_dir
    )
    for filename in saved_files:
        print(f"Saved: {filename}")


if __name__ == "__main__":
    main()
