#!/bin/bash
# 系统管理工具安装脚本

echo "========================================="
echo "  developer system"
echo "  开发者: 王崇超"
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




python3 $PATH_INSTALL/pyscript/set_command/developer_system/menu.py $UNIQUE_ID

    
python3 $PATH_INSTALL/pyscript/kill_terminal.py $UNIQUE_ID
