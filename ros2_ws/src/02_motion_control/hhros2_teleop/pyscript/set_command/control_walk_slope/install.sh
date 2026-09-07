#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人走斜坡控制脚本
# 功能：控制机器人以行走步态在斜坡上移动
# 注意：底层使用的是行走步态(模式3)，只是界面描述为走斜坡

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
CONTROL_TOPIC="/hhros2_core/set_control_mode"
DEFAULT_SPEED=0.1
MAX_SPEED=2.0
STEP=0.1
LINEAR_X=0.0
LINEAR_Y=0.0
ANGULAR_Z=0.0
CURRENT_MODE=0
WALKING_MODE=1
STOP_MODE=2
UNIQUE_ID=""
EXE_PATH=""
CLEANUP_CALLED=false  # 标记是否已调用清理
INPUT_MODE="keyboard" # 控制模式: keyboard-键盘控制, manual-手动输入

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人走斜坡控制${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic      设置速度控制话题 (默认: /cmd_vel)"
    echo "  -c, --ctrl       设置控制模式服务 (默认: /hhros2_core/set_control_mode)"
    echo "  -l, --linear     设置最大线速度 (默认: 2.0 m/s)"
    echo "  -s, --step       设置速度增量 (默认: 0.1 m/s)"
    echo "  -m, --mode       设置控制模式: keyboard(默认) 或 manual"
    echo "  -h, --help       显示此帮助信息"
    echo ""
    echo -e "${YELLOW}控制模式说明:${NC}"
    echo "  keyboard: 通过键盘WSADQE控制 (默认)"
    echo "  manual: 手动输入速度值"
    echo ""
    echo -e "${YELLOW}键盘控制命令:${NC}"
    echo -e "  ${GREEN}W${NC}: 增加前进速度 (+X方向)"
    echo -e "  ${GREEN}S${NC}: 减少前进速度 (-X方向)"
    echo -e "  ${GREEN}A${NC}: 增加左移速度 (+Y方向)"
    echo -e "  ${GREEN}D${NC}: 减少左移速度 (-Y方向)"
    echo -e "  ${GREEN}Q${NC}: 增加左转角速度 (+Z方向)"
    echo -e "  ${GREEN}E${NC}: 减少左转角速度 (-Z方向)"
    echo -e "  ${GREEN}R${NC}: 重置所有速度为0"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo ""
    echo -e "${YELLOW}手动输入控制命令:${NC}"
    echo -e "  ${GREEN}1${NC}: 输入X方向速度 (前进/后退)"
    echo -e "  ${GREEN}2${NC}: 输入Y方向速度 (左移/右移)"
    echo -e "  ${GREEN}3${NC}: 输入Z方向角速度 (旋转)"
    echo -e "  ${GREEN}4${NC}: 同时输入XYZ三轴速度"
    echo -e "  ${GREEN}R${NC}: 重置所有速度为0"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo ""
    echo -e "${YELLOW}[走斜坡重要提示]${NC}"
    echo -e "  ${GREEN}⚠ 走斜坡前请注意:${NC}"
    echo -e "  1. 请确保机器人${YELLOW}正面对准斜坡${NC}方向"
    echo -e "  2. 建议${YELLOW}低速${NC}平稳上坡"
    echo -e "  3. 避免在斜坡上${YELLOW}急转弯${NC}"
    echo -e "  4. 注意机器人${YELLOW}重心稳定${NC}"
    echo ""
    echo -e "${YELLOW}工作流程:${NC}"
    echo "  1. 脚本启动后，自动发送走斜坡步态命令 (模式3)"
    echo "  2. 然后进入速度控制模式"
    echo "  3. 通过键盘或手动输入控制机器人上斜坡"
    echo "  4. 退出时自动发送停止步态命令 (模式2)"
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
            -c|--ctrl)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    CONTROL_TOPIC="$2"
                    shift 2
                else
                    echo -e "${RED}错误: -c/--ctrl 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -l|--linear)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    MAX_SPEED="$2"
                    if ! [[ $MAX_SPEED =~ ^[0-9]+\.?[0-9]*$ ]] || (( $(echo "$MAX_SPEED <= 0" | bc -l 2>/dev/null) )); then
                        echo -e "${RED}错误: 最大线速度必须为正数${NC}"
                        exit 1
                    fi
                    shift 2
                else
                    echo -e "${RED}错误: -l/--linear 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -s|--step)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    STEP="$2"
                    if ! [[ $STEP =~ ^[0-9]+\.?[0-9]*$ ]] || (( $(echo "$STEP <= 0" | bc -l 2>/dev/null) )); then
                        echo -e "${RED}错误: 速度增量必须为正数${NC}"
                        exit 1
                    fi
                    shift 2
                else
                    echo -e "${RED}错误: -s/--step 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -m|--mode)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    INPUT_MODE="$2"
                    if [[ "$INPUT_MODE" != "keyboard" && "$INPUT_MODE" != "manual" ]]; then
                        echo -e "${RED}错误: 控制模式必须是 'keyboard' 或 'manual'${NC}"
                        exit 1
                    fi
                    shift 2
                else
                    echo -e "${RED}错误: -m/--mode 需要一个参数${NC}"
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

# 发送走斜坡步态命令
send_slope_gait() {
    local mode=$1
    local description=$2
    
    # 更新全局变量
    CURRENT_MODE=$mode
    
    # 发送 ROS2 消息
    local response
    response="$(ros2 service call "$CONTROL_TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: $mode}" 2>&1)"
    printf '%s\n' "$response"
    
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        case $mode in
            2)
                echo -e "${YELLOW}✓ 发送控制命令: 停止 (模式=$mode)${NC}"
                ;;
            3)
                echo -e "${GREEN}✓ 发送控制命令: 走斜坡 (模式=$mode)${NC}"
                ;;
            *)
                echo -e "${BLUE}✓ 发送控制命令: 模式=$mode${NC}"
                ;;
        esac
        if [[ -n "$description" ]]; then
            echo -e "描述: $description${NC}"
        fi
    else
        echo -e "${RED}✗ 发送控制命令失败${NC}"
        return 1
    fi
    return 0
}

# 发送速度命令
send_velocity() {
    local linear_x=$1
    local linear_y=$2
    local angular_z=$3
    
    # 限制速度范围
    if (( $(echo "$linear_x > $MAX_SPEED" | bc -l 2>/dev/null) )); then
        linear_x=$MAX_SPEED
    elif (( $(echo "$linear_x < -$MAX_SPEED" | bc -l 2>/dev/null) )); then
        linear_x=-$MAX_SPEED
    fi
    
    if (( $(echo "$linear_y > $MAX_SPEED" | bc -l 2>/dev/null) )); then
        linear_y=$MAX_SPEED
    elif (( $(echo "$linear_y < -$MAX_SPEED" | bc -l 2>/dev/null) )); then
        linear_y=-$MAX_SPEED
    fi
    
    if (( $(echo "$angular_z > $MAX_SPEED" | bc -l 2>/dev/null) )); then
        angular_z=$MAX_SPEED
    elif (( $(echo "$angular_z < -$MAX_SPEED" | bc -l 2>/dev/null) )); then
        angular_z=-$MAX_SPEED
    fi
    
    # 更新全局变量
    LINEAR_X=$linear_x
    LINEAR_Y=$linear_y
    ANGULAR_Z=$angular_z
    
    # 发送 ROS2 消息
    ros2 topic pub -1 $TOPIC geometry_msgs/msg/Twist "
    linear:
      x: $LINEAR_X
      y: $LINEAR_Y
      z: 0.0
    angular:
      x: 0.0
      y: 0.0
      z: $ANGULAR_Z
    " 2>/dev/null
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ 发送速度: X=${LINEAR_X} m/s, Y=${LINEAR_Y} m/s, ang_vel_yaw=${ANGULAR_Z} rad/s${NC}"
    else
        echo -e "${RED}✗ 发送速度失败${NC}"
        return 1
    fi
    return 0
}

# 重置所有速度
reset_velocity() {
    LINEAR_X=0.0
    LINEAR_Y=0.0
    ANGULAR_Z=0.0
    
    send_velocity 0.0 0.0 0.0
    if [ $? -eq 0 ]; then
        echo -e "${YELLOW}✓ 重置所有速度为0${NC}"
    else
        echo -e "${RED}✗ 重置速度失败${NC}"
    fi
}

# 安全比较函数
compare_numbers() {
    local a=$1
    local b=$2
    local op=$3
    
    # 使用bc进行比较，并处理可能的错误
    result=$(echo "$a $op $b" | bc -l 2>/dev/null || echo "0")
    if [[ "$result" == "1" ]]; then
        return 0
    else
        return 1
    fi
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          走斜坡控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制模式: ${PURPLE}$INPUT_MODE${NC}"
    echo -e "速度控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "控制模式服务: ${YELLOW}$CONTROL_TOPIC${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    
    echo -e "当前控制模式: ${GREEN}走斜坡模式 (模式3)${NC}"
    echo -e "控制状态: ${GREEN}已启动${NC}"
    echo ""
    
    echo -e "速度设置:"
    echo -e "  最大线速度: ${GREEN}$MAX_SPEED${NC} m/s"
    echo -e "  速度增量: ${GREEN}$STEP${NC} m/s"
    echo -e "  走斜坡建议速度: ${YELLOW}0.1-0.3 m/s${NC}"
    echo ""
    
    echo -e "当前速度:"
    echo -e "  前进/后退 (X): ${GREEN}$LINEAR_X${NC} m/s"
    echo -e "  左/右移 (Y): ${GREEN}$LINEAR_Y${NC} m/s"
    echo -e "  左/右转 (Z): ${GREEN}$ANGULAR_Z${NC} rad/s"
    echo ""
    
    echo -e "方向判断:"
    if (( $(echo "$LINEAR_X > 0" | bc -l 2>/dev/null) )); then
        echo -e "  X方向: ${GREEN}前进${NC}"
    elif (( $(echo "$LINEAR_X < 0" | bc -l 2>/dev/null) )); then
        echo -e "  X方向: ${GREEN}后退${NC}"
    else
        echo -e "  X方向: ${YELLOW}停止${NC}"
    fi
    
    if (( $(echo "$LINEAR_Y > 0" | bc -l 2>/dev/null) )); then
        echo -e "  Y方向: ${GREEN}左移${NC}"
    elif (( $(echo "$LINEAR_Y < 0" | bc -l 2>/dev/null) )); then
        echo -e "  Y方向: ${GREEN}右移${NC}"
    else
        echo -e "  Y方向: ${YELLOW}停止${NC}"
    fi
    
    if (( $(echo "$ANGULAR_Z > 0" | bc -l 2>/dev/null) )); then
        echo -e "  Z方向: ${GREEN}左转${NC}"
    elif (( $(echo "$ANGULAR_Z < 0" | bc -l 2>/dev/null) )); then
        echo -e "  Z方向: ${GREEN}右转${NC}"
    else
        echo -e "  Z方向: ${YELLOW}停止${NC}"
    fi
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 显示走斜坡提示
show_slope_warning() {
    echo -e "\n${YELLOW}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}        走斜坡安全提示${NC}"
    echo -e "${YELLOW}════════════════════════════════════════${NC}"
    echo -e "${GREEN}⚠ 走斜坡前请确保:${NC}"
    echo -e "  1. 机器人${YELLOW}正面对准斜坡${NC}方向"
    echo -e "  2. 从${YELLOW}低速${NC}开始控制"
    echo -e "  3. 斜坡上${YELLOW}避免急转弯${NC}"
    echo -e "  4. 注意保持机器人${YELLOW}重心稳定${NC}"
    echo -e "  5. 建议${YELLOW}匀速${NC}上坡"
    echo -e "${YELLOW}════════════════════════════════════════${NC}\n"
    echo -e "按回车键继续..."
    read_line _terminal_key ""
}

# 手动输入X方向速度
input_linear_x() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          输入X方向速度${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}走斜坡模式${NC}"
    echo -e "当前速度: ${GREEN}$LINEAR_X${NC} m/s"
    echo -e "速度范围: ${YELLOW}-$MAX_SPEED 到 $MAX_SPEED${NC} m/s"
    echo -e "正值: 前进, 负值: 后退"
    echo -e "${YELLOW}⚠ 走斜坡建议: 低速前进，小心后退${NC}"
    echo ""
    
    read_line input_x "请输入X方向速度 (m/s): "
    
    if [[ $input_x =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        send_velocity $input_x $LINEAR_Y $ANGULAR_Z
    else
        echo -e "${RED}错误: 请输入有效的数字${NC}"
    fi
}

# 手动输入Y方向速度
input_linear_y() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          输入Y方向速度${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}走斜坡模式${NC}"
    echo -e "当前速度: ${GREEN}$LINEAR_Y${NC} m/s"
    echo -e "速度范围: ${YELLOW}-$MAX_SPEED 到 $MAX_SPEED${NC} m/s"
    echo -e "正值: 左移, 负值: 右移"
    echo -e "${YELLOW}⚠ 走斜坡建议: 避免高速侧移${NC}"
    echo ""
    
    read_line input_y "请输入Y方向速度 (m/s): "
    
    if [[ $input_y =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        send_velocity $LINEAR_X $input_y $ANGULAR_Z
    else
        echo -e "${RED}错误: 请输入有效的数字${NC}"
    fi
}

# 手动输入Z方向角速度
input_angular_z() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          输入Z方向角速度${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}走斜坡模式${NC}"
    echo -e "当前角速度: ${GREEN}$ANGULAR_Z${NC} rad/s"
    echo -e "速度范围: ${YELLOW}-$MAX_SPEED 到 $MAX_SPEED${NC} rad/s"
    echo -e "正值: 左转(逆时针), 负值: 右转(顺时针)"
    echo -e "${YELLOW}⚠ 走斜坡建议: 低速转弯，保持稳定${NC}"
    echo ""
    
    read_line input_z "请输入Z方向角速度 (rad/s): "
    
    if [[ $input_z =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        send_velocity $LINEAR_X $LINEAR_Y $input_z
    else
        echo -e "${RED}错误: 请输入有效的数字${NC}"
    fi
}

# 手动输入三轴速度
input_all_velocity() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          输入三轴速度${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}走斜坡模式${NC}"
    echo -e "当前速度: X=${GREEN}$LINEAR_X${NC} m/s, Y=${GREEN}$LINEAR_Y${NC} m/s, Z=${GREEN}$ANGULAR_Z${NC} rad/s"
    echo -e "速度范围: ${YELLOW}-$MAX_SPEED 到 $MAX_SPEED${NC}"
    echo -e "X: 前进/后退, Y: 左移/右移, Z: 旋转"
    echo -e "${YELLOW}⚠ 走斜坡建议: 保持低速，避免急转弯${NC}"
    echo ""
    
    read_line input_x "请输入X方向速度 (m/s) [当前: $LINEAR_X]: "
    if [[ -z "$input_x" ]]; then
        input_x=$LINEAR_X
    fi
    
    read_line input_y "请输入Y方向速度 (m/s) [当前: $LINEAR_Y]: "
    if [[ -z "$input_y" ]]; then
        input_y=$LINEAR_Y
    fi
    
    read_line input_z "请输入Z方向角速度 (rad/s) [当前: $ANGULAR_Z]: "
    if [[ -z "$input_z" ]]; then
        input_z=$ANGULAR_Z
    fi
    
    if [[ $input_x =~ ^-?[0-9]+\.?[0-9]*$ ]] && \
       [[ $input_y =~ ^-?[0-9]+\.?[0-9]*$ ]] && \
       [[ $input_z =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        send_velocity $input_x $input_y $input_z
    else
        echo -e "${RED}错误: 请输入有效的数字${NC}"
    fi
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}      机器人走斜坡控制${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制模式]${NC}"
    echo -e "  当前模式: ${PURPLE}$INPUT_MODE${NC}"
    echo ""
    
    if [ "$INPUT_MODE" = "keyboard" ]; then
        echo -e "${YELLOW}[键盘控制命令]${NC}"
        echo -e "  ${GREEN}W${NC}: 增加前进速度 (+X方向)"
        echo -e "  ${GREEN}S${NC}: 减少前进速度 (-X方向)"
        echo -e "  ${GREEN}A${NC}: 增加左移速度 (+Y方向)"
        echo -e "  ${GREEN}D${NC}: 减少左移速度 (-Y方向)"
        echo -e "  ${GREEN}Q${NC}: 增加左转角速度 (+Z方向)"
        echo -e "  ${GREEN}E${NC}: 减少左转角速度 (-Z方向)"
        echo -e "  ${GREEN}R${NC}: 重置所有速度为0"
        echo -e "  ${GREEN}C${NC}: 显示当前状态"
        echo -e "  ${RED}X${NC}: 退出"
    else
        echo -e "${YELLOW}[手动输入控制命令]${NC}"
        echo -e "  ${GREEN}1${NC}: 输入X方向速度 (前进/后退)"
        echo -e "  ${GREEN}2${NC}: 输入Y方向速度 (左移/右移)"
        echo -e "  ${GREEN}3${NC}: 输入Z方向角速度 (旋转)"
        echo -e "  ${GREEN}4${NC}: 同时输入XYZ三轴速度"
        echo -e "  ${GREEN}R${NC}: 重置所有速度为0"
        echo -e "  ${GREEN}C${NC}: 显示当前状态"
        echo -e "  ${RED}X${NC}: 退出"
    fi
    
    echo ""
    echo -e "${YELLOW}[走斜坡控制]${NC}"
    echo -e "  控制模式: ${GREEN}走斜坡步态 (模式3)${NC}"
    echo -e "  退出时将切换为: ${YELLOW}停止步态 (模式2)${NC}"
    echo ""
    echo -e "${YELLOW}[走斜坡建议]${NC}"
    echo -e "  ${GREEN}✓${NC} 低速平稳上坡"
    echo -e "  ${GREEN}✓${NC} 避免急转弯"
    echo -e "  ${GREEN}✓${NC} 保持重心稳定"
    echo -e "  ${GREEN}✓${NC} 正面对准斜坡"
    echo ""
    echo -e "${YELLOW}[速度控制]${NC}"
    echo -e "  速度控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "  控制模式服务: ${YELLOW}$CONTROL_TOPIC${NC}"
    echo -e "  最大速度: ${GREEN}$MAX_SPEED${NC} m/s"
    echo -e "  速度增量: ${GREEN}$STEP${NC} m/s"
    echo ""
    echo -e "${YELLOW}[当前状态]${NC}"
    echo -e "  当前控制模式: ${GREEN}走斜坡${NC}"
    echo -e "  速度: X=${GREEN}$LINEAR_X${NC}, Y=${GREEN}$LINEAR_Y${NC}, Z=${GREEN}$ANGULAR_Z${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
}

# 键盘控制模式
keyboard_control_mode() {
    echo -e "${GREEN}✓ 键盘控制模式已激活${NC}"
    echo -e "${YELLOW}等待键盘输入 (WSADQE: 速度控制, R: 重置, C: 状态, X: 退出)...${NC}"
    
    while true; do
        read_key key
        
        case $key in
            w|W)  # 增加前进速度
                LINEAR_X=$(echo "$LINEAR_X + $STEP" | bc 2>/dev/null || echo "$LINEAR_X")
                send_velocity $LINEAR_X $LINEAR_Y $ANGULAR_Z
                ;;
            s|S)  # 减少前进速度
                LINEAR_X=$(echo "$LINEAR_X - $STEP" | bc 2>/dev/null || echo "$LINEAR_X")
                send_velocity $LINEAR_X $LINEAR_Y $ANGULAR_Z
                ;;
            a|A)  # 增加左移速度
                LINEAR_Y=$(echo "$LINEAR_Y + $STEP" | bc 2>/dev/null || echo "$LINEAR_Y")
                send_velocity $LINEAR_X $LINEAR_Y $ANGULAR_Z
                ;;
            d|D)  # 减少左移速度
                LINEAR_Y=$(echo "$LINEAR_Y - $STEP" | bc 2>/dev/null || echo "$LINEAR_Y")
                send_velocity $LINEAR_X $LINEAR_Y $ANGULAR_Z
                ;;
            q|Q)  # 增加左转角速度
                ANGULAR_Z=$(echo "$ANGULAR_Z + $STEP" | bc 2>/dev/null || echo "$ANGULAR_Z")
                send_velocity $LINEAR_X $LINEAR_Y $ANGULAR_Z
                ;;
            e|E)  # 减少左转角速度
                ANGULAR_Z=$(echo "$ANGULAR_Z - $STEP" | bc 2>/dev/null || echo "$ANGULAR_Z")
                send_velocity $LINEAR_X $LINEAR_Y $ANGULAR_Z
                ;;
            r|R)  # 重置所有速度
                reset_velocity
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
    done
}

# 手动输入控制模式
manual_control_mode() {
    echo -e "${GREEN}✓ 手动输入控制模式已激活${NC}"
    echo -e "${YELLOW}请选择要控制的速度轴 (1: X, 2: Y, 3: Z, 4: XYZ, R: 重置, C: 状态, X: 退出)...${NC}"
    
    while true; do
        read_key key
        
        case $key in
            1)  # 输入X方向速度
                input_linear_x
                ;;
            2)  # 输入Y方向速度
                input_linear_y
                ;;
            3)  # 输入Z方向角速度
                input_angular_z
                ;;
            4)  # 输入三轴速度
                input_all_velocity
                ;;
            r|R)  # 重置所有速度
                reset_velocity
                ;;
            c|C)  # 显示状态
                show_status
                ;;
            x|X)  # 退出
                safe_exit
                ;;
            *)
                if [ -n "$key" ]; then
                    echo -e "${YELLOW}无效按键，按 1-4/R/C/X 选择功能${NC}"
                fi
                ;;
        esac
    done
}

# 主控制函数
control_loop() {
    show_instructions
    
    # 显示走斜坡提示
    show_slope_warning
    
    # 脚本启动后自动切换到走斜坡步态
    echo -e "${GREEN}脚本启动，切换到走斜坡模式...${NC}"
    send_slope_gait 3 "切换到走斜坡模式"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ 走斜坡模式已启动，现在可以使用$INPUT_MODE模式控制机器人上斜坡${NC}"
    else
        echo -e "${RED}✗ 启动走斜坡模式失败，继续控制程序${NC}"
    fi
    
    echo ""
    
    # 根据选择的控制模式进入不同的控制循环
    if [ "$INPUT_MODE" = "keyboard" ]; then
        keyboard_control_mode
    elif [ "$INPUT_MODE" = "manual" ]; then
        manual_control_mode
    else
        echo -e "${RED}✗ 未知的控制模式: $INPUT_MODE${NC}"
        echo -e "${YELLOW}使用默认键盘控制模式${NC}"
        keyboard_control_mode
    fi
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出走斜坡控制程序${NC}"
    
    # 标记清理已被调用
    CLEANUP_CALLED=true
    
    # 重置速度
    echo -e "${YELLOW}重置机器人速度为0...${NC}"
    send_velocity 0.0 0.0 0.0
    
    # 切换到停止步态
    echo -e "${YELLOW}切换到停止步态...${NC}"
    send_slope_gait 2 "安全停止"
    
    # 等待一小会儿确保消息发送完成
    sleep 0.5
    
    exit 0
}

# 确保退出时停止机器人
cleanup() {
    # 如果已经通过safe_exit退出，则不再重复清理
    if [ "$CLEANUP_CALLED" = true ]; then
        exit 0
    fi
    
    # 防止多次调用
    CLEANUP_CALLED=true
    
    # 发送停止消息
    echo -e "${RED}程序退出，发送停止命令...${NC}"
    send_velocity 0.0 0.0 0.0
    send_slope_gait 2 "安全停止"
    echo -e "${RED}已停止走斜坡控制并退出${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}机器人走斜坡控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制模式: ${PURPLE}$INPUT_MODE${NC}"
echo -e "速度控制话题: ${YELLOW}$TOPIC${NC}"
echo -e "控制模式服务: ${YELLOW}$CONTROL_TOPIC${NC}"
echo -e "最大线速度: ${GREEN}$MAX_SPEED${NC} m/s"
echo -e "速度增量: ${GREEN}$STEP${NC} m/s"
echo -e "控制模式: ${GREEN}走斜坡 (模式3)${NC}"
echo ""

control_loop
