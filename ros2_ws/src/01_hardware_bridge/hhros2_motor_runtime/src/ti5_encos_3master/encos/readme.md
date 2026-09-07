# ENCOS EtherCAT-CAN 映射说明

## passage 与 motor_id

ENCOS 转接板每个 EtherCAT 从站默认承载 6 路 CAN 报文槽位。
代码里把这个 1-based 槽位称为 `passage`：

| passage | CAN 侧含义 | `EtherCAT_Msg` 槽位 |
| -------: | ---------- | ------------------: |
| 1 | CAN1 第 1 路 | `motor[0]` |
| 2 | CAN1 第 2 路 | `motor[1]` |
| 3 | CAN1 第 3 路 | `motor[2]` |
| 4 | CAN2 第 1 路 | `motor[3]` |
| 5 | CAN2 第 2 路 | `motor[4]` |
| 6 | CAN2 第 3 路 | `motor[5]` |

`motor_id` 是写入 CAN 帧 `id` 字段的目标电机 CAN ID。
它和 `passage` 不是同一个概念。当前硬件默认使用
`passage N -> motor_id N`，但这只是拓扑配置，不是协议要求。

默认映射集中定义在：

```text
internal/encos_motor_route.hpp
```

周期控制路径，包括正常位置命令、力位混合命令和 disabled 零命令，
都必须通过这张路由表获取 `passage` 和 `motor_id`。如果现场电机
CAN ID 或接线顺序变化，只改这个文件里的 `kDefaultMotorRoutes`，
不要在各个命令函数里单独写 `motor_offset + 1`。

## CAN TX PDO subIndex

下面是第 1 路 CAN 报文槽位的 TX PDO subIndex。完整 6 路映射在
`encos_io_controller.cpp` 的 `kCanTxPdoSubIndexes` 中维护，字段顺序
固定为：

```text
MotorId, RTR, DLC, Data0, Data1, Data2, Data3, Data4, Data5, Data6, Data7
```

| CAN 字段  | PDO subIndex |
| ------- | -----------: |
| MotorId |            3 |
| RTR     |            4 |
| DLC     |            5 |
| Data0   |            6 |
| Data1   |            7 |
| Data2   |            8 |
| Data3   |            9 |
| Data4   |           16 |
| Data5   |           17 |
| Data6   |           18 |
| Data7   |           19 |

这张 subIndex 表来自 ENCOS EtherCAT-CAN 从站的 PDO/对象字典配置。
运行时代码先用 `encos_command_builder.cpp` 构造 `EtherCAT_Msg` 中的
CAN 帧，再由 `write_can_tx_to_pdo()` 按 subIndex 把 CAN 字段写入
实际 PDO entry。
