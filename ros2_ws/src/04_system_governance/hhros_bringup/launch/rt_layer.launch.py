"""L0 real-time layer: the standalone, no-ROS runtime that owns the bus.

This process must come up FIRST so it creates the shared-memory segment before
the HAL attaches. On the real robot it runs with SCHED_FIFO + CPU affinity; here
the launch keeps it simple and lets the binary apply RT hygiene internally.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration


def generate_launch_description() -> LaunchDescription:
    shm_name = LaunchConfiguration("shm_name")
    backend = LaunchConfiguration("backend")
    rate = LaunchConfiguration("rate")

    # tmny edit
    # The L0 runtime is plain executable (no ROS), launched as a raw process.
    l0 = ExecuteProcess(
        cmd=[
            "ros2", "run", "hhros2_motor_runtime", "hhros2_l0_runtime",
            "--shm", shm_name,
            "--backend", backend,
            "--rate", rate,
        ],
        output="screen",
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("shm_name", default_value="hhros2_motor_shm"),
            DeclareLaunchArgument(
                "backend", default_value="sim",
                description="L0 backend: sim | ecat"),
            DeclareLaunchArgument("rate", default_value="1000.0"),
            l0,
        ]
    )
