#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人旋转控制脚本
# 功能：控制机器人的旋转运动

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m' # 无颜色

# 默认值
TOPIC="/cmd_vel"
ANGULAR_MAX=1.0
STEP=0.1
LINEAR_X=0.0
LINEAR_Y=0.0
ANGULAR_Z=0.0
UNIQUE_ID=""
EXE_PATH=""

# 显示帮助
show_help() {
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}          机器人旋转控制${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic      设置话题名称 (默认: /cmd_vel)"
    echo "  -a, --angular    设置最大角速度 (默认: 1.0 rad/s)"
    echo "  -s, --step       设置速度增量 (默认: 0.1 rad/s)"
    echo "  -h, --help       显示此帮助信息"
    echo ""
    echo -e "${YELLOW}控制命令:${NC}"
    echo -e "  ${GREEN}q / Q${NC}    左转 (增加角速度Z)"
    echo -e "  ${GREEN}e / E${NC}    右转 (减少角速度Z)"
    echo -e "  ${GREEN}r / R${NC}    停止 (速度归零)"
    echo -e "  ${GREEN}c / C${NC}    显示当前角速度"
    echo -e "  ${RED}x / X${NC}    退出程序"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /robot1/cmd_vel -a 0.5"
    echo -e "${GREEN}=========================================${NC}\n"
}

# 解析命令行参数
parse_args() {
    # 先处理选项参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -t|--topic)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    TOPIC="$2"
                    shift 2
                else
                    echo -e "${RED}错误: -t/--topic 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -a|--angular)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    ANGULAR_MAX="$2"
                    shift 2
                else
                    echo -e "${RED}错误: -a/--angular 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -s|--step)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    STEP="$2"
                    shift 2
                else
                    echo -e "${RED}错误: -s/--step 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            --)  # 选项结束符
                shift
                # 保存剩余的参数
                REMAINING_ARGS=("$@")
                break
                ;;
            -*)  # 未知选项
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
            *)  # 位置参数
                # 保存位置参数
                if [[ -z "$UNIQUE_ID" ]]; then
                    UNIQUE_ID="$1"
                elif [[ -z "$EXE_PATH" ]]; then
                    EXE_PATH="$1"
                fi
                shift
                ;;
        esac
    done
}

# 发送速度命令的函数
send_velocity() {
    local linear_x=$1
    local linear_y=$2
    local angular_z=$3
    
    # 限制角速度Z范围
    if (( $(echo "$angular_z > $ANGULAR_MAX" | bc -l) )); then
        angular_z=$ANGULAR_MAX
    elif (( $(echo "$angular_z < -$ANGULAR_MAX" | bc -l) )); then
        angular_z=-$ANGULAR_MAX
    fi
    
    # 更新全局变量
    ANGULAR_Z=$angular_z
    
    # 发送 ROS2 消息
    ros2 topic pub -1 $TOPIC geometry_msgs/msg/Twist "
    linear:
      x: 0.0
      y: 0.0
      z: 0.0
    angular:
      x: 0.0
      y: 0.0
      z: $ANGULAR_Z
    " 2>/dev/null
    
    echo -e "${GREEN}✓ 发送角速度: 旋转速度 = ${ANGULAR_Z} rad/s${NC}"
    
    # 显示旋转方向
    if (( $(echo "$ANGULAR_Z > 0" | bc -l) )); then
        echo -e "${YELLOW}方向: 逆时针旋转 (左转)${NC}"
    elif (( $(echo "$ANGULAR_Z < 0" | bc -l) )); then
        echo -e "${YELLOW}方向: 顺时针旋转 (右转)${NC}"
    else
        echo -e "${YELLOW}方向: 停止旋转${NC}"
    fi
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}            旋转控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "最大角速度: ${GREEN}$ANGULAR_MAX${NC} rad/s"
    echo -e "速度增量: ${GREEN}$STEP${NC} rad/s"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "当前旋转速度: ${GREEN}$ANGULAR_Z${NC} rad/s"
    
    # 计算RPM
    rpm=$(echo "scale=2; $ANGULAR_Z * 60 / (2 * 3.14159)" | bc 2>/dev/null || echo "0.00")
    echo -e "角速度 (RPM): ${BLUE}$rpm${NC}"
    
    # 显示方向
    if (( $(echo "$ANGULAR_Z > 0" | bc -l) )); then
        echo -e "旋转方向: ${PURPLE}逆时针 (左转)${NC}"
    elif (( $(echo "$ANGULAR_Z < 0" | bc -l) )); then
        echo -e "旋转方向: ${PURPLE}顺时针 (右转)${NC}"
    else
        echo -e "旋转方向: ${YELLOW}停止${NC}"
    fi
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}          机器人旋转控制${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}Q${NC}: 左转 (逆时针, 角速度 +$STEP)"
    echo -e "  ${GREEN}E${NC}: 右转 (顺时针, 角速度 -$STEP)"
    echo -e "  ${GREEN}R${NC}: 停止旋转"
    echo -e "  ${GREEN}I${NC}: 手动输入角速度"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${RED}X${NC}: 退出"
    echo ""
    
    echo -e "${YELLOW}[注意]${NC}"
    echo -e "  正值: 逆时针旋转 (左转)"
    echo -e "  负值: 顺时针旋转 (右转)"
    echo ""
    
    show_status
}

# 手动输入角速度
input_angular_speed() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          手动输入角速度${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "当前角速度: ${GREEN}$ANGULAR_Z${NC} rad/s"
    echo -e "允许范围: ${YELLOW}-$ANGULAR_MAX 到 $ANGULAR_MAX${NC} rad/s"
    echo -e "正值: 逆时针 (左转), 负值: 顺时针 (右转)"
    echo ""
    
    read_line input_z "请输入角速度 (rad/s): "
    
    if [[ $input_z =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        send_velocity 0.0 0.0 $input_z
    else
        echo -e "${RED}错误: 请输入有效的数字${NC}"
    fi
}

# 主控制函数
control_loop() {
    show_instructions
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (Q:左转, E:右转, I:手动输入, R:停止, C:状态, X:退出)...${NC}"
        read_key key
        
        case $key in
            q|Q)  # 左转
                ANGULAR_Z=$(echo "$ANGULAR_Z + $STEP" | bc)
                send_velocity 0.0 0.0 $ANGULAR_Z
                ;;
            e|E)  # 右转
                ANGULAR_Z=$(echo "$ANGULAR_Z - $STEP" | bc)
                send_velocity 0.0 0.0 $ANGULAR_Z
                ;;
            r|R)  # 停止
                send_velocity 0.0 0.0 0.0
                echo -e "${YELLOW}已停止旋转${NC}"
                ;;
            i|I)  # 手动输入
                input_angular_speed
                ;;
            c|C)  # 显示状态
                show_status
                ;;
            x|X)  # 退出
                echo -e "${RED}退出旋转控制${NC}"
                
                exit 0
                ;;
            *)
                echo -e "${YELLOW}无效按键，按 C 查看状态${NC}"
                ;;
        esac
    done
}

# 主程序
clear
echo -e "${GREEN}机器人旋转控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "发送到话题: ${YELLOW}$TOPIC${NC}"
echo -e "最大角速度: ${GREEN}$ANGULAR_MAX${NC} rad/s"

control_loop