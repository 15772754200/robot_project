#!/bin/bash
# 启动 RGBD 相机脚本（由 endcam 调用）

UNIQUE_ID=$1
PATH_INSTALL=$2

if [ "${CONDA_DEFAULT_ENV:-}" != "rosenv" ] || \
   [ "$(command -v ros2 2>/dev/null)" != "${CONDA_PREFIX:-}/bin/ros2" ]; then
    echo "错误：请先运行 rosenv，使用本项目唯一的 ROS 2 环境。"
    exit 1
fi

# ---------- 路径配置 ----------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 从当前目录向上查找项目根目录
find_project_root() {
    local current="$1"
    while [ "$current" != "/" ]; do
        if [ -d "$current/ros2_ws" ] && [ -d "$current/external_tools" ]; then
            echo "$current"
            return 0
        fi
        current="$(dirname "$current")"
    done
    return 1
}

PROJECT_ROOT=$(find_project_root "$SCRIPT_DIR")
if [ -z "$PROJECT_ROOT" ]; then
    echo "错误：未找到项目根目录（包含 ros2_ws 和 external_tools）"
    exit 1
fi

echo "🔍 调试信息:"
echo "  脚本目录: $SCRIPT_DIR"
echo "  项目根目录: $PROJECT_ROOT"

# ---------- 查找 realsense 工作空间 ----------
REALSENSE_WS=""
FOUND_TYPE=""

# 可能的 realsense 路径
REALSENSE_POSSIBLE_PATHS=(
    "$PROJECT_ROOT/external_tools/ros2_ws"
    "$PROJECT_ROOT/external_tools/hhros2_realsense/ros2_ws"
    "$PROJECT_ROOT/external_tools/realsense/ros2_ws"
    "$PROJECT_ROOT/ros2_ws"
)

echo "🔍 搜索 RealSense 工作空间..."

for ws in "${REALSENSE_POSSIBLE_PATHS[@]}"; do
    echo "  检查: $ws"
    
    # 检查1: 是否已编译且有完整的可执行文件
    if [ -f "$ws/install/setup.bash" ] && [ -d "$ws/install/realsense2_camera" ]; then
        # 验证编译完整性：检查关键库文件是否存在
        if [ -f "$ws/install/realsense2_camera/lib/librealsense2_camera.so" ] || \
           [ -f "$ws/install/realsense2_camera/lib/librealsense2_camera_node.so" ] || \
           [ -d "$ws/install/realsense2_camera/share/realsense2_camera" ]; then
            REALSENSE_WS="$ws"
            FOUND_TYPE="compiled"
            echo "✅ 找到已编译的 RealSense 工作空间: $REALSENSE_WS"
            break
        else
            echo "  ⚠️  检测到编译目录但不完整（缺少关键库文件），将重新编译"
            # 删除不完整的编译产物
            rm -rf "$ws/install/realsense2_camera" 2>/dev/null
            rm -rf "$ws/build/realsense2_camera" 2>/dev/null
            REALSENSE_WS="$ws"
            FOUND_TYPE="incomplete"
            break
        fi
    fi
    
    # 检查2: 是否有源码（src/realsense-ros 或 src/realsense2_camera）
    if [ -d "$ws/src/realsense-ros" ] || [ -d "$ws/src/realsense2_camera" ]; then
        if [ "$FOUND_TYPE" != "incomplete" ]; then
            REALSENSE_WS="$ws"
            FOUND_TYPE="source"
            echo "✅ 找到 RealSense 源码工作空间: $REALSENSE_WS"
        fi
        break
    fi
done

# 如果还是没找到，在 external_tools 下任意子目录中查找
if [ -z "$REALSENSE_WS" ]; then
    for ws in "$PROJECT_ROOT/external_tools"/*/ros2_ws; do
        if [ -f "$ws/install/setup.bash" ] && [ -d "$ws/install/realsense2_camera" ]; then
            # 验证编译完整性
            if [ -f "$ws/install/realsense2_camera/lib/librealsense2_camera.so" ] || \
               [ -f "$ws/install/realsense2_camera/lib/librealsense2_camera_node.so" ] || \
               [ -d "$ws/install/realsense2_camera/share/realsense2_camera" ]; then
                REALSENSE_WS="$ws"
                FOUND_TYPE="compiled"
                echo "✅ 找到已编译的 RealSense 工作空间: $REALSENSE_WS"
                break
            else
                echo "  ⚠️  检测到编译目录但不完整，将重新编译"
                rm -rf "$ws/install/realsense2_camera" 2>/dev/null
                rm -rf "$ws/build/realsense2_camera" 2>/dev/null
                REALSENSE_WS="$ws"
                FOUND_TYPE="incomplete"
                break
            fi
        fi
        if [ -d "$ws/src/realsense-ros" ] || [ -d "$ws/src/realsense2_camera" ]; then
            if [ "$FOUND_TYPE" != "incomplete" ]; then
                REALSENSE_WS="$ws"
                FOUND_TYPE="source"
                echo "✅ 找到 RealSense 源码工作空间: $REALSENSE_WS"
                break
            fi
        fi
    done
fi

if [ -z "$REALSENSE_WS" ]; then
    echo "❌ 未找到 RealSense 工作空间"
    echo "请确保 realsense2_camera 已安装，可能的路径："
    echo "  $PROJECT_ROOT/external_tools/ros2_ws"
    echo "  $PROJECT_ROOT/external_tools/hhros2_realsense/ros2_ws"
    echo "  $PROJECT_ROOT/external_tools/realsense/ros2_ws"
    exit 1
fi

# ---------- 如果是源码或不完整编译，执行编译 ----------
if [ "$FOUND_TYPE" = "source" ] || [ "$FOUND_TYPE" = "incomplete" ]; then
    echo ""
    echo "⚠️  检测到 RealSense 需要编译（源码: $FOUND_TYPE）"
    echo "开始自动编译..."
    echo "  工作空间: $REALSENSE_WS"
    
    cd "$REALSENSE_WS" || exit 1
    
    # 检查是否有 colcon
    if ! command -v colcon &> /dev/null; then
        echo "❌ 未找到 colcon 命令，请安装: sudo apt install python3-colcon-common-extensions"
        exit 1
    fi
    
    # ---------- 检查并安装依赖 ----------
    echo "🔍 检查并安装依赖..."
    
    # 安装 diagnostic_updater
    if ! ros2 pkg list 2>/dev/null | grep -q "diagnostic_updater"; then
        echo "  📦 安装 diagnostic_updater..."
        sudo apt update
        sudo apt install -y ros-humble-diagnostic-updater
        if [ $? -ne 0 ]; then
            echo "  ❌ 安装 diagnostic_updater 失败"
            exit 1
        fi
        echo "  ✅ diagnostic_updater 安装完成"
    else
        echo "  ✅ diagnostic_updater 已安装"
    fi
    
    # 安装其他可能的依赖
    echo "  📦 安装 realsense2_camera 依赖..."
    rosdep install --from-paths src --ignore-src -r -y 2>/dev/null || echo "  ⚠️  部分依赖可能缺失，继续尝试编译..."
    
    # 安装 librealsense2 开发库
    echo "  📦 安装 librealsense2 开发库..."
    sudo apt install -y librealsense2-dev librealsense2-dkms 2>/dev/null || echo "  ⚠️  librealsense2 可能已安装"
    
    # 删除可能残留的不完整编译文件
    echo "  🧹 清理可能残留的编译文件..."
    rm -rf build/realsense2_camera 2>/dev/null
    rm -rf install/realsense2_camera 2>/dev/null
    rm -rf log/realsense2_camera 2>/dev/null
    
    # 开始编译
    echo "🔨 开始编译 realsense2_camera..."
    # 先编译 msgs 包（基础依赖）
    echo "  📦 编译 realsense2_camera_msgs..."
    colcon build \
        --packages-select \
        realsense2_camera_msgs \
        --symlink-install \
        --event-handlers console_direct+ \
        --cmake-args \
        -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -ne 0 ]; then
        echo "❌ 编译 realsense2_camera_msgs 失败"
        exit 1
    fi
    
    # 编译 description 包
    echo "  📦 编译 realsense2_description..."
    colcon build \
        --packages-select \
        realsense2_description \
        --symlink-install \
        --event-handlers console_direct+ \
        --cmake-args \
        -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -ne 0 ]; then
        echo "❌ 编译 realsense2_description 失败"
        exit 1
    fi
    
    # 最后编译 realsense2_camera
    echo "  📦 编译 realsense2_camera..."
    colcon build \
        --packages-select \
        realsense2_camera \
        --symlink-install \
        --event-handlers console_direct+ \
        --cmake-args \
        -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -eq 0 ]; then
        # 验证编译结果
        if [ -f "install/realsense2_camera/lib/librealsense2_camera.so" ] || \
           [ -f "install/realsense2_camera/lib/librealsense2_camera_node.so" ] || \
           [ -d "install/realsense2_camera/share/realsense2_camera" ]; then
            echo "✅ 编译成功！"
            FOUND_TYPE="compiled"
        else
            echo "❌ 编译完成但验证失败（缺少关键库文件）"
            echo "请手动检查编译结果"
            exit 1
        fi
    else
        echo "❌ 编译失败！"
        echo ""
        echo "请尝试手动编译："
        echo "  cd $REALSENSE_WS"
        echo "  请先运行 rosenv"
        echo "  rosdep install --from-paths src --ignore-src -r -y"
        echo "  colcon build --packages-select realsense2_camera realsense2_camera_msgs realsense2_description --symlink-install"
        exit 1
    fi
fi

# ---------- 日志目录设置 ----------
LOG_DIR="$PROJECT_ROOT/ros2_ws/run_logs/camera"
mkdir -p "$LOG_DIR"
if [ ! -d "$LOG_DIR" ]; then
    echo "❌ 无法创建日志目录: $LOG_DIR"
    exit 1
fi

TIMESTAMP=$(date +%F_%H-%M-%S)
LOG_FILE="$LOG_DIR/realsense_start_RGBD_$TIMESTAMP.log"
echo "✅ 日志文件: $LOG_FILE"

# ---------- 配置参数 ----------
RUN_CMD="ros2 launch realsense2_camera rs_launch.py"
KILL_SCRIPT="$SCRIPT_DIR/kill_terminal.py"

# Depth + Infra 参数全集
declare -A DEPTH_FPS
DEPTH_FPS["1280x720"]="6 15 30"
DEPTH_FPS["848x480"]="6 15 30 60 90"
DEPTH_FPS["848x100"]="100 300"
DEPTH_FPS["640x480"]="6 15 30 60 90"
DEPTH_FPS["640x360"]="6 15 30 60 90"
DEPTH_FPS["480x270"]="6 15 30 60 90"
DEPTH_FPS["424x240"]="6 15 30 60 90"
DEPTH_FPS["256x144"]="90 300"

echo ""
echo "启动 RGBD 相机配置脚本"
echo -e "\n============== 可用 Depth 模式 =============="
for res in $(printf "%s\n" "${!DEPTH_FPS[@]}" | sort); do
    echo "Depth: $res @ ${DEPTH_FPS[$res]}"
done
echo "============================================"

CMD_PARAMS=""
ENABLE_DEPTH=true

# Depth 配置
echo -e "\n您选择启用 Depth + Infra 相机"
read -p "请输入 Depth 分辨率 (如 848x480): " depth_res
depth_res=$(echo "$depth_res" | tr -d ' ')
read -p "请输入 Depth 帧率(Hz): " depth_fps

if [[ -z ${DEPTH_FPS[$depth_res]} ]]; then
    echo "❌ 不支持的 Depth 分辨率！"
    exit 1
fi
if [[ ! " ${DEPTH_FPS[$depth_res]} " =~ " ${depth_fps} " ]]; then
    echo "❌ ${depth_res} 不支持 FPS=${depth_fps}"
    exit 1
fi

CMD_PARAMS="$CMD_PARAMS enable_depth:=true enable_infra1:=true enable_infra2:=true \
    depth_module.depth_profile:=${depth_res}x${depth_fps} \
    depth_module.infra_profile:=${depth_res}x${depth_fps}"

# 点云
read -p $'\n是否启用点云发布 (y/n)? ' enable_pc
if [[ "$enable_pc" == "y" ]]; then
    CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=true"
else
    CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=false"
fi

echo -e "\n✅ 参数检查通过，准备启动相机...\n"

FULL_CMD="$RUN_CMD $CMD_PARAMS"

# ===== 日志 =====
echo "----------------------------------------" >> "$LOG_FILE"
echo "启动时间: $(date)" >> "$LOG_FILE"
echo "启动命令: $FULL_CMD" >> "$LOG_FILE"
echo "工作空间: $REALSENSE_WS" >> "$LOG_FILE"

DEVICE_INFO=$(rs-enumerate-devices --compact 2>/dev/null)
if [[ -z "$DEVICE_INFO" ]]; then
    DEVICE_INFO="未检测到设备或未安装 rs-enumerate-devices"
fi
echo "设备信息:" >> "$LOG_FILE"
echo "$DEVICE_INFO" >> "$LOG_FILE"
echo "Depth 参数: ${depth_res} @ ${depth_fps} FPS" >> "$LOG_FILE"
echo "启用点云: $([[ $enable_pc == "y" ]] && echo Yes || echo No)" >> "$LOG_FILE"
echo "----------------------------------------" >> "$LOG_FILE"

# ===== 优雅退出 =====
graceful_exit() {
    echo -e "\n👋 正在优雅退出相机..."
    echo "停止时间: $(date)" >> "$LOG_FILE"
    
    if [[ -f "$KILL_SCRIPT" ]]; then
        echo "执行终止脚本: $KILL_SCRIPT"
        python3 "$PATH_INSTALL/pyscript/camera/kill_terminal.py" "$UNIQUE_ID"
    else
        echo "警告: 未找到终止脚本 $KILL_SCRIPT"
        pkill -f "ros2 launch realsense2_camera" 2>/dev/null || true
    fi
    exit 0
}
trap graceful_exit SIGINT SIGTERM SIGTSTP

# ===== 启动 =====
echo "运行命令:"
echo "$FULL_CMD"
echo -e "按 Ctrl + C 退出\n"

# ROS 基础环境已经由项目 rosenv 提供。
echo "  ✅ 使用项目 rosenv"

if [ "$FOUND_TYPE" = "compiled" ]; then
    source "$REALSENSE_WS/install/setup.bash"
    echo "  ✅ RealSense 工作空间 sourced: $REALSENSE_WS"
    
    # 验证 realsense2_camera 是否可找到
    echo "🔍 验证 realsense2_camera 包..."
    if ros2 pkg list | grep -q "realsense2_camera"; then
        echo "  ✅ realsense2_camera 包已找到"
    else
        echo "  ❌ realsense2_camera 包未找到！"
        echo "  请检查: $REALSENSE_WS/install/realsense2_camera"
        exit 1
    fi
else
    echo "  ⚠️  使用系统环境"
fi

# 启动
echo ""
eval $FULL_CMD &
ROS_PID=$!

wait $ROS_PID
