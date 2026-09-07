#!/bin/bash
# 清理 cJSON 和 M2_SDK 编译产物，恢复环境至编译前状态
# 用法：在 hhros2_mic 目录下执行 sudo ./clean_mic.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "麦克风环境清理脚本"
echo "工作目录：$SCRIPT_DIR"
echo "=========================================="

# ---------- 确认清理 ----------
read -p "此操作将删除 cJSON 的系统安装文件、编译中间文件以及 M2_SDK 的编译产物。是否继续？(y/N) " -r
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "已取消。"
    exit 0
fi

# ---------- 清理 cJSON ----------
echo ""
echo "[1/3] 清理 cJSON 安装与构建..."

# 1.1 删除系统安装文件（需要 sudo）
CJSON_INSTALL_DIRS=(
    "/usr/local/include/cjson"
    "/usr/local/lib/libcjson.so*"
    "/usr/local/lib/pkgconfig/libcjson.pc"
    "/usr/local/lib/cmake/cJSON"
)

echo "  删除系统安装文件..."
for item in "${CJSON_INSTALL_DIRS[@]}"; do
    if ls $item &>/dev/null 2>&1; then
        sudo rm -rf $item
        echo "  已删除：$item"
    fi
done

# 1.2 删除 cJSON 源码目录中的 build 文件夹（如果有）
CJSON_BUILD_DIR="$SCRIPT_DIR/cJSON/build"
if [ -d "$CJSON_BUILD_DIR" ]; then
    echo "  删除 cJSON 构建目录：$CJSON_BUILD_DIR"
    rm -rf "$CJSON_BUILD_DIR"
else
    echo "  cJSON 构建目录不存在，跳过。"
fi

# 1.3 删除可能生成的临时文件（如 CMakeCache.txt 等）
CJSON_SRC_DIR="$SCRIPT_DIR/cJSON"
if [ -f "$CJSON_SRC_DIR/CMakeCache.txt" ]; then
    rm -f "$CJSON_SRC_DIR/CMakeCache.txt"
    echo "  删除 CMakeCache.txt"
fi
if [ -d "$CJSON_SRC_DIR/CMakeFiles" ]; then
    rm -rf "$CJSON_SRC_DIR/CMakeFiles"
    echo "  删除 CMakeFiles"
fi

echo "cJSON 清理完成。"

# ---------- 清理 M2_SDK 编译产物 ----------
echo ""
echo "[2/3] 清理 M2_SDK 编译产物..."

M2_SDK_BIN_DIR="$SCRIPT_DIR/M2_SDK/offline_mic/bin"
M2_SDK_BUILD_DIR="$SCRIPT_DIR/M2_SDK/offline_mic/samples/record_sample"

# 2.1 删除可执行文件 record_sample
if [ -f "$M2_SDK_BIN_DIR/record_sample" ]; then
    rm -f "$M2_SDK_BIN_DIR/record_sample"
    echo "  已删除：$M2_SDK_BIN_DIR/record_sample"
else
    echo "  可执行文件 record_sample 不存在，跳过。"
fi

# 2.2 删除编译产生的 .o 文件（在 record_sample 目录下）
if [ -d "$M2_SDK_BUILD_DIR" ]; then
    cd "$M2_SDK_BUILD_DIR"
    # 删除所有 .o 文件
    if ls *.o &>/dev/null 2>&1; then
        rm -f *.o
        echo "  已删除所有 .o 文件（$M2_SDK_BUILD_DIR）"
    else
        echo "  没有 .o 文件需要删除。"
    fi
    # 删除其他可能的临时文件（如 make 生成的依赖文件）
    if [ -f "make.sh" ]; then
        # 保留 make.sh 本身，只删除编译中间文件
        # 但有些编译可能生成 .d 等，可酌情删除
        rm -f *.d 2>/dev/null || true
    fi
else
    echo "  编译目录不存在，跳过。"
fi

# 2.3 可选：删除 bin 目录下其他可能生成的测试文件（保留目录结构）
# 这里只清理我们已知的 record_sample，其余不动
echo "M2_SDK 清理完成。"

# ---------- 清理串口规则（可选） ----------
echo ""
echo "[3/3] 是否删除串口 udev 规则文件？（输入 y 删除，其他跳过）"
read -p "删除 /etc/udev/rules.d/*ch9102* 规则？(y/N) " -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
    RULES=$(ls /etc/udev/rules.d/*ch9102* 2>/dev/null || true)
    if [ -n "$RULES" ]; then
        sudo rm -f /etc/udev/rules.d/*ch9102*
        echo "  已删除串口规则文件。"
        echo "  提示：请重新插拔麦克风，设备名将恢复为 ttyACM*。"
    else
        echo "  未找到串口规则文件，跳过。"
    fi
else
    echo "  保留串口规则。"
fi

echo ""
echo "=========================================="
echo "清理完成！"
echo "提示："
echo "  - 如需重新编译，请运行 ./setup_mic.sh"
echo "  - 如需重新配置串口规则，请运行 serial_driver/ch9102_udev.sh"
echo "=========================================="