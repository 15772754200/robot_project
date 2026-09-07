#!/bin/bash
# 外设管理系统安装脚本

echo "========================================="
echo "              调用扬声器接口说明 "
echo "             插上扬声器后即可使用 "
echo "========================================="


UNIQUE_ID=$1
PATH_INSTALL=$2

echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

python3 $PATH_INSTALL/pyscript/kill_terminal.py $UNIQUE_ID
