# readme

## 电机模式

力位混合模式+位置模式

```c++
enum class MotorControlMode
{
    PositionMode,
    HybridForcePositionMode,    //这个翻译为中文是 力位混合模式
};
```

每个主站对应的电机数

```c++
struct MotorGroup
{
    MotorMessageSingle motor_id[kMotorCountPerMaster];
};
```

这里的port不是ros topic的端口，也不是TCP/UDP网络端口，而是实时IPC设备/通道端口。bridge侧会根据这个编号打开：

```cpp
/dev/rtp1
```

```c++
constexpr int kIpcMotorPort = 1;
constexpr int kMotorCountPerMaster = 23;
constexpr int kMasterCount = 3;
```

对应代码在`ipc device`里：

```cpp
std::snprintf(device_name, sizeof(device_name), "/dev/rtp%d", port)
```

23是每个master最多提供23个电机曹位

按照在实际使用的话

```text
master0: ENCOS，下肢相关，实际使用 12 个
master1: TI5，实际使用 5 个
master2: TI5，实际使用 6 个
```
