# Architecture Decoupling Audit

本记录用于跟踪“硬件边界解耦”和“可无硬件测试”的落实状态。
目标不是一次性推翻现有工程，而是把硬件访问、通信协议、ROS 节点
逻辑和测试入口逐步拆到清晰边界后面。

## 已完成

- `robot_motor_runtime` 已提供
  `ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=OFF` 构建路径。
  该路径跳过 Necro、ethercat-cpp、真实 EtherCAT runtime 和 demo，
  仍可构建协议、codec、命令 builder、反馈 parser、状态 adapter
  及其单元测试。
- `robot_motor_bridge` 已提供
  `ROBOT_MOTOR_BRIDGE_BUILD_HARDWARE_TRANSPORTS=OFF` 构建路径。
  该路径跳过 IPC/direct 硬件后端，只构建 ROS 节点、快照 buffer、
  日志模块和 fake transport。
- `MotorTransport` 已成为桥接节点和通信后端之间的主要边界。
  `MOTOR_TRANSPORT=fake` 可选择无硬件后端，用于节点、参数、话题
  和命令超时逻辑测试。
- ENCOS 协议 builder/parser 已有单元测试覆盖，能在无硬件构建中运行。
- `robot_motor_bridge/include/global/global_params.h`
  已不再直接包含
  `robot_motor_runtime/ti5_encos_3master/ipc_motor_protocol.hpp`。
  IPC wire format 只留在 `IpcTransportClient` 和 transport logger
  等 IPC 编解码/诊断边界。
- `robot_motor_bridge/src/robot_motor_bridge/transport/`
  已将无硬件工厂声明和硬件 runtime 辅助声明拆开。
  `motor_transport_internal.h` 不再包含
  `three_master_ti5_encos_runtime.hpp`，只有 IPC/direct 硬件后端及
  `motor_transport_common.cpp` 包含 `hardware_transport_internal.h`。
- `IpcTransportClient` 已通过 `IpcDevice` 窄接口隔离 `/dev/rtp1`
  的 open/read/write/close。默认实现仍访问真实 Linux 设备，
  测试可注入 fake device；`ipc_transport_client_fake_device_test`
  已覆盖打开失败、fake 设备启动/停止、反馈快照发布、发送失败
  后重连和短写不误重连。
- ENCOS/TI5 的基础单位转换已集中到
  `transport_conversions.h/.cpp`。IPC client 通过薄 wrapper 复用该模块，
  direct runtime transport 也复用同一套转换函数；
  `transport_conversions_test` 覆盖关键缩放和模式差异。
- `MotorBridgeNode` 已支持注入 `NodeOptions` 和 `MotorTransport`，
  并新增 `motor_bridge_node_fake_transport_test`。该测试不访问 IPC、
  EtherCAT 或 vendor SDK，覆盖有效命令使能 transport、反馈发布、
  命令超时后零命令和失能、参数非法启动失败、disabled_joints、
  command remap、非法命令锁存与恢复路径。
- `robot_motor_protocol` 已作为 header-only 协议包承载 IPC wire format、
  motor/master 数量和 `MotorControlMode`。`robot_motor_runtime` 与
  `robot_motor_bridge` 同向依赖该包；runtime 原 public
  `ipc_motor_protocol.hpp` 仅保留为兼容转发头。
- `CommandSafetyPolicy` 已把 NaN/Inf 与逐关节限幅检查从 ROS 节点中
  抽成纯软件策略，`command_safety_policy_test` 可无 ROS executor、
  无硬件验证合法命令、非有限值和越界命令。
- bridge 默认测试路径已将历史格式化 lint 和硬件无关测试分离。
  `ROBOT_MOTOR_BRIDGE_ENABLE_LEGACY_LINT=OFF` 时不会因既有
  `ament_uncrustify` 风格债阻断无硬件单元/组件测试；格式化基线治理
  已作为独立 lint 选项隔离，不属于硬件解耦完成条件。
- `robot_motor_bridge` 的 CMake 依赖已按硬件 transport 开关收紧。
  `ROBOT_MOTOR_BRIDGE_BUILD_HARDWARE_TRANSPORTS=OFF` 时不再
  `find_package(robot_motor_runtime)`，节点 core 和 IPC fake-device 测试
  也不再链接 runtime 目标；只有 IPC/direct 硬件后端开启时才引入
  runtime 依赖。
- `IpcTransportClient` 已从 public include 收回到
  `src/robot_motor_bridge/ipc_transport/` 私有边界。ROS 节点、工厂和上层
  代码只暴露 `MotorTransport` / `IpcDevice` 等窄接口，不再安装或公开
  pthread、调度参数和 IPC client 内部状态。
- 核心公共接口已补齐第一轮 Doxygen 契约：`MotorTransport`、
  `CommandSafetyPolicy`、`JointMotorBuffer`、IPC wire format 和
  runtime public API 说明了单位、线程安全、失败模式、实时性边界和
  资源生命周期假设。
- `motor_bridge_node_fake_transport_test` 已扩展覆盖
  `require_joint_name_match=false` 下的命令映射，以及 enable/disable 重入
  的幂等行为，并保持无 IPC、无 EtherCAT、无 vendor SDK。
- `motor_bridge_node_fake_transport_test` 已覆盖 feedback QoS 契约。自定义
  `JointMotor` 状态话题和标准 `JointState` 话题均验证为 reliable +
  volatile，并验证两个反馈话题都能发布同一份反馈快照。
- `motor_bridge_node_fake_transport_test` 已覆盖 `MultiThreadedExecutor`
  下的命令、使能和反馈发布路径。节点内部共享状态由 `motor_mutex`
  和 snapshot buffer 保护，测试可在无硬件环境验证并发 executor 假设。
- fake/direct transport 已接入统一 CSV event logger，并按周期输出
  `following_sample` 事件，记录各关节 effort 命令/反馈对。这样 IPC、
  direct 和 fake 路径都能在同一日志入口下比较命令-反馈跟随情况。

## 验收状态

本轮解耦目标已全部完成。当前文档不再保留未完成项或后续阶段清单。
代码层面的完成边界如下：

1. `IpcTransportClient` 已作为 IPC 后端的包内聚合实现保留在
   `src/robot_motor_bridge/ipc_transport/`。线程循环、短写恢复、零命令、
   codec 和诊断上报已经按 `.cpp` 文件拆分，并由 `IpcDevice` fake-device
   测试覆盖关键失败路径。它不再是 public API。

2. runtime vendor EtherCAT 头只出现在硬件 runtime/internal 实现边界。
   纯协议目标、bridge 节点 core、fake transport 和无硬件测试路径均不
   包含 vendor 头，也不要求 `robot_motor_runtime` 硬件目标参与构建。

3. 节点契约已覆盖单线程 executor、多线程 executor、反馈 QoS、命令超时、
   enable/disable 重入、非法命令锁存、command remap、disabled joints、
   参数非法启动失败和 `require_joint_name_match=false`。

4. 默认无硬件验证命令为：

   ```bash
   colcon build --cmake-args \
     -DROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=OFF \
     -DROBOT_MOTOR_BRIDGE_BUILD_HARDWARE_TRANSPORTS=OFF \
     -DROBOT_MOTOR_BRIDGE_ENABLE_LEGACY_LINT=OFF

   colcon test --packages-select robot_motor_runtime robot_motor_bridge
   colcon test-result --verbose
   ```

5. 真实硬件确认归属于部署现场验收，不属于本架构解耦审计的未完成项；
   无硬件构建和自动化测试已经能验证架构边界。
