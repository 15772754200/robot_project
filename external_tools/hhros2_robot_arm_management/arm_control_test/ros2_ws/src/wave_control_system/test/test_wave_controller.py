from wave_control_system.legacy_sim_model import ARM_JOINTS, JOINT_COUNT, MOTOR_IDS
from wave_control_system.wave_example import generate_gesture_trajectories


def test_legacy_sim_model_matches_real_arm_joint_count():
    assert JOINT_COUNT == 8
    assert len(ARM_JOINTS) == 8
    assert MOTOR_IDS["both"] == list(range(1, 9))
    assert MOTOR_IDS["left_arm"] == [1, 2, 3, 4]
    assert MOTOR_IDS["right_arm"] == [5, 6, 7, 8]


def test_generated_trajectories_use_the_canonical_eight_joints():
    for trajectory in generate_gesture_trajectories().values():
        assert trajectory["joint_names"] == ARM_JOINTS
        assert trajectory["num_joints"] == JOINT_COUNT
        assert len(trajectory["frames"]) >= 2
        timestamps = [frame["time"] for frame in trajectory["frames"]]
        assert timestamps == sorted(timestamps)
        assert all(
            len(frame["positions"]) == JOINT_COUNT
            and len(frame["velocities"]) == JOINT_COUNT
            for frame in trajectory["frames"]
        )
