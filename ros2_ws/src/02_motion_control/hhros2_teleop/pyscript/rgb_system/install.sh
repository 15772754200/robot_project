#!/bin/bash

# 检查Python版本
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3,请先安装Python3"
    exit 1
fi

echo "检查并安装必要的库函数..."

# 检查并安装opencv-python
if python3 -c "import cv2" 2>/dev/null; then
    echo "  opencv-python 已安装"
else
    echo "  正在安装opencv-python..."
    pip install opencv-python
    if [ $? -ne 0 ]; then
        echo "  安装失败，尝试使用pip3..."
        pip3 install opencv-python || echo "  opencv-python 安装失败，请手动安装"
    fi
fi

# 检查并安装cheese
if command -v cheese &> /dev/null; then
    echo "  cheese 已安装"
else
    echo "  正在安装cheese..."
    sudo apt-get install -y cheese
    if [ $? -ne 0 ]; then
        echo "  cheese 安装失败，请检查网络连接或软件源"
    fi
fi

# 检查并安装v4l-utils
if command -v v4l2-ctl &> /dev/null; then
    echo "  v4l-utils 已安装"
else
    echo "  正在安装v4l-utils..."
    sudo apt-get install -y v4l-utils
    if [ $? -ne 0 ]; then
        echo "  v4l-utils 安装失败，请检查网络连接或软件源"
    fi
fi

# 检查并安装ffmpeg
if command -v ffmpeg &> /dev/null; then
    echo "  ffmpeg 已安装"
else
    echo "  正在安装ffmpeg..."
    sudo apt-get install -y ffmpeg
    if [ $? -ne 0 ]; then
        echo "  ffmpeg 安装失败，请检查网络连接或软件源"
    fi
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
RGB_SCRIPT="$REALSENSE_ROOT/rgb1/RGB_sys.py"

# 检查RGB系统脚本是否存在
if [ ! -f "$RGB_SCRIPT" ]; then
    echo "错误: 未找到RGB系统脚本: $RGB_SCRIPT"
    exit 1
fi

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即运行RGB系统? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "正在启动RGB系统..."
    python3 "$RGB_SCRIPT"
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
