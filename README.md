# yidong_robot

This repository is a ROS 2 workspace for the Yidong robot platform.  The
workspace is intentionally exposed at the repository root so deployment and
developer commands match standard ROS 2 habits.

## Layout

- `ros2_ws/`: ROS 2 workspace. All packages live under layered folders in
  `ros2_ws/src/`.
- `docs/`: Architecture notes and migration documentation.

Layered workspace (`ros2_ws/src/`):

| Layer | Folder | Packages |
|-------|--------|----------|
| 00 | `00_infrastructure` | `hhros2_interfaces`, `hhros2_description` |
| 01 | `01_hardware_bridge` | `hhros2_hal`, `hhros2_motor_protocol`, `hhros2_motor_runtime`, `hhros2_motor_diagnostics`, `xsens_mti_ros2_driver`, `ntrip` |
| 02 | `02_motion_control` | `hhros2_controllers`, `hhros2_estimation`, `hhros2_motion_cores` |
| 03 | `03_intelligence` | `hhros2_perception`, `hhros2_behavior` |
| 04 | `04_system_governance` | `hhros2_core`, `hhros_bringup` |
| 05 | `05_simulation_verification` | `hhros2_sim` |

All first-party packages now follow the `hhros2_*` naming convention. The
legacy `robot_*` stack (`robot_motor_bridge`, `robot_locomotion_control`,
`robot_lifecycle_manager`, `robot_teleop_joy`, `robot_test_tools`) and the
legacy `motor_interface` message package have been removed; their roles are
covered by `hhros2_hal`, `hhros2_motion_cores`, `hhros2_core`, and the unified
`hhros2_interfaces`. `xsens_mti_ros2_driver` and `ntrip` keep upstream names
as third-party drivers.

The NIIC SDK intentionally lives inside
`01_hardware_bridge/hhros2_motor_runtime/vendor/` so deployment is
self-contained. The SDK itself is not treated as a ROS package;
`hhros2_motor_runtime` is the ROS-facing wrapper.

## Architecture Boundary

The intended dependency direction is:

`hhros2_hal -> hhros2 SHM -> hhros2_motor_runtime -> NIIC EtherCAT SDK`

The HAL (`ros2_control` SystemInterface) and the L0 runtime communicate over the
shared-memory ABI defined in `hhros2_motor_protocol`. Diagnostics may observe
the path, but should not own control decisions or hardware state. Launch files
compose packages but should not contain motor protocol, hardware access, or
teleop policy logic.

## Build Note

This project uses one ROS environment only: the Conda environment named
`rosenv`, defined by `ros2_ws/environment.yml`. Do not source `/opt/ros` or
another ROS workspace in the same terminal.

Create or update the environment, then build the simulation workspace:

```bash
cd ros2_ws
./install_deps.sh
./auto_build.sh sim --jobs 1
```

For an interactive shell, zsh users can run `rosenv`. The equivalent explicit
setup is:

```bash
cd ros2_ws
source scripts/workspace_env.sh
hhros_setup_rosenv
hhros_source_workspace
```

The hardware runtime requires the Necro SDK package configuration:

```bash
export Necro_DIR=/path/to/directory/containing/NecroConfig.cmake
```

For mock/protocol/launch validation without hardware SDKs:

```bash
cd ros2_ws
./auto_build.sh sim --jobs 1
```

This is the package default; the explicit flag is only needed after a prior
configure cached a different value.

Real EtherCAT hardware additionally requires Necro and enables the vendor runtime:

```bash
export Necro_DIR=/path/to/directory/containing/NecroConfig.cmake
./auto_build.sh real --jobs 1
```

After structural package or workspace moves, clean stale generated artifacts
through the build entry point:

```bash
cd ros2_ws
./auto_build.sh sim --clean --jobs 1
```

## Launch

Whole platform bringup:

```bash
cd ros2_ws
source scripts/workspace_env.sh
hhros_setup_rosenv
hhros_source_workspace
ros2 launch hhros_bringup bringup.launch.py
```

Real hardware with IMU:

```bash
ros2 launch hhros_bringup bringup.launch.py hardware:=real enable_imu:=true
```

MuJoCo simulation:

```bash
ros2 launch hhros_bringup bringup.launch.py hardware:=mujoco
```
