# 和辉人形机器人Workspace Rebuild说明（2026-05---2026-06 tmny）

本文记录对移动机器人项目的结构性整理：把散落的旧栈包统一到分层并命名规范。
目标是让整个工作区只保留一套清晰的、命名一致的hhros2 软件栈，更符合现代化
且更干净的工程基线。



## 0. 改版的特点

1. 全栈统一命名。 

2. 系统按00–05分层组织的，由hhros_bringup 统一拉起（ros2_control + 共享内存双环架构）。
   所有包归入按"基础设施→硬件桥接→运动控制→智能→系统治理→仿真验证"排列的分层目录，
   目录树即架构图，编号高低即依赖方向。

3. 单一软件栈，无遗留混杂。 

4. 统一消息契约。 JointMotor等接口统一收敛到hhros2_interfaces，消除等价重复。

5. 默认可无硬件构建。 硬件/重依赖改为显式开关且默认关闭
  （ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=OFF），开发机、CI无需Necro、LibTorch
   即可完整colcon build；真机按需显式打开。





## 1. 项目的最终结构


层级关系：基础设施→硬件桥接→运动控制→智能→系统治理→仿真验证

```
ros2_ws/src/
├── 00_infrastructure/                # 00基础设施层   
│   ├── hhros2_interfaces             # 统一消息/服务/动作接口
│   ├── hhros2_description            # URDF / xacro / 模型
|   └── hhros2_log                    # 统一日志管理
├── 01_hardware_bridge/               # 01硬件桥接层
│   ├── hhros2_hal                    # ros2_control SystemInterface（HAL）
│   ├── hhros2_motor_protocol         # 共享内存 / IPC 协议（header-only）
│   ├── hhros2_motor_runtime          # L0 实时进程（hhros2_l0_runtime）+ EtherCAT runtime
│   ├── hhros2_motor_diagnostics      # 电机诊断 / 延迟探测 / 脚裸关节测试
│   ├── ntrip                         # 第三方：RTK/NTRIP（保留原名）
│   └── xsens_mti_ros2_driver         # 第三方：Xsens IMU 驱动（保留原名）
├── 02_motion_control/                # 02运动控制层
│   ├── hhros2_controllers            # humanoid_base / damping 等控制器
│   ├── hhros2_estimation             # 状态估计
│   ├── hhros2_motion_cores           # RL policy core / WBC core / fsm
|   ├── hhros2_mechanisms             # 裸关节串并联解算，在hhros2_ecat_l0_backend中调用
|   └── hhros2_teleop                 # 键盘/遥控/远程控制
├── 03_intelligence/                  # 03智能层
│   ├── hhros2_perception             # 感知规划层
│   └── hhros2_behavior               # 业务决策层
├── 04_system_governance/             # 04系统治理层 
│   ├── hhros2_core                   # 安全治理 / 模式仲裁
│   └── hhros_bringup                 # 分层 launch / QoS / 启动时序
│── 05_simulation_verification/       # 05仿真验证层 
│   └── hhros2_sim                    # MuJoCo 仿真 SystemInterface
│
├external_tools                       # 外部工具
|   ├── hhros2_mic                    # 麦克风独立工具；mic.py 与 M2 麦克风 SDK/使用文档
|   ├── hhros2_realsense              # RealSense 独立工具与 realsense-ros 源码工作区
|   ├── hhros2_robot_arm_management   # 机械臂、系统管理与外设管理补充工具
|   ├── hhros2_radar                  # RoboSense rslidar_sdk 与 rslidar_msg
|   ├── hhros2_menu_services_script   # 客户测试菜单、服务脚本（蓝牙、网络、RTC、遥控等测试脚本）
|   ├── hhros2_thirdparty             # 第三方库
|   └── hhros2_embedded               # 机器人四肢电源供电相关硬件包
```



所有一方包统一 hhros2_* 前缀；电机基础设施改为hhros2_motor_protocol、hhros2_motor_runtime、hhros2_motor_diagnostics；
  ntrip、xsens_mti_ros2_driver 为第三方驱动，保留原名只做了归属层级转移。

---旧包转新包以及删除的包 
```
# robot_motor_bridge  → hhros2_hal 
# robot_locomotion_control  →  hhros2_motion_cores + hhros2_controllers
# robot_lifecycle_manager  →  hhros2_core  
# robot_teleop_joy    （未接入，移除） 
# robot_test_tools    （测试工具，移除）
# 其余包根据功能层级，归并到相关层，并修改前缀名
```


### 依赖方向

```
hhros2_hal ──> hhros2 共享内存(ABI: hhros2_motor_protocol) ──> hhros2_motor_runtime ──> NIIC EtherCAT SDK
hhros2_core ──(switch_controller)──> hhros2_controllers
hhros2_controllers ──> hhros2_motion_cores
hhros2_motor_runtime ──> hhros2_motor_protocol + hhros2_motor_diagnostics
hhros2_motor_diagnostics ──> hhros2_interfaces
```

### RL 运行环境

ONNX Runtime、MuJoCo 和 ROS 2 Humble 都由 `ros2_ws/environment.yml`
中的 Conda `rosenv` 唯一提供，不再手工安装或切换其他版本。
---




## 2. 构建选项

### 无硬件构建（默认此选项）

```bash
cd ros2_ws
./install_deps.sh
./auto_build.sh sim --jobs 1
```

### 真机EtherCAT构建  此指令没有执行，待验证。

```bash
export Necro_DIR=/path/to/directory/containing/NecroConfig.cmake
./auto_build.sh real --jobs 1
```
### 安装onnxruntime方法如下：
```bash
cd /tmp
x86_64版本：
wget https://github.com/microsoft/onnxruntime/releases/download/v1.27.0/onnxruntime-linux-x64-1.27.0.tgz
aarch64版本：
wget https://github.com/microsoft/onnxruntime/releases/download/v1.27.0/onnxruntime-linux-aarch64-1.27.0.tgz
解压后放到external_tools/hhros2_thirdarty目录下
```
### 安装mujoco方法如下：
```bash
# 第一次部署时在/home/niic下创建mkdir .mujoco文件夹，然后 wget https://github.com/google-deepmind/mujoco/releases/download/3.3.7/mujoco-3.3.7-linux-aarch64.tar.gz
# 最后将压缩包解压到.mujoco文件夹下，解压后文件夹名为mujoco-3.3.7
```

## 3. 启动与验证

### 启动

```bash
source scripts/workspace_env.sh
hhros_setup_rosenv
hhros_source_workspace

ros2 launch hhros_bringup bringup.launch.py hardware:=mujoco        # 仿真 需要安装python3 -m pip install mujoco
ros2 launch hhros_bringup bringup.launch.py hardware:=mock          # 空载  
ros2 launch hhros_bringup bringup.launch.py hardware:=real enable_imu:=true   # 真机 未验证

启动时序（bringup.launch.py）：

```
t0    L0 实时进程（仅 hardware:=real）── 创建共享内存段
t0+1s 传感器层（可选 IMU / NTRIP）
t0+2s 控制层 controller_manager + 广播器 + base/damping(INACTIVE) + 估计/核心容器
t0+4s 治理层 hhros2_core（仲裁 + 安全监控）
```

典型调用：

```bash
# 1) 切到 stand，自动激活 humanoid_base_controller、停用 damping_controller
ros2 service call /hhros2_core/set_control_mode \
  hhros2_interfaces/srv/SetControlMode "{mode: 2}"

# 2) 总使能（真机会真正上力，先确认机器人已支撑/悬挂，需要真机验证）
ros2 service call /hhros2_core/set_system_state \
  hhros2_interfaces/srv/SetSystemState "{enable: true}"
```

验证：

```bash
ros2 service call /controller_manager/list_controllers \
  controller_manager_msgs/srv/ListControllers   # humanoid_base_controller=active
ros2 topic echo /hhros2_core/safety_status --once  # level=0 (LEVEL_OK)
ros2 topic echo /joint_states                      # position 向 stand 收敛
```

```
脚裸关节算法测试步骤：
1：修改bringup.launch.py和control_layer.launch.py,使用ankle_parallel_sweep_probe测试部分的配置，确保只有一个命令话题发布者
2：启动start_robot.sh ankle_test
3：启动脚本ankle_test_control.sh,脚本中可以修改当前测试的是哪个关节，测试完后可以在tmp/ankle_parallel_sweep_probe.csv中查看测试结果数据
```
