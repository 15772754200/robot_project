#!/bin/bash
# 系统管理工具安装脚本

echo "========================================="
echo "  Ubuntu 系统管理工具 - 安装程序"
echo "  开发者: 丁培峰"
echo "========================================="

# 检查Python版本
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3，请先安装Python3"
    exit 1
fi

echo "检测到 Python 版本: $(python3 --version)"

# 安装依赖包
echo ""
echo "正在安装依赖包..."
pip3 install psutil tabulate

# 创建必要的目录
echo ""
echo "创建配置目录..."
mkdir -p ~/.system_manager/logs

# 复制主程序
echo ""
echo "安装主程序..."
sudo cp system_manager.py /usr/local/bin/system_manager
sudo chmod +x /usr/local/bin/system_manager

# 创建桌面快捷方式
if [ -d ~/Desktop ]; then
    cat > ~/Desktop/system_manager.desktop << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=系统管理工具
Comment=进程和存储管理工具
Exec=gnome-terminal -- /usr/local/bin/system_manager
Icon=utilities-system-monitor
Terminal=false
Categories=System;
EOF
    chmod +x ~/Desktop/system_manager.desktop
    echo "已创建桌面快捷方式"
fi

# 创建命令别名
echo ""
echo "创建命令别名..."
echo "alias sysmgr='python3 /usr/local/bin/system_manager'" >> ~/.bashrc
echo "alias sysmgr-sudo='sudo python3 /usr/local/bin/system_manager'" >> ~/.bashrc

echo ""
echo "========================================="
echo "  安装完成！"
echo ""
echo "  使用方法："
echo "  1. 命令行运行: sysmgr"
echo "  2. 管理员模式: sysmgr-sudo"
echo "  3. 或直接运行: python3 system_manager.py"
echo ""
echo "  注意：某些功能需要管理员权限"
echo "========================================="

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即运行系统管理工具? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    python3 /usr/local/bin/system_manager
fi