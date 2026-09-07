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

# 检查RGB系统脚本是否存在
if [ ! -f "$PATH_INSTALL/pyscript/network_system/net_toolbox_menu.py" ]; then
    echo "错误: 未找到RGB系统脚本: $PATH_INSTALL/pyscript/network_system/net_toolbox_menu.py"
    exit 1
fi

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即运行RGB系统? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "正在启动RGBD系统..."
    python3 "$PATH_INSTALL/pyscript/network_system/net_toolbox_menu.py"
fi

echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

# 检查kill终端脚本是否存在
if [ -f "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" ]; then
    python3 "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" "$UNIQUE_ID"
else
    echo "警告: 未找到kill_terminal.py脚本,无法自动关闭终端"
    echo "终端ID: $UNIQUE_ID"
fi