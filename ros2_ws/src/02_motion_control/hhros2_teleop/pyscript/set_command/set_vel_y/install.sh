#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人左右移动控制脚本
# 功能：控制机器人的左右平移运动

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # 无颜色
UNIQUE_ID=$1
PATH_INSTALL=$2

# 默认值
TOPIC="/cmd_vel"
LINEAR_MAX=2.0
STEP=0.1
LINEAR_X=0.0
LINEAR_Y=0.0
ANGULAR_Z=0.0
UNIQUE_ID=""
EXE_PATH=""

# 显示帮助
show_help() {
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}          机器人左右移动控制${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic    设置话题名称 (默认: /cmd_vel)"
    echo "  -l, --linear   设置最大线速度 (默认: 2.0 m/s)"
    echo "  -s, --step     设置速度增量 (默认: 0.1)"
    echo "  -h, --help     显示此帮助信息"
    echo ""
    echo -e "${YELLOW}控制命令:${NC}"
    echo -e "  ${GREEN}a / A${NC}    左移 (增加线速度Y)"
    echo -e "  ${GREEN}d / D${NC}    右移 (减少线速度Y)"
    echo -e "  ${GREEN}r / R${NC}    停止 (速度归零)"
    echo -e "  ${GREEN}c / C${NC}    显示当前速度"
    echo -e "  ${RED}x / X${NC}    退出程序"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /robot1/cmd_vel -l 1.0"
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
            -l|--linear)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    LINEAR_MAX="$2"
                    shift 2
                else
                    echo -e "${RED}错误: -l/--linear 需要一个参数${NC}"
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
    
    # 限制线速度Y范围
    if (( $(echo "$linear_y > $LINEAR_MAX" | bc -l) )); then
        linear_y=$LINEAR_MAX
    elif (( $(echo "$linear_y < -$LINEAR_MAX" | bc -l) )); then
        linear_y=-$LINEAR_MAX
    fi
    
    # 更新全局变量
    LINEAR_Y=$linear_y
    
    # 发送 ROS2 消息
    ros2 topic pub -1 $TOPIC geometry_msgs/msg/Twist "
    linear:
      x: 0.0
      y: $LINEAR_Y
      z: 0.0
    angular:
      x: 0.0
      y: 0.0
      z: 0.0
    " 2>/dev/null
    
    echo -e "${GREEN}✓ 发送速度: 左/右移速度 = ${LINEAR_Y} m/s${NC}"
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}            左右移动控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "最大线速度: ${GREEN}$LINEAR_MAX${NC} m/s"
    echo -e "速度增量: ${GREEN}$STEP${NC} m/s"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "当前左/右移速度: ${GREEN}$LINEAR_Y${NC} m/s"
    echo -e "方向: ${YELLOW}$(if (( $(echo "$LINEAR_Y > 0" | bc -l) )); then echo "左移"; elif (( $(echo "$LINEAR_Y < 0" | bc -l) )); then echo "右移"; else echo "停止"; fi)${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}          机器人左右移动控制${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}A${NC}: 左移 (速度 +$STEP)"
    echo -e "  ${GREEN}D${NC}: 右移 (速度 -$STEP)"
    echo -e "  ${GREEN}R${NC}: 停止"
    echo -e "  ${GREEN}I${NC}: 手动输入速度"
    echo -e "  ${GREEN}C${NC}: 显示状态"
    echo -e "  ${RED}X${NC}: 退出"
    echo ""
    
    show_status
}

# 手动输入速度
input_speed() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          手动输入左/右移速度${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "当前速度: ${GREEN}$LINEAR_Y${NC} m/s"
    echo -e "允许范围: ${YELLOW}-$LINEAR_MAX 到 $LINEAR_MAX${NC} m/s"
    echo -e "正数: 左移, 负数: 右移"
    echo ""
    
    read_line input_speed "请输入左/右移速度 (m/s): "
    
    if [[ $input_speed =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        send_velocity 0.0 $input_speed 0.0
    else
        echo -e "${RED}错误: 请输入有效的数字${NC}"
    fi
}

# 主控制函数
control_loop() {
    show_instructions
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (A:左移, D:右移, I:手动输入, R:停止, C:状态, X:退出)...${NC}"
        read_key key
        
        case $key in
            a|A)  # 左移
                LINEAR_Y=$(echo "$LINEAR_Y + $STEP" | bc)
                send_velocity 0.0 $LINEAR_Y 0.0
                ;;
            d|D)  # 右移
                LINEAR_Y=$(echo "$LINEAR_Y - $STEP" | bc)
                send_velocity 0.0 $LINEAR_Y 0.0
                ;;
            r|R)  # 停止
                send_velocity 0.0 0.0 0.0
                echo -e "${YELLOW}已停止${NC}"
                ;;
            i|I)  # 手动输入
                input_speed
                ;;
            c|C)  # 显示状态
                show_status
                ;;
            x|X)  # 退出
                echo -e "${RED}退出左右移动控制${NC}"
                
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
echo -e "${GREEN}机器人左右移动控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "发送到话题: ${YELLOW}$TOPIC${NC}"
echo -e "最大线速度: ${GREEN}$LINEAR_MAX${NC} m/s"

control_loop