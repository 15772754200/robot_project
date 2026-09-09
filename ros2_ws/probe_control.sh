#!/usr/bin/env bash
set -euo pipefail

# 探针启动脚本（前台运行，Ctrl+C 停止）
# 启动前自动清理旧的探针共享内存，避免版本冲突

# 获取脚本所在目录（假设脚本在项目根目录下）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="${SCRIPT_DIR}"
source install/setup.bash

# 默认配置文件路径
DEFAULT_CONFIG="${WORKSPACE}/install/hhros2_motor_diagnostics/share/hhros2_motor_diagnostics/config/single_shot_motor_probe.yaml"

# 如果用户提供了自定义配置文件路径，则使用
if [ $# -ge 1 ]; then
    CONFIG_FILE="$1"
else
    CONFIG_FILE="$DEFAULT_CONFIG"
fi

# 检查配置文件是否存在
if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件 '$CONFIG_FILE' 不存在。"
    exit 1
fi

# ★ 清理旧的探针共享内存（避免版本不匹配）
SHM_NAME="/dev/shm/sb_single_shot_probe_v1"
if [ -e "$SHM_NAME" ]; then
    echo "清理旧的探针共享内存: $SHM_NAME"
    rm -f "$SHM_NAME"
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
echo "[2/4] 使能电机..."
ros2 service call /hhros2_core/set_system_state hhros2_interfaces/srv/SetSystemState "{enable: true}" || {
    echo "使能失败。"
    exit 1
}

# 等待使能生效
sleep 0.5

echo "========================================"
echo "启动 single_shot_motor_probe 探针"
echo "配置文件: $CONFIG_FILE"
echo "按 Ctrl+C 停止探针"
echo "========================================"

# 运行探针节点（前台运行）
ros2 run hhros2_motor_diagnostics single_shot_motor_probe \
    --ros-args --params-file "$CONFIG_FILE"
