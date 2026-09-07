#!/bin/bash
# 外设管理系统安装脚本


UNIQUE_ID=$1
PATH_INSTALL=$2

python3 $PATH_INSTALL/pyscript/mic/use_mic/mic.py $UNIQUE_ID

python3 $PATH_INSTALL/pyscript/kill_terminal.py $UNIQUE_ID
