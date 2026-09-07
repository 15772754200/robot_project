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

# 重新加载bashrc
source ~/.bashrc 2>/dev/null
UNIQUE_ID=$1
PATH_INSTALL=$2

echo ""
read -p "是否立即运行外设管理系统? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    # python3 peripheral_manager.py
    python3 $PATH_INSTALL/pyscript/peripheral_system/peripheral_manager.py $UNIQUE_ID
else
    python3 $PATH_INSTALL/pyscript/kill_terminal.py $UNIQUE_ID
fi