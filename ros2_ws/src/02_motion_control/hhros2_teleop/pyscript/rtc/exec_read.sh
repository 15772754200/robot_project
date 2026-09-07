#!/bin/bash

echo "========================================="
echo "    RTC时钟读取 "
echo "  开发者: 油亚龙"
echo "========================================="

# 检查Python版本
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3,请先安装Python3"
    exit 1
fi

echo "检测到 Python 版本: $(python3 --version)"

UNIQUE_ID=$1
PATH_INSTALL=$2

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即读取RTC时钟历史? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    python3 $PATH_INSTALL/pyscript/rtc/rtc_read.py
fi


echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

python3 $PATH_INSTALL/pyscript/set_command/kill_terminal.py $UNIQUE_ID