## 开启四肢先开启的哪个电源然后延时
开启“四肢电源”对应命令是 `open-limbs-power`，实际发送顺序在：

[hardware_driver.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/hardware_driver.cpp:909)

逻辑是：

```cpp
if (command == PowerControlCommand::OpenLimbs)
{
    return {
        powerControlBytes(config, PowerControlCommand::OpenUpperLimbs),
        powerControlBytes(config, PowerControlCommand::OpenLimbs),
    };
}
```

也就是：先开上肢电源 `OpenUpperLimbs`，然后再发送全四肢开启 `OpenLimbs`。

两帧之间的延时在：

[hardware_driver.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/hardware_driver.cpp:1814)

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(10));
```

所以结论：**开启四肢时先开启上肢电源，延时 10ms 后再发送四肢全部开启命令**。

入口映射在 [power_node.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/power_node.cpp:73)：`open-limbs-power -> PowerControlCommand::OpenLimbs`。

## 下发开启电源的完整帧结构
按当前代码和配置 `rs485_id = 1`，电源控制帧结构来自 [hardware_driver.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/hardware_driver.cpp:862)。

帧格式是：

```text
A5 5A | LEN_L LEN_H | RS485_ID | PAYLOAD... | CRC8
```

长度 `LEN` 包含整帧长度：帧头 2 字节 + 长度 2 字节 + ID 1 字节 + payload + CRC 1 字节。  
这两个命令 payload 都是 13 字节，所以总长度是 `19`，小端写成 `13 00`。

**开启上肢电源 `OpenUpperLimbs`**

payload 来源在 [hardware_driver.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/hardware_driver.cpp:881)：

```text
01 01 01 00 00 64 00 64 00 64 00 64 00
```

完整帧：

```text
A5 5A 13 00 01 01 01 01 00 00 64 00 64 00 64 00 64 00 19
```

拆开看：

```text
A5 5A       帧头
13 00       长度 19
01          RS485 ID
01          电源控制类命令/功能字
01 01       上肢两路开启
00 00       下肢两路关闭
64 00       参数1 = 0x0064
64 00       参数2 = 0x0064
64 00       参数3 = 0x0064
64 00       参数4 = 0x0064
19          CRC8
```

**开启四肢电源 `OpenLimbs`**

payload 来源在 [hardware_driver.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/hardware_driver.cpp:887)：

```text
01 01 01 01 01 64 00 64 00 64 00 64 00
```

完整帧：

```text
A5 5A 13 00 01 01 01 01 01 01 64 00 64 00 64 00 64 00 08
```

拆开看：

```text
A5 5A       帧头
13 00       长度 19
01          RS485 ID
01          电源控制类命令/功能字
01 01       上肢两路开启
01 01       下肢两路开启
64 00       参数1 = 0x0064
64 00       参数2 = 0x0064
64 00       参数3 = 0x0064
64 00       参数4 = 0x0064
08          CRC8
```

现在执行“开启四肢电源”时，实际会先发上肢完整帧，延时 `10ms`，再发四肢完整帧。


## 当前代码会打印出RX的数据帧吧，无论是否有效
对，**在 `customer_panel` 里，只要 service response 的 `raw_frame` 非空，就会打印出来，不管这次结果是 success 还是 failed。**

位置在 [customer_panel.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_bringup/src/customer_panel.cpp:423)：

```cpp
printResult(response->success, response->code, response->message);

if (!response->request_frame.empty())
{
    std::cout << "request_frame: " << bytesToHex(response->request_frame) << "\n";
}
std::cout << "command_code: " << static_cast<int>(response->command_code) << "\n";
std::cout << "result: " << static_cast<int>(response->result) << "\n";
if (!response->raw_frame.empty())
{
    std::cout << "raw_frame: " << bytesToHex(response->raw_frame) << "\n";
}
```

所以判断条件不是 `success`，而是：

```cpp
!response->raw_frame.empty()
```

也就是说：

- 收到正确响应帧：会打印 `raw_frame`
- 收到业务失败响应帧：也会打印 `raw_frame`
- 收到 CRC 错/解析失败/超时但有 RX 数据：driver 会把 buffer 填进 `raw_frame`，`customer_panel` 也会打印
- 完全没收到任何 RX 数据，或者串口打开/发送阶段就失败：`raw_frame` 为空，就不会打印

所以你的理解基本对：**只要底层收到了任何 RX 数据并放进 response，customer_panel 就会显示出来，无论是否有效。**

在驱动源码里硬编码设置的，不在 yaml 配置里。

位置是 [hardware_driver.cpp](/home/niic/yidong_robot_project/external_tools/hhros2_embebded/src/robot_embeded_driver/src/hardware_driver.cpp:1845)：

```cpp
const auto command_interval = std::chrono::milliseconds(30);
```

这段在 `sendPowerControl()` 里，只对 `open-limbs-power` 这种多帧命令生效。当前流程是：

```text
发送上肢开启帧
读取上肢返回
等待到距离第一帧发送至少 30ms
发送四肢开启帧
读取四肢返回
```

也就是说，“上下肢分时”的指令间隔现在就是这里的 `30ms`。