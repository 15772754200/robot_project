#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"
# 外设管理系统安装脚本

echo "========================================="
echo "           机器人跳跃 "
echo "1. 确保机器人处于行走或跑步阶段"
echo "2. 在控制终端进入单字符模式"
echo "3. 通过键盘设定机器人线速度:"        
echo "4.     'w':前进速度+0.1"
echo "5.     's':前进速度-0.1"
echo "6.     'a':平移速度+0.1"
echo "7.     'd':平移速度-0.1"
echo "8.     'q':偏航速度+0.1"
echo "9.     'e':偏航速度-0.1"
echo "10.    'r':清0所有速度"
echo "========================================="


UNIQUE_ID=$1
PATH_INSTALL=$2

echo ""
read_key_prompt REPLY "按任意键返回主控终端 "
echo

python3 $PATH_INSTALL/pyscript/set_command/kill_terminal.py $UNIQUE_ID
