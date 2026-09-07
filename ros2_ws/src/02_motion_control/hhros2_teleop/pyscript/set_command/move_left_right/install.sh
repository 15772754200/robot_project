#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人左右移动距离控制脚本
# 功能：控制机器人以固定速度左右移动指定距离

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
DEFAULT_SPEED=0.5
LINEAR_X=0.0
LINEAR_Y=0.0
ANGULAR_Z=0.0
TARGET_DISTANCE=0.0
TOTAL_TIME=0.0
IS_MOVING=false
SPEED=0.5  # 默认移动速度
UNIQUE_ID=""
EXE_PATH=""
CLEANUP_CALLED=false  # 标记是否已调用清理

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人左右移动距离控制${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic      设置话题名称 (默认: /cmd_vel)"
    echo "  -s, --speed      设置移动速度 (默认: 0.5 m/s)"
    echo "  -h, --help       显示此帮助信息"
    echo ""
    echo -e "${YELLOW}控制命令:${NC}"
    echo -e "  ${GREEN}1${NC}    设置向左移动距离"
    echo -e "  ${GREEN}2${NC}    设置向右移动距离"
    echo -e "  ${GREEN}3${NC}    设置移动速度和距离"
    echo -e "  ${GREEN}s${NC}    立即停止"
    echo -e "  ${GREEN}c${NC}    显示当前状态"
    echo -e "  ${RED}x${NC}    退出程序"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /robot1/cmd_vel -s 0.3"
    echo "  输入1，然后输入3，表示向左移动3米"
    echo ""
    echo -e "${YELLOW}工作流程:${NC}"
    echo "  1. 选择方向 (1:向左, 2:向右)"
    echo "  2. 输入距离 (单位: 米)"
    echo "  3. 机器人以设定速度移动"
    echo "  4. 到达距离后自动停止"
    echo -e "${GREEN}========================================${NC}\n"
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
            -s|--speed)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    DEFAULT_SPEED="$2"
                    SPEED="$2"
                    if ! [[ $SPEED =~ ^[0-9]+\.?[0-9]*$ ]] || (( $(echo "$SPEED <= 0" | bc -l) )); then
                        echo -e "${RED}错误: 速度必须为正数${NC}"
                        exit 1
                    fi
                    shift 2
                else
                    echo -e "${RED}错误: -s/--speed 需要一个参数${NC}"
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
    
    # 更新全局变量
    LINEAR_X=$linear_x
    LINEAR_Y=$linear_y
    ANGULAR_Z=$angular_z
    
    # 发送 ROS2 消息 - 修改为控制linear.y实现左右移动
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
    
    if [ "$IS_MOVING" = true ]; then
        echo -e "${GREEN}▶ 移动中: 速度=${LINEAR_Y} m/s, 目标距离=${TARGET_DISTANCE}m, 剩余时间=${TOTAL_TIME}s${NC}"
    fi
}

# 停止机器人
stop_robot() {
    # 如果已经在清理中，不再重复发送停止消息
    if [ "$CLEANUP_CALLED" = true ] && [ "$IS_MOVING" = false ]; then
        return
    fi
    
    send_velocity 0.0 0.0 0.0
    IS_MOVING=false
    TARGET_DISTANCE=0.0
    TOTAL_TIME=0.0
    echo -e "${YELLOW}✓ 已停止${NC}"
}

# 计算移动时间
calculate_move_time() {
    local distance=$1
    if (( $(echo "$SPEED != 0" | bc -l) )); then
        TOTAL_TIME=$(echo "scale=2; $distance / $SPEED" | bc)
    else
        TOTAL_TIME=0
    fi
}

# 移动指定距离
move_distance() {
    local direction=$1
    local distance=$2
    local move_speed=$3
    
    if (( $(echo "$distance <= 0" | bc -l) )); then
        echo -e "${RED}错误: 距离必须大于0${NC}"
        return
    fi
    
    if (( $(echo "$move_speed <= 0" | bc -l) )); then
        echo -e "${RED}错误: 速度必须大于0${NC}"
        return
    fi
    
    SPEED=$move_speed
    
    # 设置移动方向 - 修改为控制linear.y
    if [ "$direction" = "left" ]; then
        LINEAR_Y=$SPEED
        TARGET_DISTANCE=$distance
    else
        LINEAR_Y=-$SPEED
        TARGET_DISTANCE=$distance
    fi
    
    # 计算所需时间
    calculate_move_time $distance
    
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          开始移动${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "方向: ${YELLOW}$([ "$direction" = "left" ] && echo "向左" || echo "向右")${NC}"
    echo -e "目标距离: ${GREEN}$distance${NC} 米"
    echo -e "移动速度: ${GREEN}$SPEED${NC} m/s"
    echo -e "预计时间: ${BLUE}$TOTAL_TIME${NC} 秒"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    # 开始移动
    IS_MOVING=true
    send_velocity 0.0 $LINEAR_Y 0.0
    
    # 显示倒计时
    local remaining_time=$TOTAL_TIME
    while (( $(echo "$remaining_time > 0" | bc -l) )); do
        sleep 0.1
        remaining_time=$(echo "$remaining_time - 0.1" | bc)
        printf "\r⏱️  剩余时间: %.1f 秒 | 剩余距离: %.2f 米" "$remaining_time" "$(echo "scale=2; $remaining_time * $SPEED" | bc)"
    done
    echo ""
    
    # 停止机器人
    stop_robot
    echo -e "${GREEN}✓ 已完成移动: 距离=$distance 米${NC}"
}

# 设置向左移动
setup_left_move() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          设置向左移动${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    read_line distance "请输入向左移动的距离 (米): "
    
    if [[ $distance =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$distance > 0" | bc -l) )); then
        read_line speed_input "请输入移动速度 [默认: $SPEED m/s]: "
        if [[ -n "$speed_input" ]]; then
            if [[ $speed_input =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$speed_input > 0" | bc -l) )); then
                move_distance "left" $distance $speed_input
            else
                echo -e "${RED}错误: 速度必须是正数${NC}"
            fi
        else
            move_distance "left" $distance $SPEED
        fi
    else
        echo -e "${RED}错误: 距离必须是正数${NC}"
    fi
}

# 设置向右移动
setup_right_move() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          设置向右移动${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    read_line distance "请输入向右移动的距离 (米): "
    
    if [[ $distance =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$distance > 0" | bc -l) )); then
        read_line speed_input "请输入移动速度 [默认: $SPEED m/s]: "
        if [[ -n "$speed_input" ]]; then
            if [[ $speed_input =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$speed_input > 0" | bc -l) )); then
                move_distance "right" $distance $speed_input
            else
                echo -e "${RED}错误: 速度必须是正数${NC}"
            fi
        else
            move_distance "right" $distance $SPEED
        fi
    else
        echo -e "${RED}错误: 距离必须是正数${NC}"
    fi
}

# 设置移动速度和距离
setup_move_with_speed() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          设置移动参数${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    read_line direction_choice "请选择方向 (1:向左, 2:向右): "
    
    case $direction_choice in
        1)
            read_line distance "请输入向左移动的距离 (米): "
            if [[ $distance =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$distance > 0" | bc -l) )); then
                read_line speed "请输入移动速度 (m/s): "
                if [[ $speed =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$speed > 0" | bc -l) )); then
                    move_distance "left" $distance $speed
                else
                    echo -e "${RED}错误: 速度必须是正数${NC}"
                fi
            else
                echo -e "${RED}错误: 距离必须是正数${NC}"
            fi
            ;;
        2)
            read_line distance "请输入向右移动的距离 (米): "
            if [[ $distance =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$distance > 0" | bc -l) )); then
                read_line speed "请输入移动速度 (m/s): "
                if [[ $speed =~ ^[0-9]+\.?[0-9]*$ ]] && (( $(echo "$speed > 0" | bc -l) )); then
                    move_distance "right" $distance $speed
                else
                    echo -e "${RED}错误: 速度必须是正数${NC}"
                fi
            else
                echo -e "${RED}错误: 距离必须是正数${NC}"
            fi
            ;;
        *)
            echo -e "${RED}错误: 请选择1或2${NC}"
            ;;
    esac
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          左右移动控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "当前速度: ${GREEN}$SPEED${NC} m/s"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    
    if [ "$IS_MOVING" = true ]; then
        local direction=$([ $(echo "$LINEAR_Y > 0" | bc -l) -eq 1 ] && echo "向左" || echo "向右")
        local moved_distance=$(echo "scale=2; ($TOTAL_TIME - $(echo "$TARGET_DISTANCE / $SPEED" | bc)) * $SPEED" | bc 2>/dev/null || echo "0.00")
        if (( $(echo "$moved_distance < 0" | bc -l) )); then
            moved_distance=0
        fi
        echo -e "状态: ${GREEN}移动中${NC}"
        echo -e "方向: ${YELLOW}$direction${NC}"
        echo -e "当前速度: ${GREEN}$LINEAR_Y${NC} m/s"
        echo -e "目标距离: ${BLUE}$TARGET_DISTANCE${NC} 米"
        echo -e "已移动距离: ${BLUE}$moved_distance${NC} 米"
        echo -e "剩余距离: ${BLUE}$(echo "scale=2; $TARGET_DISTANCE - $moved_distance" | bc 2>/dev/null || echo "0.00")${NC} 米"
        echo -e "预计剩余时间: ${YELLOW}$(echo "scale=1; ($TARGET_DISTANCE - $moved_distance) / $SPEED" | bc 2>/dev/null || echo "0.0")${NC} 秒"
    else
        echo -e "状态: ${YELLOW}已停止${NC}"
        echo -e "当前速度: ${GREEN}0.0${NC} m/s"
    fi
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}      机器人左右移动距离控制${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}1${NC}: 向左移动指定距离"
    echo -e "  ${GREEN}2${NC}: 向右移动指定距离"
    echo -e "  ${GREEN}3${NC}: 自定义速度和距离"
    echo -e "  ${GREEN}S${NC}: 立即停止"
    echo -e "  ${GREEN}C${NC}: 显示状态"
    echo -e "  ${RED}X${NC}: 退出"
    echo ""
    echo -e "${YELLOW}[默认设置]${NC}"
    echo -e "  控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "  默认速度: ${GREEN}$SPEED${NC} m/s"
    echo ""
    
    if [ "$IS_MOVING" = true ]; then
        echo -e "${GREEN}[状态] 机器人正在移动中...${NC}"
    else
        echo -e "${YELLOW}[状态] 机器人已停止${NC}"
    fi
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出左右移动控制程序${NC}"
    
    # 标记清理已被调用
    CLEANUP_CALLED=true
    
    # 停止机器人（只调用一次）
    stop_robot
    
    # 等待一小会儿确保消息发送完成
    sleep 0.1
    
    exit 0
}

# 主控制函数
control_loop() {
    show_instructions
    
    while true; do
        if [ "$IS_MOVING" = false ]; then
            echo -e "\n${YELLOW}等待输入命令 (1:左移, 2:右移, 3:自定义, S:停止, C:状态, X:退出)...${NC}"
            read_key key
            
            case $key in
                1)  # 向左移动
                    setup_left_move
                    show_instructions
                    ;;
                2)  # 向右移动
                    setup_right_move
                    show_instructions
                    ;;
                3)  # 自定义移动
                    setup_move_with_speed
                    show_instructions
                    ;;
                s|S)  # 停止
                    stop_robot
                    show_instructions
                    ;;
                c|C)  # 显示状态
                    show_status
                    ;;
                x|X)  # 退出
                    safe_exit
                    ;;
                *)
                    if [ -n "$key" ]; then
                        echo -e "${YELLOW}无效按键，按 C 查看状态${NC}"
                    fi
                    ;;
            esac
        else
            # 在移动中，只响应停止和退出命令
            echo -e "\n${YELLOW}移动中... 按 S 停止, X 退出${NC}"
            read_key_timeout key 0.5
            case $key in
                s|S)
                    stop_robot
                    show_instructions
                    ;;
                x|X)
                    safe_exit
                    ;;
            esac
        fi
    done
}

# 确保退出时停止机器人
cleanup() {
    # 如果已经通过safe_exit退出，则不再重复清理
    if [ "$CLEANUP_CALLED" = true ]; then
        exit 0
    fi
    
    # 防止多次调用
    CLEANUP_CALLED=true
    
    # 发送一次停止消息
    if [ "$IS_MOVING" = true ]; then
        send_velocity 0.0 0.0 0.0
        echo -e "${RED}已停止机器人并退出${NC}"
    fi
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}机器人左右移动距离控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
echo -e "默认速度: ${GREEN}$SPEED${NC} m/s"
echo ""

control_loop