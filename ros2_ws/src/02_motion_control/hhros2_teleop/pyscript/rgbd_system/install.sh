#!/bin/bash

# 检查Python版本
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
RGBD_SCRIPT="$REALSENSE_ROOT/realsense_toolbox_menu.py"

# 检查RGBD系统脚本是否存在
if [ ! -f "$RGBD_SCRIPT" ]; then
    echo "错误: 未找到RGBD系统脚本: $RGBD_SCRIPT"
    exit 1
fi

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即运行RGBD系统? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "正在启动RGBD系统..."
    python3 "$RGBD_SCRIPT"
fi

echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

# 检查kill终端脚本是否存在
if [ -f "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" ]; then
    python3 "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" "$UNIQUE_ID"
else
    echo "警告: 未找到kill_terminal.py脚本，无法自动关闭终端"
    echo "终端ID: $UNIQUE_ID"
fi
