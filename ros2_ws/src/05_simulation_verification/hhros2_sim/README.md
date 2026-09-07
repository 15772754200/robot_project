# hhros2_sim

该包提供 MuJoCo `ros2_control` 硬件插件、基座/IMU 反馈、安全吊绳服务和独立观察器。

## 安全绳仿真流程

启动 MuJoCo。默认场景包含跟随式安全绳，硬件激活后物理立即开始。
短绳档位在正常站立高度留有少量余量，只在机器人继续下坠时承重：

```bash
ros2 launch hhros_bringup bringup.launch.py hardware:=mujoco
```

另开终端启动只读观察器：

```bash
ros2 run hhros2_sim mujoco_state_viewer.py
```

等待生命周期节点和控制器就绪后，显式请求 Stand。物理自动运行，控制权仍只
通过 `hhros2_core` 获取：

```bash
ros2 service call /hhros2_core/set_control_mode \
  hhros2_interfaces/srv/SetControlMode "{mode: 2}"
```

站立稳定后显式放绳。放绳后的自然长度为 `0.92 m`，正常站立时应无张力，
躯干明显下坠后 tendon 才承担防坠作用：

```bash
ros2 service call /mujoco/rope/release std_srvs/srv/Trigger "{}"
```

观察器默认持续跟踪 `base_link`，机器人行走后仍保持在视野中。绳索采用双层
表示：不可见的弹性 tendon 负责真实受力，12 段无碰撞胶囊根据
`/mujoco/rope/length` 画出余绳。胶囊明显弯曲表示有余量，逐渐拉直表示接近
承重；画面本身不参与物理计算。

逻辑关节顺序来自 `hhros2_description/config/joint_order.yaml`，初始基座和关节
姿态来自 `home_pose.yaml`。当前策略模型是 22-DoF plant，因此 23 个逻辑槽位中
仅 `head_yaw_joint` 不映射到 MuJoCo；配置不一致时启动会直接报错。
当前运行时支持 `scene.xml`、`scene_safety_rope.xml`、`scene_slope.xml` 和
`scene_stair.xml`；旧的 23-DoF `scene_stand.xml` 不属于当前部署契约。

切换 Walk 并持续发布速度：

```bash
ros2 service call /hhros2_core/set_control_mode \
  hhros2_interfaces/srv/SetControlMode "{mode: 3}"
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.15}, angular: {z: 0.0}}"
```

停止速度后可重新收绳：

```bash
ros2 topic pub --once /cmd_vel geometry_msgs/msg/Twist "{}"
ros2 service call /mujoco/rope/raise std_srvs/srv/Trigger "{}"
```

若要验证完全无绳场景，可覆盖模型：

```bash
ros2 launch hhros_bringup bringup.launch.py hardware:=mujoco \
  mujoco_model:=package://hhros2_description/models/Yidong/mjcf/scene.xml
```
