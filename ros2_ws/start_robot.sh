#!/bin/bash

# ============================================================
# 启动脚本：完全依赖 bringup.launch.py 启动 L0 与所有节点
# 仅提供启动前/后的清理，确保无残留进程
# ============================================================

# 工作目录（根据实际修改）
WORKSPACE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$WORKSPACE" || exit 1

# 使用相对项目结构的路径设置 ONNX Runtime 库搜索路径
export LD_LIBRARY_PATH="$WORKSPACE/../external_tools/hhros2_thirdparty/onnxruntime-linux-x64-1.27.0/lib:$LD_LIBRARY_PATH"

source install/setup.bash

# 定义清理函数
cleanup() {
    echo "[$(date +%H:%M:%S)] Cleaning up residual processes..."
    pkill -f hhros2_l0_runtime 2>/dev/null
    pkill -f ros2_control_node 2>/dev/null
    pkill -f xsens_mti_node 2>/dev/null
    pkill -f spawner 2>/dev/null
    pkill -f component_container_mt 2>/dev/null
    pkill -f robot_state_publisher 2>/dev/null
    pkill -f hhros2_core 2>/dev/null
    rm -f /dev/shm/hhros2_motor_shm /dev/shm/fastrtps_* 2>/dev/null
    echo "[$(date +%H:%M:%S)] Cleanup done."
}

# 捕获 EXIT 信号（正常退出 / Ctrl+C），执行清理
trap cleanup EXIT

# 启动前先清理一次，确保环境干净
cleanup

# 启动 bringup（前台运行，内部会按序启动 L0 及其他节点）
# hardware:=mujoco/real
# backend:=sim/ecat
# IMU 发布者由 hardware 自动选择：real 使用 Xsens，mujoco 使用
# imu_sensor_broadcaster，mock 默认不发布 IMU。
# 如果测试脚裸关节并联算法，执行指令为start_robot.sh ankle_test，启动时会将 motion_reference_topic 切换为 /motion_core/reference_disabled，避免干扰测试。
echo "[$(date +%H:%M:%S)] Starting bringup..."
MOTION_REFERENCE_TOPIC="/humanoid_base_controller/reference"
if [ "${1:-}" = "ankle_test" ]; then
    MOTION_REFERENCE_TOPIC="/motion_core/reference_disabled"
    echo "[$(date +%H:%M:%S)] Ankle diagnostic isolation enabled."
fi
ros2 launch hhros_bringup bringup.launch.py \
    hardware:=mujoco \
    backend:=sim \
    enable_imu:=false \
    motion_reference_topic:="${MOTION_REFERENCE_TOPIC}"

# bringup 退出后（无论是正常退出还是被中断），trap 会自动执行 cleanup
echo "[$(date +%H:%M:%S)] bringup exited. Cleanup will be performed automatically."

