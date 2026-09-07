#!/bin/bash
# 外设管理系统安装脚本

echo "========================================="
echo "  运控主板外设管理系统 - 安装程序"
echo "  开发者: 丁培峰"
echo "========================================="

# 检查Python版本
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3，请先安装Python3"
    exit 1
fi

echo "检测到 Python 版本: $(python3 --version)"

# 安装系统依赖
echo ""
echo "安装系统工具..."
sudo apt update
sudo apt install -y usbutils          # lsusb
sudo apt install -y bluez bluez-tools  # 蓝牙工具
sudo apt install -y net-tools         # 网络工具
sudo apt install -y alsa-utils        # 音频工具

# 安装Python依赖包
echo ""
echo "正在安装Python依赖包..."
sudo pip3 install psutil
sudo pip3 install pyserial
sudo pip3 install tabulate

# 创建必要的目录
echo ""
echo "创建配置目录..."
mkdir -p ~/.peripheral_manager/logs

# 复制主程序
echo ""
echo "安装主程序..."
sudo cp peripheral_manager.py /usr/local/bin/peripheral_manager
sudo chmod +x /usr/local/bin/peripheral_manager

# 创建命令别名
echo ""
echo "创建命令别名..."
echo "alias pmgr='python3 /usr/local/bin/peripheral_manager'" >> ~/.bashrc
echo "alias pmgr-sudo='sudo python3 /usr/local/bin/peripheral_manager'" >> ~/.bashrc

# 设置udev规则（允许普通用户访问USB设备）
echo ""
echo "配置设备访问权限..."
sudo tee /etc/udev/rules.d/99-peripheral-manager.rules > /dev/null << EOF
# Allow users to access USB devices
SUBSYSTEM=="usb", MODE="0666"
# Allow users to access serial ports
SUBSYSTEM=="tty", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger

echo ""
echo "========================================="
echo "  安装完成！"
echo ""
echo "  使用方法："
echo "  1. 命令行运行: pmgr"
echo "  2. 管理员模式: pmgr-sudo"
echo "  3. 或直接运行: python3 peripheral_manager.py"
echo ""
echo "  注意："
echo "  - 启用/禁用网络接口需要管理员权限"
echo "  - 首次运行可能需要重新登录以应用权限设置"
echo "========================================="

# 重新加载bashrc
source ~/.bashrc 2>/dev/null

echo ""
read -p "是否立即运行外设管理系统? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    python3 peripheral_manager.py
fi