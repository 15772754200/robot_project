#!/bin/bash
# 启动 LiDAR 雷达脚本（由 enlid 调用）
# 功能：启动 RSLiDAR 雷达

UNIQUE_ID=$1
PATH_INSTALL=$2

if [ "${CONDA_DEFAULT_ENV:-}" != "rosenv" ] || \
   [ "$(command -v ros2 2>/dev/null)" != "${CONDA_PREFIX:-}/bin/ros2" ]; then
    echo "错误：请先运行 rosenv，使用本项目唯一的 ROS 2 环境。"
    exit 1
fi

# ---------- 路径配置 ----------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 从当前目录向上查找项目根目录（包含 ros2_ws 和 external_tools）
find_project_root() {
    local current="$1"
    local max_depth=15
    local count=0
    while [ "$current" != "/" ] && [ $count -lt $max_depth ]; do
        if [ -d "$current/ros2_ws" ] && [ -d "$current/external_tools" ]; then
            echo "$current"
            return 0
        fi
        current="$(dirname "$current")"
        count=$((count + 1))
    done
    return 1
}

# 尝试从多个位置查找项目根目录
PROJECT_ROOT=""
for try_dir in "$SCRIPT_DIR" "$PATH_INSTALL" "$(pwd)"; do
    if [ -n "$try_dir" ]; then
        PROJECT_ROOT=$(find_project_root "$try_dir")
        if [ -n "$PROJECT_ROOT" ]; then
            break
        fi
    fi
done

if [ -z "$PROJECT_ROOT" ]; then
    echo "❌ 错误：未找到项目根目录（包含 ros2_ws 和 external_tools）"
    echo "  当前脚本目录: $SCRIPT_DIR"
    echo "  PATH_INSTALL: $PATH_INSTALL"
    echo ""
    echo "请手动设置项目根目录："
    echo "  例如: export PROJECT_ROOT=/home/niic/yidong_robot_project"
    echo "  然后重新运行 enlid"
    exit 1
fi

echo "🔍 调试信息:"
echo "  脚本目录: $SCRIPT_DIR"
echo "  项目根目录: $PROJECT_ROOT"

# ---------- LiDAR 包路径（直接使用 hhros2_radar 工作空间） ----------
LIDAR_WS="$PROJECT_ROOT/external_tools/hhros2_radar"
LIDAR_PKG_SRC="$LIDAR_WS/src/rslidar_sdk"
LIDAR_MSG_SRC="$LIDAR_WS/src/rslidar_msg"

echo "🔍 LiDAR 工作空间: $LIDAR_WS"
echo "🔍 rslidar_sdk 源码: $LIDAR_PKG_SRC"
echo "🔍 rslidar_msg 源码: $LIDAR_MSG_SRC"

# 检查 rslidar_sdk 是否存在
if [ ! -d "$LIDAR_PKG_SRC" ]; then
    echo "❌ 错误：找不到 rslidar_sdk 源码目录"
    echo "  期望路径: $LIDAR_PKG_SRC"
    exit 1
fi

# 检查 package.xml 是否存在
if [ ! -f "$LIDAR_PKG_SRC/package.xml" ]; then
    echo "❌ 错误：$LIDAR_PKG_SRC/package.xml 不存在，不是有效的 ROS2 包"
    exit 1
fi

# 检查 rslidar_msg 是否存在
if [ ! -d "$LIDAR_MSG_SRC" ]; then
    echo "⚠️  警告：找不到 rslidar_msg 源码目录"
    echo "  期望路径: $LIDAR_MSG_SRC"
    echo "  编译 rslidar_sdk 需要 rslidar_msg 依赖"
    echo ""
    echo "请确保 rslidar_msg 位于: $LIDAR_MSG_SRC"
    exit 1
fi

echo "✅ LiDAR 包有效"

# ---------- 检查并安装依赖 ----------
echo ""
echo "🔍 检查并安装依赖（根据 rslidar_sdk README）..."

# 定义依赖检查函数
check_and_install() {
    local pkg_name="$1"
    local dev_pkg="$2"
    local check_cmd="$3"
    
    echo "  📦 检查 $pkg_name..."
    if eval "$check_cmd" &> /dev/null; then
        echo "    ✅ $pkg_name 已安装"
        return 0
    else
        echo "    📥 安装 $pkg_name..."
        sudo apt update
        sudo apt install -y "$dev_pkg"
        if [ $? -eq 0 ]; then
            echo "    ✅ $pkg_name 安装完成"
            return 0
        else
            echo "    ❌ $pkg_name 安装失败"
            return 1
        fi
    fi
}

# 1. 安装 libyaml-cpp-dev（必需）
check_and_install "libyaml-cpp-dev" "libyaml-cpp-dev" "dpkg -s libyaml-cpp-dev"
if [ $? -ne 0 ]; then
    echo "❌ 依赖安装失败，请手动安装: sudo apt install -y libyaml-cpp-dev"
    exit 1
fi

# 2. 安装 libpcap-dev（必需）
check_and_install "libpcap-dev" "libpcap-dev" "dpkg -s libpcap-dev"
if [ $? -ne 0 ]; then
    echo "❌ 依赖安装失败，请手动安装: sudo apt install -y libpcap-dev"
    exit 1
fi

# 3. 安装 libpcap0.8-dev（可选但推荐）
check_and_install "libpcap0.8-dev" "libpcap0.8-dev" "dpkg -s libpcap0.8-dev" 2>/dev/null || true

# 4. 使用 rosdep 安装 ROS 依赖
echo "  📦 使用 rosdep 安装 ROS 依赖..."
rosdep install --from-paths src --ignore-src -r -y 2>/dev/null || echo "    ⚠️  部分依赖可能已安装"

echo "✅ 依赖检查完成"

# ---------- 检查是否已编译 ----------
COMPILED=false

# 检查是否已编译（检查两个包）
if [ -d "$LIDAR_WS/install/rslidar_sdk" ] && [ -d "$LIDAR_WS/install/rslidar_msg" ] && [ -f "$LIDAR_WS/install/setup.bash" ]; then
    echo "✅ rslidar_sdk 和 rslidar_msg 已编译"
    COMPILED=true
else
    echo ""
    echo "⚠️  rslidar_sdk 或 rslidar_msg 尚未编译，开始自动编译..."
    
    cd "$LIDAR_WS" || exit 1
    
    if ! command -v colcon &> /dev/null; then
        echo "❌ 未找到 colcon 命令，请安装: sudo apt install python3-colcon-common-extensions"
        exit 1
    fi
    
    # 开始编译
    echo "🔨 开始编译 rslidar_msg 和 rslidar_sdk..."
    # 先编译 rslidar_msg（依赖包）
    echo "  📦 编译 rslidar_msg..."
    colcon build \
        --packages-select \
        rslidar_msg \
        --symlink-install \
        --event-handlers console_direct+ \
        --cmake-args \
        -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -ne 0 ]; then
        echo "❌ 编译 rslidar_msg 失败！"
        echo ""
        echo "请尝试手动编译："
        echo "  cd $LIDAR_WS"
        echo "  请先运行 rosenv"
        echo "  colcon build --packages-select rslidar_msg --symlink-install"
        exit 1
    fi
    echo "  ✅ rslidar_msg 编译成功"
    
    # 再编译 rslidar_sdk（主包）
    echo "  📦 编译 rslidar_sdk..."
    colcon build \
        --packages-select \
        rslidar_sdk \
        --symlink-install \
        --event-handlers console_direct+ \
        --cmake-args \
        -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -eq 0 ] && [ -d "$LIDAR_WS/install/rslidar_sdk" ]; then
        echo "✅ 编译成功！"
        COMPILED=true
    else
        echo "❌ 编译 rslidar_sdk 失败！"
        echo ""
        echo "请尝试手动编译："
        echo "  cd $LIDAR_WS"
        echo "  请先运行 rosenv"
        echo "  colcon build --packages-select rslidar_msg rslidar_sdk --symlink-install"
        exit 1
    fi
fi

# ---------- 日志目录设置 ----------
LOG_DIR="$PROJECT_ROOT/ros2_ws/run_logs/lidar"
mkdir -p "$LOG_DIR"
if [ ! -d "$LOG_DIR" ]; then
    echo "❌ 无法创建日志目录: $LOG_DIR"
    exit 1
fi

TIMESTAMP=$(date +%F_%H-%M-%S)
LOG_FILE="$LOG_DIR/lidar_$TIMESTAMP.log"
echo "✅ 日志文件: $LOG_FILE"

# ---------- 配置参数 ----------
RUN_CMD="ros2 launch rslidar_sdk start.launch.py"
KILL_SCRIPT="$SCRIPT_DIR/kill_terminal.py"

# 检查是否存在 start.launch.py，如果不存在则尝试其他名称
if [ ! -f "$LIDAR_PKG_SRC/launch/start.launch.py" ]; then
    echo "⚠️  未找到 start.launch.py，尝试使用 start.py..."
    if [ -f "$LIDAR_PKG_SRC/launch/start.py" ]; then
        RUN_CMD="ros2 launch rslidar_sdk start.py"
        echo "  使用 start.py"
    else
        echo "⚠️  未找到 launch 文件，请检查 rslidar_sdk/launch/ 目录"
        # 列出可用的 launch 文件
        if [ -d "$LIDAR_PKG_SRC/launch" ]; then
            echo "  可用的 launch 文件："
            ls -la "$LIDAR_PKG_SRC/launch/" | grep -E "\.(py|xml)$" || echo "  无"
        fi
    fi
fi

FULL_CMD="$RUN_CMD"

# ===== 日志 =====
echo "----------------------------------------" >> "$LOG_FILE"
echo "启动时间: $(date)" >> "$LOG_FILE"
echo "启动命令: $FULL_CMD" >> "$LOG_FILE"
echo "工作空间: $LIDAR_WS" >> "$LOG_FILE"
echo "包路径: $LIDAR_PKG_SRC" >> "$LOG_FILE"
echo "----------------------------------------" >> "$LOG_FILE"

# ===== 优雅退出 =====
graceful_exit() {
    echo -e "\n👋 正在优雅退出雷达..."
    echo "停止时间: $(date)" >> "$LOG_FILE"
    
    if [[ -f "$KILL_SCRIPT" ]]; then
        echo "执行终止脚本: $KILL_SCRIPT"
        python3 "$PATH_INSTALL/pyscript/lidar/kill_terminal.py" "$UNIQUE_ID" 2>/dev/null || \
        python3 "$PATH_INSTALL/pyscript/camera/kill_terminal.py" "$UNIQUE_ID" 2>/dev/null || \
        echo "⚠️  未找到 kill_terminal.py"
    else
        echo "警告: 未找到终止脚本 $KILL_SCRIPT"
        pkill -f "ros2 launch rslidar_sdk" 2>/dev/null || true
    fi
    exit 0
}
trap graceful_exit SIGINT SIGTERM SIGTSTP

# ===== 启动 =====
echo ""
echo "运行命令:"
echo "$FULL_CMD"
echo -e "按 Ctrl + C 退出\n"

# ROS 基础环境已经由项目 rosenv 提供。
echo "  ✅ 使用项目 rosenv"

if [ "$COMPILED" = true ]; then
    source "$LIDAR_WS/install/setup.bash"
    echo "  ✅ LiDAR 工作空间 sourced: $LIDAR_WS"
    
    # 验证 rslidar_sdk 是否可找到
    echo "🔍 验证 rslidar_sdk 包..."
    if ros2 pkg list | grep -q "rslidar_sdk"; then
        echo "  ✅ rslidar_sdk 包已找到"
    else
        echo "  ❌ rslidar_sdk 包未找到！"
        echo "  请检查: $LIDAR_WS/install/rslidar_sdk"
        exit 1
    fi
else
    echo "  ⚠️  使用系统环境（可能未编译）"
fi

# 启动
echo ""
eval $FULL_CMD &
ROS_PID=$!

wait $ROS_PID
