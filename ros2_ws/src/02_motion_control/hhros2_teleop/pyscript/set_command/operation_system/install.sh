#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"
# 蓝牙连接日志系统
echo "========================================="
echo "  运控操作系统采集系统 "
echo "  开发者: 油亚龙"
echo "========================================="

# 检查Python版本
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3，请先安装Python3"
    exit 1
fi

echo "检测到 Python 版本: $(python3 --version)"

UNIQUE_ID=$1
PATH_INSTALL=$2

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read_key_prompt REPLY "是否立即运行网络连接日志采集系统? (y/n): "
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    python3 $PATH_INSTALL/pyscript/set_command/robot_joint.py
fi

echo ""
read_key_prompt REPLY "按任意键返回主控终端 "
echo

python3 $PATH_INSTALL/pyscript/set_command/kill_terminal.py $UNIQUE_ID