#!/bin/bash
# 系统管理工具安装脚本

echo "========================================="
echo "  Ubuntu 系统管理工具 - 安装程序"
echo "  开发者: 丁培峰"
echo "========================================="


# 检查Python版本
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3,请先安装Python3"
    exit 1
fi

echo "检测到 Python 版本: $(python3 --version)"

# 安装依赖包
echo ""
echo "正在安装依赖包..."
pip3 install psutil tabulate

echo "========================================="
echo "          调整系统进程优先级"
echo "  可进入下方管理工具后选择1,2调整进程优先级"
echo "========================================="



UNIQUE_ID=$1
PATH_INSTALL=$2

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即运行进程管理工具? (y/n): " -n 1 -r
echo




if [[ $REPLY =~ ^[Yy]$ ]]; then
    python3 $PATH_INSTALL/pyscript/system/system_manager.py $UNIQUE_ID
else
    python3 $PATH_INSTALL/pyscript/kill_terminal.py $UNIQUE_ID
fi