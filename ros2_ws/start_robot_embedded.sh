#!/usr/bin/env bash
set -e

# ============================================================
# 本脚本用来实现自动加载rs485驱动、配置GPIO、编译工作空间并启动robot_embeded程序,
# 从而控制机器人电源板给电机供电，确保机器人正常运行。
# ============================================================

# ============================================
# 颜色定义
# ============================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============================================
# 配置变量（根据实际工程结构修正）
# ============================================
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTERNAL_TOOLS="../${PROJECT_ROOT}/external_tools"
EMBEDDED_WS="${EXTERNAL_TOOLS}/hhros2_embedded"  # 修正拼写
ROS2_DISTRO="humble"  # 如果使用其他版本，请修改这里

# GPIO 配置
GPIO485_RIGHT=460  # 靠近电源一端（对应 /dev/ttyTHS0）
GPIO485_LEFT=477   # 另一端（对应 /dev/ttyTHS2）

# 485 驱动路径（位于 src 目录下）
DRIVER_485="${EMBEDDED_WS}/src/485.ko"

# ============================================
# 打印函数
# ============================================
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}>>> $1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

# ============================================
# 检查并设置 485.ko 文件权限
# ============================================
check_485_permissions() {
    print_step "检查 485.ko 驱动文件权限"

    if [ ! -f "${DRIVER_485}" ]; then
        print_error "485.ko 驱动文件不存在: ${DRIVER_485}"
        print_info "请确认文件路径是否正确"
        return 1
    fi

    print_info "驱动文件路径: ${DRIVER_485}"
    
    # 显示当前权限
    CURRENT_PERMS=$(ls -l "${DRIVER_485}" | awk '{print $1}')
    print_info "当前权限: ${CURRENT_PERMS}"

    # 检查是否有执行权限（对于 .ko 文件，实际上是可读权限即可，但 insmod 需要可读）
    # 这里检查是否所有用户都有可读权限，或者至少当前用户有可读权限
    if [ ! -r "${DRIVER_485}" ]; then
        print_warning "驱动文件缺少可读权限，正在添加..."
        sudo chmod +r "${DRIVER_485}" 2>/dev/null || {
            print_warning "无法添加可读权限，尝试添加所有权限..."
            sudo chmod 644 "${DRIVER_485}" 2>/dev/null || true
        }
        print_success "已添加可读权限"
    else
        print_success "驱动文件权限正常（可读）"
    fi

    # 额外检查：确保 .ko 文件对 root 可执行（insmod 需要 root）
    if [ ! -x "${DRIVER_485}" ] && [ "$EUID" -eq 0 ]; then
        print_info "添加执行权限（便于调试）..."
        sudo chmod +x "${DRIVER_485}" 2>/dev/null || true
    fi

    # 显示更新后的权限
    NEW_PERMS=$(ls -l "${DRIVER_485}" | awk '{print $1}')
    print_info "更新后权限: ${NEW_PERMS}"
    
    print_success "权限检查完成"
    return 0
}

# ============================================
# 检查并配置 485 驱动
# ============================================
setup_485_driver() {
    print_step "配置 RS485 驱动和 GPIO"

    # 先检查并设置 485.ko 权限
    check_485_permissions || {
        print_warning "485.ko 权限检查失败，但继续执行..."
    }

    # 加载 485.ko 驱动
    if [ -f "${DRIVER_485}" ]; then
        print_info "找到 485 驱动: ${DRIVER_485}"
        if lsmod | grep -q "485"; then
            print_info "485 驱动已加载"
        else
            print_info "加载 485 驱动模块..."
            sudo insmod "${DRIVER_485}" 2>/dev/null || {
                print_warning "加载 485.ko 失败，可能已加载或权限不足"
                print_info "尝试使用 modprobe 或检查 dmesg 查看详细信息"
            }
        fi
    else
        print_warning "未找到 485.ko 驱动文件: ${DRIVER_485}"
        print_info "请确保文件存在于: ${DRIVER_485}"
    fi

    # 检查 GPIO 导出
    if [ ! -d "/sys/class/gpio/gpio${GPIO485_RIGHT}" ]; then
        print_info "导出 GPIO ${GPIO485_RIGHT}..."
        echo ${GPIO485_RIGHT} > /sys/class/gpio/export 2>/dev/null || {
            print_warning "GPIO ${GPIO485_RIGHT} 可能已导出或权限不足，尝试继续..."
        }
    fi

    if [ ! -d "/sys/class/gpio/gpio${GPIO485_LEFT}" ]; then
        print_info "导出 GPIO ${GPIO485_LEFT}..."
        echo ${GPIO485_LEFT} > /sys/class/gpio/export 2>/dev/null || {
            print_warning "GPIO ${GPIO485_LEFT} 可能已导出或权限不足，尝试继续..."
        }
    fi

    # 设置 GPIO 方向为输出
    print_info "设置 GPIO 方向为输出..."
    if [ -f "/sys/class/gpio/gpio${GPIO485_RIGHT}/direction" ]; then
        echo out > /sys/class/gpio/gpio${GPIO485_RIGHT}/direction 2>/dev/null || {
            print_warning "无法设置 GPIO ${GPIO485_RIGHT} 方向，可能需要 root 权限"
        }
    fi
    if [ -f "/sys/class/gpio/gpio${GPIO485_LEFT}/direction" ]; then
        echo out > /sys/class/gpio/gpio${GPIO485_LEFT}/direction 2>/dev/null || {
            print_warning "无法设置 GPIO ${GPIO485_LEFT} 方向，可能需要 root 权限"
        }
    fi

    # 设置默认值（接收模式）
    print_info "设置 GPIO 默认状态为接收模式 (value=1)..."
    if [ -f "/sys/class/gpio/gpio${GPIO485_RIGHT}/value" ]; then
        echo 1 > /sys/class/gpio/gpio${GPIO485_RIGHT}/value 2>/dev/null || true
    fi
    if [ -f "/sys/class/gpio/gpio${GPIO485_LEFT}/value" ]; then
        echo 1 > /sys/class/gpio/gpio${GPIO485_LEFT}/value 2>/dev/null || true
    fi

    # 配置引脚复用（需要 root 权限）
    print_info "配置引脚复用寄存器..."
    if command -v busybox &> /dev/null; then
        sudo busybox devmem 0x0243d070 w 0x00000400 2>/dev/null || {
            print_warning "配置 0x0243d070 失败，可能需要 root 权限"
        }
        sudo busybox devmem 0x0243d078 w 0x00000458 2>/dev/null || {
            print_warning "配置 0x0243d078 失败，可能需要 root 权限"
        }
    else
        print_warning "busybox 未安装，跳过引脚复用配置"
    fi

    # 验证配置
    print_info "验证串口设备..."
    if ls -l /dev/ttyTHS* &>/dev/null; then
        ls -l /dev/ttyTHS* | grep -E "ttyTHS[02]" || true
        print_success "串口设备已就绪"
    else
        print_warning "未找到 /dev/ttyTHS* 设备"
    fi

    # 验证 485 驱动加载状态
    print_info "验证 485 驱动状态..."
    if lsmod | grep -q "485"; then
        print_success "485 驱动已成功加载"
        lsmod | grep "485"
    else
        print_warning "485 驱动未加载，请检查 dmesg 日志"
        print_info "可以执行: dmesg | tail -20 查看错误信息"
    fi

    print_success "RS485 配置完成"
}

# ============================================
# 编译功能包
# ============================================
build_embeded_ws() {
    print_step "检查并编译 hhros2_embedded 工作空间"

    if [ ! -d "${EMBEDDED_WS}" ]; then
        print_error "工作空间目录不存在: ${EMBEDDED_WS}"
        print_info "请确认路径是否正确，当前目录结构应为："
        echo "  ${EXTERNAL_TOOLS}/"
        echo "  └── hhros2_embedded/"
        echo "      ├── src/"
        echo "      │   ├── robot_embeded_bringup/"
        echo "      │   ├── robot_embeded_driver/"
        echo "      │   ├── robot_embeded_interfaces/"
        echo "      │   └── 485.ko"
        echo "      ├── install/"
        echo "      └── build/"
        exit 1
    fi

    cd "${EMBEDDED_WS}"

    # 检查是否已编译
    if [ -d "install" ] && [ -f "install/setup.bash" ]; then
        print_info "检测到已编译的工作空间，跳过编译步骤"
        print_info "如需重新编译，请删除 install/ 和 build/ 目录后再次运行"
        return 0
    fi

    print_info "开始编译工作空间..."

    # 设置 ROS2 环境
    if [ -f "/opt/ros/${ROS2_DISTRO}/setup.bash" ]; then
        source "/opt/ros/${ROS2_DISTRO}/setup.bash"
    else
        print_error "ROS2 ${ROS2_DISTRO} 环境未找到"
        print_info "请确认 ROS2 版本是否正确，或修改脚本中的 ROS2_DISTRO 变量"
        exit 1
    fi

    # 安装依赖（如果存在）
    if [ -f "src/robot_embeded_bringup/package.xml" ]; then
        print_info "安装 ROS 依赖包..."
        rosdep install --from-paths src --ignore-src -r -y 2>/dev/null || {
            print_warning "rosdep 安装依赖失败，继续编译..."
        }
    fi

    # 编译
    print_info "执行 colcon build (--symlink-install)..."
    colcon build --symlink-install || {
        print_error "编译失败，请检查错误信息"
        exit 1
    }

    print_success "编译完成"
}

# ============================================
# 启动程序
# ============================================
start_application() {
    print_step "启动 robot_embeded 程序"

    cd "${EMBEDDED_WS}"

    # 检查编译是否完成
    if [ ! -f "install/setup.bash" ]; then
        print_error "未找到 install/setup.bash，请先编译"
        exit 1
    fi

    # 设置环境
    if [ -f "/opt/ros/${ROS2_DISTRO}/setup.bash" ]; then
        source "/opt/ros/${ROS2_DISTRO}/setup.bash"
    fi
    source install/setup.bash

    print_info "启动 launch 文件..."
    print_info "命令: ros2 launch robot_embeded_bringup robot_embeded.launch.py"

    # 启动 launch

    ros2 launch robot_embeded_bringup robot_embeded.launch.py &
    LAUNCH_PID=$!

    cleanup() {
    if kill -0 "$LAUNCH_PID" 2>/dev/null; then
        kill "$LAUNCH_PID"
        wait "$LAUNCH_PID" 2>/dev/null || true
    fi
    }
    trap cleanup EXIT INT TERM

    ros2 run robot_embeded_bringup customer_panel
}

# ============================================
# 显示启动信息
# ============================================
show_banner() {
    echo ""
    echo "  ╔══════════════════════════════════════════════════╗"
    echo "  ║     robot_embeded 自动化启动脚本                   ║"
    echo "  ║     Project: YIDONG_ROBOT_PROJECT                ║"
    echo "  ╚══════════════════════════════════════════════════╝"
    echo ""
}

# ============================================
# 主函数
# ============================================
main() {
    show_banner

    # 检查是否以 root 运行
    if [ "$EUID" -ne 0 ]; then 
        print_warning "当前用户不是 root，某些操作可能需要 sudo 权限"
        print_warning "如果遇到权限问题，请使用 sudo 运行此脚本"
        echo ""
    fi

    # 步骤 1: 设置 RS485（包含权限检查）
    setup_485_driver

    # 步骤 2: 编译工作空间
    build_embeded_ws

    # 步骤 3: 启动程序
    start_application
}

# ============================================
# 错误处理
# ============================================
trap 'print_error "脚本被中断或发生错误"; exit 1' INT TERM ERR

# ============================================
# 执行主函数
# ============================================
main "$@"