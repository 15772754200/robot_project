#!/bin/bash

# 使用日期格式生成日志文件名
TIMESTAMP=$(date +%F_%H-%M-%S)  # YYYY-MM-DD_HH-MM-SS


SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"  # 定位到 ROBOT_ROS2 目录

echo "🔍 调试信息:"
echo "  脚本目录: $SCRIPT_DIR"
echo "  项目根目录: $PROJECT_ROOT"

# 日志目录设置 - 使用绝对路径
LOG_DIR="$PROJECT_ROOT/run_logs/camera"
echo "  日志目录: $LOG_DIR"

mkdir -p "$LOG_DIR"
if [ ! -d "$LOG_DIR" ]; then
    echo "❌ 无法创建日志目录: $LOG_DIR"
    exit 1
fi

TIMESTAMP=$(date +%F_%H-%M-%S)
LOG_FILE="$LOG_DIR/realsense_start_RGBD_$TIMESTAMP.log"

echo "✅ 日志文件: $LOG_FILE"

# 测试日志文件是否可写
if ! touch "$LOG_FILE" 2>/dev/null; then
    echo "❌ 无法创建日志文件，使用备用路径"
    LOG_FILE="/tmp/realsense_start_$TIMESTAMP.log"
fi

RUN_CMD="ros2 launch realsense2_camera rs_launch.py"

# RGB 参数全集（已排序）
declare -A RGB_FPS
RGB_FPS["1920x1080"]="15 30 6"
RGB_FPS["1280x720"]="15 30 6"
RGB_FPS["960x540"]="15 30 6"
RGB_FPS["848x480"]="6 15 30 60"
RGB_FPS["640x480"]="6 15 30 60"
RGB_FPS["640x360"]="6 15 30 60"
RGB_FPS["424x240"]="6 15 30 60"
RGB_FPS["320x240"]="6 30 60"
RGB_FPS["320x180"]="6 30 60"

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

echo "============== 可用 RGB 模式 =============="
for res in $(printf "%s\n" "${!RGB_FPS[@]}" | sort); do
    echo "RGB  : $res @ ${RGB_FPS[$res]}"
done

echo -e "\n============== 可用 Depth 模式 =============="
for res in $(printf "%s\n" "${!DEPTH_FPS[@]}" | sort); do
    echo "Depth: $res @ ${DEPTH_FPS[$res]}"
done
echo "============================================"

# 选择要启用相机
read -p $'\n选择要启用相机(可多选: 1 RGB, 2 Depth): ' sensors
CMD_PARAMS=""
ENABLE_DEPTH=false

# RGB 配置
if [[ $sensors == *"1"* ]]; then
    echo -e "\n您选择启用 RGB 相机"
    read -p "请输入 RGB 分辨率 (如 848x480): " rgb_res
    rgb_res=$(echo "$rgb_res" | tr -d ' ')
    read -p "请输入 RGB 帧率(Hz): " rgb_fps

    if [[ -z ${RGB_FPS[$rgb_res]} ]]; then
        echo "❌ 不支持的 RGB 分辨率！"
        exit 1
    fi
    if [[ ! " ${RGB_FPS[$rgb_res]} " =~ " ${rgb_fps} " ]]; then
        echo "❌ ${rgb_res} 不支持 FPS=${rgb_fps}"
        exit 1
    fi

    CMD_PARAMS="$CMD_PARAMS enable_color:=true rgb_camera.color_profile:=${rgb_res}x${rgb_fps}"
else
    CMD_PARAMS="$CMD_PARAMS enable_color:=false"
fi

# Depth 配置 + 自动启用 Infra
if [[ $sensors == *"2"* ]]; then
    ENABLE_DEPTH=true
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
else
    CMD_PARAMS="$CMD_PARAMS enable_depth:=false enable_infra1:=false enable_infra2:=false"
fi

# 点云仅在启用深度时询问
if [[ $ENABLE_DEPTH == true ]]; then
    read -p $'\n是否启用点云发布 (y/n)? ' enable_pc
    if [[ "$enable_pc" == "y" ]]; then
        CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=true"
    else
        CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=false"
    fi
else
    CMD_PARAMS="$CMD_PARAMS pointcloud.enable:=false"
fi

echo -e "\n✅ 参数检查通过，准备启动相机...\n"

FULL_CMD="$RUN_CMD $CMD_PARAMS"

# ===== 日志增强部分 =====
echo "----------------------------------------" >> "$LOG_FILE"
echo "启动时间: $(date)" >> "$LOG_FILE"
echo "启动命令: $FULL_CMD" >> "$LOG_FILE"

# 获取设备信息写入日志
DEVICE_INFO=$(rs-enumerate-devices --compact 2>/dev/null)
if [[ -z "$DEVICE_INFO" ]]; then
    DEVICE_INFO="未检测到设备或未安装 rs-enumerate-devices"
fi
echo "设备信息:" >> "$LOG_FILE"
echo "$DEVICE_INFO" >> "$LOG_FILE"

# 写入实际参数
echo "启用 RGB: $([[ $sensors == *"1"* ]] && echo Yes || echo No)" >> "$LOG_FILE"
if [[ $sensors == *"1"* ]]; then
    echo "RGB 参数: ${rgb_res} @ ${rgb_fps} FPS" >> "$LOG_FILE"
fi
echo "启用 Depth + Infra: $([[ $ENABLE_DEPTH == true ]] && echo Yes || echo No)" >> "$LOG_FILE"
if [[ $ENABLE_DEPTH == true ]]; then
    echo "Depth 参数: ${depth_res} @ ${depth_fps} FPS" >> "$LOG_FILE"
    echo "Infra 参数: ${depth_res} @ ${depth_fps} FPS" >> "$LOG_FILE"
    echo "启用点云: $([[ $enable_pc == "y" ]] && echo Yes || echo No)" >> "$LOG_FILE"
fi
echo "----------------------------------------" >> "$LOG_FILE"
# ===== 日志增强部分结束 =====

# Ctrl+C 优雅退出
trap 'echo -e "\n👋 相机已停止"; echo "停止时间: $(date)" >> "$LOG_FILE"; exit 0' SIGINT

echo "运行命令:"
echo "$FULL_CMD"
echo -e "按 Ctrl + C 退出\n"

eval $FULL_CMD
