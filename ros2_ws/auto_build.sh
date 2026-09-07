#!/bin/bash
# ============================================================
# 编译脚本：编译 yidong_robot 项目
# 用法：./build.sh [clean]
#   clean - 先清理构建目录再编译
# ============================================================

set -e  # 遇到错误立即退出

# 设置工作目录（根据实际修改，或使用脚本所在目录）
WORKSPACE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$WORKSPACE" || exit 1

# 加载 ROS 2 环境
if [ -f /opt/ros/humble/setup.bash ]; then
    source /opt/ros/humble/setup.bash
else
    echo "ERROR: ROS 2 Humble environment not found." >&2
    exit 1
fi

# 如果设置了 Necro_DIR，则导出（可选）
if [ -n "$Necro_DIR" ]; then
    export Necro_DIR
fi

# 处理 clean 参数
if [ "$1" = "clean" ]; then
    echo "Cleaning build/ install/ log/ ..."
    rm -rf build/ install/ log/
fi

# 编译， --event-handlers console_direct+表示显示详细编译输出，便于排查错误
echo "Starting colcon build..."
colcon build \
    --symlink-install \
    --cmake-args -DROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    --event-handlers console_direct+

# 编译成功提示
echo "Build completed successfully."
echo "Source the workspace: source install/setup.bash"