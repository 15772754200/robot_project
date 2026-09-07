"""Shared eight-joint model for the isolated legacy arm simulation."""

ARM_JOINTS = [
    "left_shoulder_pitch_joint",
    "left_shoulder_roll_joint",
    "left_shoulder_yaw_joint",
    "left_elbow_joint",
    "right_shoulder_pitch_joint",
    "right_shoulder_roll_joint",
    "right_shoulder_yaw_joint",
    "right_elbow_joint",
]

JOINT_COUNT = len(ARM_JOINTS)

MOTOR_IDS = {
    "left_arm": [1, 2, 3, 4],
    "right_arm": [5, 6, 7, 8],
    "both": list(range(1, JOINT_COUNT + 1)),
}
