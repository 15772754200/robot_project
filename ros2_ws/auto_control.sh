#!/bin/bash

# ============================================================
# 自动化控制脚本：在 运行start_robot.sh 后执行
# 用法：./auto_control.sh [--force]
#   --force  跳过确认提示（慎用）
# ============================================================

# 设置项目唯一的 ROS 环境
WORKSPACE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$WORKSPACE" || exit 1
source install/setup.bash

# 确认机器人已安全支撑/悬挂
if [ "$1" != "--force" ]; then
    echo "⚠️  警告：此脚本将使能电机并发送运动命令！"
    echo "请确保机器人已安全支撑/悬挂，且周围无人。"
    read -p "确认继续？(y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "取消操作。"
        exit 1
    fi
fi

# 1. 激活 humanoid_base_controller（切换到站立模式）
echo "[1/4] 激活 humanoid_base_controller..."
ros2 service call /hhros2_core/set_control_mode hhros2_interfaces/srv/SetControlMode "{mode: 2}" || {
    echo "激活失败，请检查控制器是否已加载。"
    exit 1
}

# 等待控制器切换完成
sleep 0.5

# 2. 使能电机（正式上力）
# echo "[2/4] 使能电机..."
# ros2 service call /hhros2_core/set_system_state hhros2_interfaces/srv/SetSystemState "{enable: true}" || {
#     echo "使能失败。"
#     exit 1
# }

# 等待使能生效
sleep 0.5

# 3. 按照200hz的频率发布测试指令
read -p "是否发送测试命令（第一个关节移动到0.1 rad）？(y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "[3/4] 发送测试命令..."
    ros2 topic pub /humanoid_base_controller/reference hhros2_interfaces/msg/JointMotor \
    "{position: [0.0, 0.0, 0.0, 0.0, -0.024101902974297103, 0.07373704631755151, 0.0, 0.0, 0.0, 0.0, 0.01641490599885822, 0.09208016326836128, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], velocity: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], effort: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], kp: [100.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], kd: [10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}" \
    --rate 200
fi

echo "[4/4] 完成！"
echo "可以使用 'ros2 topic echo /joint_states' 观察关节运动。"
