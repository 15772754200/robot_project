# 外设管理工具

本目录提供独立的 Linux 外设扫描和管理工具，不依赖 ROS，也不直接控制机器人关节或电机。

## 功能

- 扫描 USB、串口、网络接口、蓝牙、显示器和音频设备。
- 查看设备详细信息和按通信类型筛选设备。
- 启用或禁用网络接口、蓝牙设备。
- 导出设备列表和统计信息。
- 记录设备连接与启停操作日志。

主要脚本：

| 文件 | 用途 |
| --- | --- |
| `peripheral_manager.py` | 交互式外设管理程序 |
| `quick_scan.py` | 快速扫描并显示外设信息 |
| `install_peripheral.sh` | 安装依赖并配置命令别名 |

## 安装

在本目录执行：

```bash
chmod +x install_peripheral.sh
./install_peripheral.sh
```

安装完成后可使用以下别名：

```bash
pmgr
pmgr-sudo
```

也可以不安装别名，直接运行：

```bash
python3 peripheral_manager.py
sudo python3 peripheral_manager.py
python3 quick_scan.py
```

部分系统可能需要安装依赖：

```bash
sudo apt update
sudo apt install usbutils bluez net-tools
sudo pip3 install psutil pyserial tabulate
```

## 常用方式

快速查看当前设备：

```bash
python3 quick_scan.py
```

以普通权限启动交互式管理器：

```bash
pmgr
```

涉及网络接口、蓝牙状态或需要读取受限系统信息时，以管理员权限启动：

```bash
pmgr-sudo
```

## 日志

默认日志目录：

```text
~/.peripheral_manager/logs
```

可在启动前设置 `PERIPHERAL_MANAGER_LOG_DIR` 指定其他日志目录。

## 注意事项

- 禁用网卡或蓝牙前，先确认不会断开当前远程连接、手柄连接或调试链路。
- 通过 SSH 使用时，不要禁用正在使用的网络接口。
- 本工具不检查机器人是否处于运行状态；机器人执行测试时，应避免修改其通信相关设备。
