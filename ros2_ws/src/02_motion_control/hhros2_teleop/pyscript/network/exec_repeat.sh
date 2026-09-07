#!/bin/bash

# 设置脚本遇到错误立即退出
set -e

echo "========== ROS2 工程自动化脚本 =========="

# 1. 进入工作空间目录
echo "当前目录: $(pwd)"

# 2. 清理并构建
echo "步骤1: 编译工程..."
colcon build 

# 3. Source 安装文件
echo "步骤2: 设置环境..."
source install/setup.bash

# 4. 启动launch文件
echo "步骤3: 启动节点..."
ros2 launch robot_pkg robot_ctrl_manager.launch.py 