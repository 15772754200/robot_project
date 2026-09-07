# 机械臂录制与回放工作空间

本工作空间包含两套彼此隔离的功能：

1. `teach_manager_real`：适配当前 `hhros2` 框架的实机八关节手臂录制与回放。
2. 旧 `wave_*` 工具：保留的独立挥手仿真演示，不能控制实机。

## 实机八关节录制与回放

### 管理关节

```text
left_shoulder_pitch_joint
left_shoulder_roll_joint
left_shoulder_yaw_joint
left_elbow_joint
right_shoulder_pitch_joint
right_shoulder_roll_joint
right_shoulder_yaw_joint
right_elbow_joint
```

### 当前框架接口

| 用途 | 接口 |
| --- | --- |
| 关节反馈 | `/joint_states` (`sensor_msgs/msg/JointState`) |
| 关节命令 | `/humanoid_base_controller/reference` (`hhros2_interfaces/msg/JointMotor`) |
| 模式请求 | `/hhros2_core/set_control_mode` (`hhros2_interfaces/srv/SetControlMode`) |
| 模式确认 | `/humanoid/arbitration_mode` (`hhros2_interfaces/msg/ArbitrationMode`) |
| 示教服务 | `/teach_control_real` (`wave_control_msgs/srv/TeachControl`) |

录制仅在 `MODE_STAND=2` 下接受。回放会请求 `MODE_MOTION=6`，等待仲裁状态确认后才开始发布命令；完成、停止或异常时会请求回到 `MODE_STAND=2`。

### 构建

先构建主机器人工作空间，再构建本工作空间：

```bash
source /home/niic/yidong_robot_project/ros2_ws/install/setup.bash
cd /home/niic/yidong_robot_project/external_tools/hhros2_robot_arm_management/arm_control_test/ros2_ws
colcon build --packages-select wave_control_msgs wave_control_system
source install/setup.bash
```

### 启动实机示教节点

先正常启动主机器人框架并确认机器人姿态稳定、`/joint_states` 持续更新。在第二个终端执行：

```bash
source /home/niic/yidong_robot_project/ros2_ws/install/setup.bash
source /home/niic/yidong_robot_project/external_tools/hhros2_robot_arm_management/arm_control_test/ros2_ws/install/setup.bash
ros2 launch wave_control_system teach_real.launch.py
```

默认轨迹目录为 `~/hhros2_arm_trajectories`。可通过启动参数指定其他目录：

```bash
ros2 launch wave_control_system teach_real.launch.py trajectory_dir:=/home/niic/arm_trajectories
```

### 录制实机动作

1. 将机器人切换到站立模式，确认模式为 `MODE_STAND=2`。
2. 确认站立控制器的上肢阻尼允许人工带动手臂。
3. 启动交互式录制客户端：

```bash
ros2 run wave_control_system test_teach_real
```

客户端以 50 Hz 记录八个手臂关节的位置。按客户端提示开始录制、人工带动手臂、停止录制并保存轨迹。

也可直接调用服务：

```bash
ros2 service call /teach_control_real wave_control_msgs/srv/TeachControl "{command: 'start_record', record_frequency: 50.0}"
ros2 service call /teach_control_real wave_control_msgs/srv/TeachControl "{command: 'stop_record'}"
ros2 service call /teach_control_real wave_control_msgs/srv/TeachControl "{command: 'save_trajectory', trajectory_id: 'arm_demo'}"
```

### 回放实机动作

开始前检查实机命令话题，回放期间应只有本示教节点作为有意的发布者：

```bash
ros2 topic info /humanoid_base_controller/reference -v
```

启动交互式回放客户端：

```bash
ros2 run wave_control_system test_playback
```

输入已保存的轨迹名称、播放倍率和是否循环。循环播放时，按 Enter 可停止播放，节点随后会请求切回站立模式。

也可直接调用服务：

```bash
ros2 service call /teach_control_real wave_control_msgs/srv/TeachControl "{command: 'play_trajectory', trajectory_id: 'arm_demo', play_speed: 1.0, loop_playback: false}"
ros2 service call /teach_control_real wave_control_msgs/srv/TeachControl "{command: 'stop_playback'}"
```

### 实机安全检查

回放前和回放过程中，节点会检查：

- `/joint_states` 是否完整且最新反馈时间不超过 0.25 秒。
- 轨迹关节名、帧时间和每帧位置是否有效。
- 关节位置是否在配置的手臂限位内。
- 分段速度不超过 1.0 rad/s，分段加速度不超过 5.0 rad/s²。
- 首帧目标与当前手臂反馈的差值不超过 0.25 rad。
- 命令话题是否存在其他发布者。

这些参数可由启动参数覆盖，但实机上不应为了绕过检查而随意放宽。轨迹异常、反馈丢失或模式切换失败时，节点会停止命令并请求站立模式。

### 轨迹格式

实机回放只接受该节点生成的八关节 JSON。旧五/六关节轨迹不会被自动映射，以避免关节含义不明确。

```json
{
  "joint_names": [
    "left_shoulder_pitch_joint",
    "left_shoulder_roll_joint",
    "left_shoulder_yaw_joint",
    "left_elbow_joint",
    "right_shoulder_pitch_joint",
    "right_shoulder_roll_joint",
    "right_shoulder_yaw_joint",
    "right_elbow_joint"
  ],
  "frames": [
    {"time": 0.0, "positions": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]},
    {"time": 1.0, "positions": [0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}
  ]
}
```

## 独立八关节仿真示教

`wave_*`、`motor_driver_sim` 和 `teach_manager` 是独立的八关节手臂仿真示教系统。关节顺序与实机示教完全一致：左臂四关节在前、右臂四关节在后。它们只使用 `/legacy_wave_sim/*` 话题，不会发布当前框架的 `JointMotor`，也不使用控制模式仲裁、`/joint_states` 或 EtherCAT。

启动完整仿真示教链路：

```bash
ros2 launch wave_control_system teach_demo.launch.py
```

另开一个终端运行自动闭环演示。它会启动左臂模拟波形、从模拟驱动反馈录制八关节轨迹、保存为 `legacy_sim_smoke.json`，然后回放并保持最终位置：

```bash
ros2 run wave_control_system test_teach_function \
  --ros-args -r __ns:=/legacy_wave_sim
```

默认轨迹目录是 `~/hhros2_legacy_arm_trajectories`。可在启动时指定其他目录：

```bash
ros2 launch wave_control_system teach_demo.launch.py \
  trajectory_dir:=/home/niic/legacy_arm_trajectories
```

也可以分步调用服务：

```bash
ros2 service call /legacy_wave_sim/wave_control wave_control_msgs/srv/WaveControl \
  "{command: {robot_id: 'LEGACY_SIM', operation_type: 'wave_hello', wave_part: 'left_arm', amplitude: 25.0, frequency: 0.5}}"
ros2 service call /legacy_wave_sim/teach_control wave_control_msgs/srv/TeachControl \
  "{command: 'start_record', record_frequency: 20.0}"
ros2 service call /legacy_wave_sim/teach_control wave_control_msgs/srv/TeachControl \
  "{command: 'stop_record'}"
ros2 service call /legacy_wave_sim/teach_control wave_control_msgs/srv/TeachControl \
  "{command: 'save_trajectory', trajectory_id: 'sim_arm_demo'}"
ros2 service call /legacy_wave_sim/teach_control wave_control_msgs/srv/TeachControl \
  "{command: 'play_trajectory', trajectory_id: 'sim_arm_demo', play_speed: 1.0, loop_playback: false}"
```

`teach_manager` 只接受八个规范关节名、至少两帧且时间严格递增的轨迹。原有五/六关节轨迹没有明确映射关系，已被拒绝，不能用于仿真或实机回放。使用 `wave_system.launch.py` 与 `wave_client` 时只启动普通波形仿真，不包含录制和回放管理器。
