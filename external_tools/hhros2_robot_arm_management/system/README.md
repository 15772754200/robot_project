# Linux 系统管理工具

本目录提供独立的 Linux 系统维护工具，用于查看和管理进程、存储、缓存和备份。它不依赖 ROS，也没有机器人安全互锁。

## 功能

- 查看系统进程、搜索指定进程、调整进程优先级。
- 以 `SIGTERM` 或 `SIGKILL` 终止进程。
- 查看磁盘分区使用情况和分析目录大小。
- 清理缓存文件。
- 创建目录压缩备份。
- 记录进程管理和存储操作日志。

主要脚本：

| 文件 | 用途 |
| --- | --- |
| `system_manager.py` | 交互式系统管理程序 |
| `quick_start.py` | 快速查看常用系统信息 |
| `install.sh` | 安装依赖并配置命令别名 |

## 安装

在本目录执行：

```bash
chmod +x install.sh
./install.sh
```

安装完成后可使用：

```bash
sysmgr
sysmgr-sudo
```

也可直接运行：

```bash
python3 system_manager.py
sudo python3 system_manager.py
python3 quick_start.py
```

如需手动安装依赖：

```bash
sudo apt update
sudo apt install python3-tabulate python3-psutil
```

## 常用方式

快速查看系统状态：

```bash
python3 quick_start.py
```

普通权限启动管理器：

```bash
sysmgr
```

需要调整受限进程优先级、清理系统目录或执行系统级操作时：

```bash
sysmgr-sudo
```

## 日志

默认日志目录：

```text
~/.system_manager/logs
```

可在启动前通过 `SYSTEM_MANAGER_LOG_DIR` 指定其他日志目录。

## 注意事项

- 终止进程前必须核对 PID、命令行和所属用户，尤其是 `ros2`、控制器、EtherCAT 或守护进程。
- 优先使用 `SIGTERM`；`SIGKILL` 会跳过进程清理逻辑，只应在确认进程无法正常退出时使用。
- 清理缓存和创建备份前确认目标目录和剩余磁盘空间。
- 本工具不会自动停机或禁用电机。机器人运行期间，避免通过此工具终止控制链路相关进程。
