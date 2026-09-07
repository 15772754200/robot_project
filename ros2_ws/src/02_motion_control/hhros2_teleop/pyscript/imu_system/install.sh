#!/bin/bash

###############################################################################
# IMU 功能管理系统 (IMU Manager for ROS 2)
# 适用系统: Ubuntu 22.04 (Gnome Terminal)
# 功能: 自动管理 Xsens 驱动启动、校准、自检及数据分析
###############################################################################

# --- 颜色定义 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# --- 配置变量 ---
USB_PORT="/dev/ttyUSB0"
WORKSPACE_SETUP="install/setup.bash"

UNIQUE_ID=$1
PATH_INSTALL=$2

# --- 检查并赋予 USB 权限 ---
check_usb_permission() {
    echo -e "${BLUE}[系统] 正在检查传感器端口 $USB_PORT ...${NC}"
    
    if [ ! -e "$USB_PORT" ]; then
        echo -e "${RED}[错误] 未找到设备 $USB_PORT。请确认 IMU 已连接 USB。${NC}"
        read -p "按回车键返回菜单..."
        return 1
    fi

    # 检查读写权限
    if [ ! -r "$USB_PORT" ] || [ ! -w "$USB_PORT" ]; then
        echo -e "${YELLOW}[警告] 当前用户无权限访问 $USB_PORT。${NC}"
        echo -e "${YELLOW}[提示] 需要输入 sudo 密码来赋予权限...${NC}"
        sudo chmod 666 "$USB_PORT"
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}[成功] 权限已赋予。${NC}"
        else
            echo -e "${RED}[失败] 无法赋予权限。${NC}"
            read -p "按回车键返回..."
            return 1
        fi
    else
        echo -e "${GREEN}[正常] 端口连接正常且有访问权限。${NC}"
    fi
    return 0
}

# --- 在新标签页运行命令 ---
run_in_new_tab() {
    local title="$1"
    local command="$2"
    
    echo -e "${GREEN}[执行] 正在启动: $title ...${NC}"
    
    # 构造在新标签页中执行的完整命令链：
    # 1. source 环境 (处理 ROS2 依赖)
    # 2. 执行具体指令
    # 3. exec bash (防止指令执行完或报错后窗口直接关闭)
    gnome-terminal --tab --title="$title" -- bash -c "
        echo -e '\033[1;33m=== $title ===\033[0m';
        if [ -f '$WORKSPACE_SETUP' ]; then
            source $WORKSPACE_SETUP;
        else
            echo -e '\033[0;31m[错误] 找不到 $WORKSPACE_SETUP。请先执行编译 (选项 6)。\033[0m';
            exec bash;
        fi
        echo '正在执行: $command';
        echo '---------------------------------------------------';
        $command;
        echo '---------------------------------------------------';
        echo -e '\033[1;32m[提示] 任务结束或已停止。\033[0m';
        exec bash
    "
}

# --- 检查编译环境 ---
check_env() {
    if [ ! -f "$WORKSPACE_SETUP" ]; then
        echo -e "${RED}[警告] 未检测到 $WORKSPACE_SETUP。${NC}"
        echo -e "${YELLOW}建议先执行 [6] 编译工作空间。${NC}"
        return 1
    fi
    return 0
}

# --- 主界面循环 ---
while true; do
    clear
    echo -e "${CYAN}=============================================================${NC}"
    echo -e "${CYAN}* ROS 2 IMU SYSTEM MANAGER (Xsens)                 *${NC}"
    echo -e "${CYAN}=============================================================${NC}"
    
    # 显示端口状态
    if [ -e "$USB_PORT" ]; then
        echo -e "* 传感器状态: ${GREEN}在线 ($USB_PORT)${NC}"
    else
        echo -e "* 传感器状态: ${RED}离线 (未检测到 $USB_PORT)${NC}"
    fi
    
    echo -e "${CYAN}=============================================================${NC}"
    echo -e "  [1] 🚀 启动 IMU 驱动 (Driver)"
    echo -e "      (ros2 launch xsens_mti_ros2_driver xsens_mti_node.launch.py)"
    echo -e ""
    echo -e "  [2] 🔧 IMU 校准程序 (Calibration)"
    echo -e "      (ros2 launch imu_tools imu_calibration.launch.py)"
    echo -e ""
    echo -e "  [3] 🏥 姿态自检 (Attitude Self-Check)"
    echo -e "      (ros2 launch imu_tools attitude_self_check.launch.py)"
    echo -e ""
    echo -e "  [4] 🩺 简单自检 (Simple Self-Check)"
    echo -e "      (ros2 launch imu_tools imu_self_check.launch.py)"
    echo -e ""
    echo -e "  [5] 📉 数据上报分析 (Data Reporter)"
    echo -e "      (ros2 launch imu_tools imu_data_reporter.launch.py)"
    echo -e "${CYAN}-------------------------------------------------------${NC}"
    echo -e "  [6] 🔨 编译工作空间 (Colcon Build)"
    echo -e "  [0] ❌ 退出 (Exit)"
    echo -e "${CYAN}=============================================================${NC}"
    
    read -p "请输入选项 [0-6]: " choice

    case $choice in
        1)
            check_usb_permission && \
            run_in_new_tab "IMU_Driver" "ros2 launch xsens_mti_ros2_driver xsens_mti_node.launch.py"
            ;;
        2)
            check_env && \
            run_in_new_tab "IMU_Calibration" "ros2 launch imu_tools imu_calibration.launch.py"
            ;;
        3)
            check_env && \
            run_in_new_tab "Attitude_Check" "ros2 launch imu_tools attitude_self_check.launch.py"
            ;;
        4)
            check_env && \
            run_in_new_tab "Simple_Check" "ros2 launch imu_tools imu_self_check.launch.py"
            ;;
        5)
            check_env && \
            run_in_new_tab "Data_Reporter" "ros2 launch imu_tools imu_data_reporter.launch.py"
            ;;
        6)
            echo -e "${YELLOW}[系统] 开始编译... (colcon build --symlink-install)${NC}"
            colcon build --symlink-install
            if [ $? -eq 0 ]; then
                echo -e "${GREEN}[成功] 编译完成！${NC}"
                # 编译完成后尝试自动 source，方便当前终端也能用（虽然子标签页会自己 source）
                source install/setup.bash 2>/dev/null
            else
                echo -e "${RED}[失败] 编译出错，请检查代码。${NC}"
            fi
            read -p "按回车键继续..."
            ;;
        0)
            echo -e "${GREEN}再见!${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}无效选项，请重新输入。${NC}"
            sleep 1
            ;;
    esac
done

if [ -f "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" ]; then
    python3 "$PATH_INSTALL/pyscript/set_command/kill_terminal.py" "$UNIQUE_ID"
else
    echo "警告: 未找到kill_terminal.py脚本，无法自动关闭终端"
    echo "终端ID: $UNIQUE_ID"
fi