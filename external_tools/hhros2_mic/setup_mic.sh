#!/bin/bash
# 麦克风环境自动配置脚本（精确检测，避免重复编译）
# 用法：在包含 hhros2_mic 的目录下执行 sudo ./setup_mic.sh

set -e  # 遇错退出

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MIC_BASE="$SCRIPT_DIR"

echo "=========================================="
echo "开始配置麦克风环境，基础目录：$MIC_BASE"
echo "=========================================="

# ---------- 步骤1：串口规则 ----------
echo "[1/4] 配置串口 udev 规则..."
RULE_EXIST=$(ls /etc/udev/rules.d/ | grep -i "ch9102" || true)
if [ -n "$RULE_EXIST" ]; then
    echo "→ 已检测到串口规则文件 ($RULE_EXIST)，跳过此步骤。"
else
    SERIAL_DIR="$MIC_BASE/serial_driver"
    if [ -f "$SERIAL_DIR/ch9102_udev.sh" ]; then
        cd "$SERIAL_DIR"
        sudo ./ch9102_udev.sh
        echo "串口规则已配置。"
    else
        echo "错误：找不到 $SERIAL_DIR/ch9102_udev.sh"
        exit 1
    fi
fi

# ---------- 步骤2：cJSON 安装（检测实际安装路径）----------
echo "[2/4] 安装 cJSON 库..."
# 实际头文件在 /usr/local/include/cjson/cJSON.h，库文件为 libcjson.so
if [ -f "/usr/local/include/cjson/cJSON.h" ] && [ -f "/usr/local/lib/libcjson.so" ]; then
    echo "→ cJSON 已安装（检测到 /usr/local/include/cjson/cJSON.h 和 libcjson.so），跳过此步骤。"
else
    CJSON_DIR="$MIC_BASE/cJSON"
    if [ ! -d "$CJSON_DIR" ]; then
        echo "错误：cJSON 目录不存在"
        exit 1
    fi
    cd "$CJSON_DIR"
    
    # 尝试多种构建方式
    if [ -f "CMakeLists.txt" ]; then
        echo "使用 CMake 构建..."
        mkdir -p build && cd build
        cmake ..
        make
        sudo make install
    elif [ -f "Makefile" ]; then
        echo "使用 Makefile 构建..."
        make
        sudo make install
    elif [ -f "libcjson.a" ] || [ -f "libcjson.so" ]; then
        echo "检测到已编译的库文件，直接复制..."
        sudo cp *.h /usr/local/include/ 2>/dev/null || true
        sudo cp *.a /usr/local/lib/ 2>/dev/null || true
        sudo cp *.so /usr/local/lib/ 2>/dev/null || true
    else
        echo "错误：无法识别 cJSON 目录的构建方式"
        exit 1
    fi
    
    # 确保动态库路径
    if ! grep -q "^/usr/local/lib$" /etc/ld.so.conf; then
        echo "/usr/local/lib" | sudo tee -a /etc/ld.so.conf > /dev/null
    fi
    sudo ldconfig
    echo "cJSON 安装完成。"
fi

# ---------- 步骤3：编译驱动（检测最终生成的 bin 文件）----------
echo "[3/4] 编译麦克风驱动..."
BIN_DIR="$MIC_BASE/M2_SDK/offline_mic/bin"
# 编译后的可执行文件在 bin/record_sample
if [ -f "$BIN_DIR/record_sample" ]; then
    echo "→ 驱动已编译（检测到 $BIN_DIR/record_sample），跳过此步骤。"
else
    DRIVER_DIR="$MIC_BASE/M2_SDK/offline_mic/samples/record_sample"
    if [ -f "$DRIVER_DIR/make.sh" ]; then
        cd "$DRIVER_DIR"
        chmod +x ./make.sh
        sh ./make.sh
        echo "驱动编译完成。"
    else
        echo "错误：找不到 $DRIVER_DIR/make.sh"
        exit 1
    fi
fi

# ---------- 步骤4：修改 mic.py 路径 ----------
echo "[4/4] 更新 mic.py 中的 bin 路径..."
MIC_PY="$MIC_BASE/mic.py"
if [ ! -f "$MIC_PY" ]; then
    echo "错误：找不到 $MIC_PY"
    exit 1
fi

USER_NAME=$(whoami)
EXPECTED_PATH="/home/$USER_NAME/yidong_robot_project/external_tools/hhros2_mic/M2_SDK/offline_mic/bin"

if grep -q "$EXPECTED_PATH" "$MIC_PY"; then
    echo "→ mic.py 中的路径已经是正确的 ($EXPECTED_PATH)，跳过修改。"
else
    sed -i "s|/home/[^/]*/yidong_robot_project/external_tools/hhros2_mic/M2_SDK/offline_mic/bin|$EXPECTED_PATH|g" "$MIC_PY"
    if grep -q "$EXPECTED_PATH" "$MIC_PY"; then
        echo "mic.py 路径已更新为：$EXPECTED_PATH"
    else
        echo "警告：未能自动替换路径，请手动检查 $MIC_PY 第 9 行。"
    fi
fi

echo "=========================================="
echo "所有步骤执行完毕！"
echo "=========================================="