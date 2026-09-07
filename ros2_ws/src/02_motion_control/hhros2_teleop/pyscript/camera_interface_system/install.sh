#!/bin/bash

if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3,请先安装Python3"
    exit 1
fi

echo ""
echo "检测到 Python 版本: $(python3 --version)"

UNIQUE_ID=$1
PATH_INSTALL=$2

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

REALSENSE_ROOT="$PROJECT_ROOT/external_tools/hhros2_realsense"
CAMERA_MENU_SCRIPT="$REALSENSE_ROOT/camera_menu.py"

if [ ! -f "$CAMERA_MENU_SCRIPT" ]; then
    echo "错误: 未找到相机接口脚本: $CAMERA_MENU_SCRIPT"
    exit 1
fi

source ~/.bashrc 2>/dev/null

echo "正在启动相机接口..."
python3 "$CAMERA_MENU_SCRIPT"

echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

if [ -f "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" ]; then
    python3 "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" "$UNIQUE_ID"
else
    echo "警告: 未找到kill_terminal.py脚本，无法自动关闭终端"
    echo "终端ID: $UNIQUE_ID"
fi
