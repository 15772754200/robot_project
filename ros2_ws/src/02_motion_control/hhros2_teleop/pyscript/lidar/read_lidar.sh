#!/bin/bash
UNIQUE_ID=$1
PATH_INSTALL=$2

if [ "${CONDA_DEFAULT_ENV:-}" != "rosenv" ] || \
   [ "$(command -v ros2 2>/dev/null)" != "${CONDA_PREFIX:-}/bin/ros2" ]; then
    echo "错误：请先运行 rosenv，使用本项目唯一的 ROS 2 环境。"
    exit 1
fi

# 使用日期格式生成日志文件名
TIMESTAMP=$(date +%F_%H-%M-%S)  # YYYY-MM-DD_HH-MM-SS

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 获取项目根目录
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"  # 定位到 ROBOT_ROS2 目录

echo "🔍 调试信息:"
echo "  脚本目录: $SCRIPT_DIR"
echo "  项目根目录: $PROJECT_ROOT"

# 日志目录设置 - 使用绝对路径
LOG_DIR="$PROJECT_ROOT/run_logs/lidar"
echo "  日志目录: $LOG_DIR"

# 创建日志目录（如果不存在）
mkdir -p "$LOG_DIR"
# 检查日志目录是否创建成功
if [ ! -d "$LOG_DIR" ]; then
    echo "❌ 无法创建日志目录: $LOG_DIR"
    exit 1
fi

# 定义日志文件路径
LOG_FILE="$LOG_DIR/lidar_$TIMESTAMP.log"

echo "✅ 日志文件: $LOG_FILE"

# 测试日志文件是否可写
if ! touch "$LOG_FILE" 2>/dev/null; then
    echo "❌ 无法创建日志文件，使用备用路径"
    LOG_FILE="/tmp/lidar_$TIMESTAMP.log"
fi

RUN_CMD="ros2 launch rslidar_sdk start.py"
KILL_SCRIPT="$SCRIPT_DIR/kill_terminal.py"

FULL_CMD="$RUN_CMD"

# ===== 日志增强部分 =====
echo "----------------------------------------" >> "$LOG_FILE"
echo "启动时间: $(date)" >> "$LOG_FILE"
echo "启动命令: $FULL_CMD" >> "$LOG_FILE"
# 写入实际参数
echo "启用 lidar:" >> "$LOG_FILE"
echo "----------------------------------------" >> "$LOG_FILE"
# ===== 日志增强部分结束 =====

# Ctrl+C 优雅退出
# 优雅退出函数
graceful_exit() {
    echo -e "\n👋 正在优雅退出相机..."
    echo "停止时间: $(date)" >> "$LOG_FILE"
    
    # 调用 kill_terminal.py 脚本
    if [[ -f "$KILL_SCRIPT" ]]; then
        echo "执行终止脚本: $KILL_SCRIPT"
        python3  $PATH_INSTALL/pyscript/camera/kill_terminal.py $UNIQUE_ID
    else
        echo "警告: 未找到终止脚本 $KILL_SCRIPT"
        # 备用方案：直接杀死相关进程
        pkill -f "ros2 launch rslidar_sdk"
    fi
    exit 0
}
# trap graceful_exit SIGINT SIGTERM SIGTSTP
echo "运行命令:"
echo "$FULL_CMD"
echo -e "按 Ctrl + C 退出\n"
# 在后台启动 ROS2 并获取进程ID
eval $FULL_CMD &
ROS_PID=$!

# 等待后台进程结束
wait $ROS_PID
