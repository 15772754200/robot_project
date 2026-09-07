#!/bin/bash
# -*- coding: utf-8 -*-

# RealSense D435i 启动脚本：分辨率 + 帧率编号选择版

set +e

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

echo "🔍 项目根目录: $PROJECT_ROOT"

# ---------- Source 环境 ----------
echo "🔍 Source 环境..."

# 1. ROS2 Humble
if [ -f "/opt/ros/humble/setup.bash" ]; then
    source /opt/ros/humble/setup.bash
    echo "  ✅ ROS2 Humble sourced"
else
    echo "  ❌ 未找到 ROS2 Humble"
    exit 1
fi

# 2. 查找 Realsense 工作空间
REALSENSE_WS=""
for ws in \
    "$PROJECT_ROOT/external_tools/hhros2_realsense/ros2_ws" \
    "$PROJECT_ROOT/external_tools/ros2_ws" \
    "$PROJECT_ROOT/ros2_ws"
do
    if [ -f "$ws/install/setup.bash" ] && [ -d "$ws/install/realsense2_camera" ]; then
        REALSENSE_WS="$ws"
        break
    fi
done

if [ -n "$REALSENSE_WS" ]; then
    source "$REALSENSE_WS/install/setup.bash"
    echo "  ✅ Realsense 工作空间 sourced: $REALSENSE_WS"
else
    echo "  ⚠️  未找到 Realsense 工作空间，尝试使用系统环境..."
fi

# 验证 realsense2_camera 是否可用
if ! ros2 pkg list 2>/dev/null | grep -q "realsense2_camera"; then
    echo "  ❌ 未找到 realsense2_camera 包！"
    echo "  请确认 Realsense 工作空间已正确编译"
    exit 1
else
    echo "  ✅ realsense2_camera 包已找到"
fi

RUN_LOG_ROOT="/home/niic/robot_ros2_deploy_4_10_humanoid/run_logs/realsense"
mkdir -p "$RUN_LOG_ROOT"

TIMESTAMP=$(date +%F_%H-%M-%S)
SESSION_DIR="$RUN_LOG_ROOT/realsense_start_$TIMESTAMP"
mkdir -p "$SESSION_DIR"
LOG_FILE="$SESSION_DIR/realsense_start_$TIMESTAMP.log"
RUN_CMD="ros2 launch realsense2_camera rs_launch.py"

RGB_MODES=(
    "1920x1080@6"
    "1920x1080@15"
    "1920x1080@30"
    "1280x720@6"
    "1280x720@15"
    "1280x720@30"
    "960x540@6"
    "960x540@15"
    "960x540@30"
    "848x480@6"
    "848x480@15"
    "848x480@30"
    "848x480@60"
    "640x480@6"
    "640x480@15"
    "640x480@30"
    "640x480@60"
    "640x360@6"
    "640x360@15"
    "640x360@30"
    "640x360@60"
    "424x240@6"
    "424x240@15"
    "424x240@30"
    "424x240@60"
    "320x240@6"
    "320x240@30"
    "320x240@60"
    "320x180@6"
    "320x180@30"
    "320x180@60"
)

DEPTH_MODES=(
    "1280x720@6"
    "1280x720@15"
    "1280x720@30"
    "848x480@6"
    "848x480@15"
    "848x480@30"
    "848x480@60"
    "848x480@90"
    "848x100@100"
    "848x100@300"
    "640x480@6"
    "640x480@15"
    "640x480@30"
    "640x480@60"
    "640x480@90"
    "640x360@6"
    "640x360@15"
    "640x360@30"
    "640x360@60"
    "640x360@90"
    "480x270@6"
    "480x270@15"
    "480x270@30"
    "480x270@60"
    "480x270@90"
    "424x240@6"
    "424x240@15"
    "424x240@30"
    "424x240@60"
    "424x240@90"
    "256x144@90"
    "256x144@300"
)

write_log_line() {
    echo "$1" >> "$LOG_FILE"
}

check_ros2_env() {
    if ! command -v ros2 >/dev/null 2>&1; then
        echo "❌ 未找到 ros2 命令。请先 source ROS2 环境。"
        echo "示例："
        echo "  source /opt/ros/<你的ROS2版本>/setup.bash"
        echo "  source ~/你的工作空间/install/setup.bash"
        write_log_line "错误: 未找到 ros2 命令"
        exit 1
    fi
}

print_header() {
    echo "============================================================"
    echo "RealSense D435i 启动脚本 - 编号选择版"
    echo "============================================================"
    echo "日志文件: $LOG_FILE"
    echo
}

print_sensor_menu() {
    echo "请选择要启用的相机："
    echo "  1) 仅启用 RGB"
    echo "  2) 仅启用 Depth + Infra"
    echo "  3) 同时启用 RGB + Depth + Infra"
    echo
}

print_modes() {
    local title="$1"
    shift
    local modes=("$@")
    local i=1
    local mode=""
    local res=""
    local fps=""

    echo
    echo "================ ${title} 可选模式 ================"
    for mode in "${modes[@]}"; do
        res="${mode%@*}"
        fps="${mode#*@}"
        printf "  %2d) %-10s @ %3s FPS\n" "$i" "$res" "$fps"
        i=$((i + 1))
    done
    echo "=================================================="
}

is_positive_integer() {
    [[ "$1" =~ ^[0-9]+$ ]] && [[ "$1" -ge 1 ]]
}

choose_rgb_mode() {
    local choice=""
    local count=${#RGB_MODES[@]}
    local selected=""

    while true; do
        print_modes "RGB" "${RGB_MODES[@]}"
        read -p "请选择 RGB 模式编号(1-${count})：" choice
        choice=$(echo "$choice" | tr -d ' ')

        if ! is_positive_integer "$choice"; then
            echo "❌ 输入非法：请输入 1-${count} 之间的数字。"
            continue
        fi
        if [[ "$choice" -lt 1 || "$choice" -gt "$count" ]]; then
            echo "❌ 编号超出范围：请输入 1-${count}。"
            continue
        fi

        selected="${RGB_MODES[$((choice - 1))]}"
        rgb_res="${selected%@*}"
        rgb_fps="${selected#*@}"
        rgb_choice="$choice"
        break
    done
}

choose_depth_mode() {
    local choice=""
    local count=${#DEPTH_MODES[@]}
    local selected=""

    while true; do
        print_modes "Depth + Infra" "${DEPTH_MODES[@]}"
        read -p "请选择 Depth + Infra 模式编号(1-${count})：" choice
        choice=$(echo "$choice" | tr -d ' ')

        if ! is_positive_integer "$choice"; then
            echo "❌ 输入非法：请输入 1-${count} 之间的数字。"
            continue
        fi
        if [[ "$choice" -lt 1 || "$choice" -gt "$count" ]]; then
            echo "❌ 编号超出范围：请输入 1-${count}。"
            continue
        fi

        selected="${DEPTH_MODES[$((choice - 1))]}"
        depth_res="${selected%@*}"
        depth_fps="${selected#*@}"
        depth_choice="$choice"
        break
    done
}

ask_yes_no() {
    local prompt="$1"
    local ans=""
    while true; do
        read -p "$prompt" ans
        ans=$(echo "$ans" | tr 'A-Z' 'a-z' | tr -d ' ')
        case "$ans" in
            y|yes)
                echo "y"
                return 0
                ;;
            n|no)
                echo "n"
                return 0
                ;;
            *)
                echo "❌ 输入非法：请输入 y 或 n。"
                ;;
        esac
    done
}

print_header
check_ros2_env

CMD_PARAMS=""
ENABLE_RGB=false
ENABLE_DEPTH=false
rgb_res=""
rgb_fps=""
rgb_choice=""
depth_res=""
depth_fps=""
depth_choice=""
enable_pc="n"

while true; do
    print_sensor_menu
    read -p "请输入选项编号(1/2/3)：" sensor_choice
    sensor_choice=$(echo "$sensor_choice" | tr -d ' ')

    case "$sensor_choice" in
        1)
            ENABLE_RGB=true
            ENABLE_DEPTH=false
            break
            ;;
        2)
            ENABLE_RGB=false
            ENABLE_DEPTH=true
            break
            ;;
        3)
            ENABLE_RGB=true
            ENABLE_DEPTH=true
            break
            ;;
        *)
            echo "❌ 输入非法：请输入 1、2 或 3。"
            ;;
    esac
done

if [[ "$ENABLE_RGB" == true ]]; then
    echo
    echo "✅ 已选择启用 RGB 相机"
    choose_rgb_mode
    CMD_PARAMS="$CMD_PARAMS enable_color:=true rgb_camera.color_profile:=${rgb_res}x${rgb_fps}"
else
    CMD_PARAMS="$CMD_PARAMS enable_color:=false"
fi

if [[ "$ENABLE_DEPTH" == true ]]; then
    echo
    echo "✅ 已选择启用 Depth + Infra 相机"
    choose_depth_mode
    CMD_PARAMS="$CMD_PARAMS enable_depth:=true enable_infra1:=true enable_infra2:=true"
    CMD_PARAMS="$CMD_PARAMS depth_module.depth_profile:=${depth_res}x${depth_fps}"
    CMD_PARAMS="$CMD_PARAMS depth_module.infra_profile:=${depth_res}x${depth_fps}"

    enable_pc=$(ask_yes_no $'\n是否启用点云发布 (y/n)? ')
    if [[ "$enable_pc" == "y" ]]; then
        CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=true"
    else
        CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=false"
    fi
else
    CMD_PARAMS="$CMD_PARAMS enable_depth:=false enable_infra1:=false enable_infra2:=false pointcloud.enable:=false"
fi

FULL_CMD="$RUN_CMD $CMD_PARAMS"

echo
echo "================ 启动参数确认 ================"
echo "启用 RGB: $([[ "$ENABLE_RGB" == true ]] && echo Yes || echo No)"
if [[ "$ENABLE_RGB" == true ]]; then
    echo "RGB 模式编号: $rgb_choice"
    echo "RGB 参数: ${rgb_res} @ ${rgb_fps} FPS"
fi
echo "启用 Depth + Infra: $([[ "$ENABLE_DEPTH" == true ]] && echo Yes || echo No)"
if [[ "$ENABLE_DEPTH" == true ]]; then
    echo "Depth 模式编号: $depth_choice"
    echo "Depth 参数: ${depth_res} @ ${depth_fps} FPS"
    echo "Infra 参数: ${depth_res} @ ${depth_fps} FPS"
    echo "启用点云: $([[ "$enable_pc" == "y" ]] && echo Yes || echo No)"
fi
echo "=============================================="
echo

confirm=$(ask_yes_no "确认启动相机吗？(y/n): ")
if [[ "$confirm" != "y" ]]; then
    echo "⚠️ 已取消启动。"
    write_log_line "用户取消启动"
    exit 0
fi

echo
echo "✅ 参数检查通过，准备启动相机..."
echo

write_log_line "----------------------------------------"
write_log_line "启动时间: $(date)"
write_log_line "启动命令: $FULL_CMD"

DEVICE_INFO=$(rs-enumerate-devices --compact 2>/dev/null)
if [[ -z "$DEVICE_INFO" ]]; then
    DEVICE_INFO="未检测到设备或未安装 rs-enumerate-devices"
fi
write_log_line "设备信息:"
write_log_line "$DEVICE_INFO"

write_log_line "启用 RGB: $([[ "$ENABLE_RGB" == true ]] && echo Yes || echo No)"
if [[ "$ENABLE_RGB" == true ]]; then
    write_log_line "RGB 模式编号: $rgb_choice"
    write_log_line "RGB 参数: ${rgb_res} @ ${rgb_fps} FPS"
fi
write_log_line "启用 Depth + Infra: $([[ "$ENABLE_DEPTH" == true ]] && echo Yes || echo No)"
if [[ "$ENABLE_DEPTH" == true ]]; then
    write_log_line "Depth 模式编号: $depth_choice"
    write_log_line "Depth 参数: ${depth_res} @ ${depth_fps} FPS"
    write_log_line "Infra 参数: ${depth_res} @ ${depth_fps} FPS"
    write_log_line "启用点云: $([[ "$enable_pc" == "y" ]] && echo Yes || echo No)"
fi
write_log_line "----------------------------------------"

trap 'echo -e "\n👋 相机已停止"; echo "停止时间: $(date)" >> "$LOG_FILE"; exit 0' SIGINT

echo "运行命令:"
echo "$FULL_CMD"
echo
echo "按 Ctrl + C 退出"
echo

eval "$FULL_CMD"
