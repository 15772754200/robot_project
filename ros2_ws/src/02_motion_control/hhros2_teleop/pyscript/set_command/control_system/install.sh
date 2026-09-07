#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 运控主板运动管理系统 v1.5
# 功能：多模式机器人运动控制系统
# 开发者：王崇超

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
WHITE='\033[1;37m'
NC='\033[0m' # 无颜色

# 控制模式服务
CONTROL_TOPIC="/hhros2_core/set_control_mode"
VELOCITY_TOPIC="/cmd_vel"

# 全局变量
CURRENT_MODE=0
CURRENT_VEL_X=0.0
CURRENT_VEL_Y=0.0
CURRENT_VEL_Z=0.0
MAX_SPEED=2.0
STEP=0.1
SYSTEM_RUNNING=true
DEVELOPER="王崇超"
VERSION="v1.5"

# 日志相关变量
LOG_DIR="./run_logs/robot_control"
LOG_FILE=""
LOG_ENABLED=true
LOG_LEVEL="INFO"  # DEBUG, INFO, WARN, ERROR

# 控制模式定义
MODE_STAND=2
MODE_WALK=3
MODE_RUN=4
# 当前框架没有独立跳跃模式，禁止将旧模式4误当作跳跃模式。
MODE_JUMP=-1
MODE_IMITATE=6
MODE_STOP_STAND=2
MODE_PREPARE=0

# 显示系统标题
show_title() {
    clear
    echo ""
    echo -e "${PURPLE}运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
}

# 初始化日志系统
init_log_system() {
    # 创建日志目录
    mkdir -p "$LOG_DIR"
    
    # 设置日志文件名
    local timestamp=$(date '+%Y%m%d_%H%M%S')
    LOG_FILE="${LOG_DIR}/robot_control_${timestamp}.log"
    
    # 写入日志头
    log_info "=========================================="
    log_info "机器人运动控制系统 v1.5"
    log_info "开始时间: $(date '+%Y-%m-%d %H:%M:%S')"
    log_info "开发者: $DEVELOPER"
    log_info "日志级别: $LOG_LEVEL"
    log_info "日志文件: $LOG_FILE"
    log_info "=========================================="
    log_info "系统初始化完成"
}

# 日志记录函数
log_message() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    if [ "$LOG_ENABLED" = true ]; then
        # 检查日志级别
        case $LOG_LEVEL in
            "DEBUG")
                echo "[$timestamp] [$level] $message" >> "$LOG_FILE"
                ;;
            "INFO")
                if [[ "$level" != "DEBUG" ]]; then
                    echo "[$timestamp] [$level] $message" >> "$LOG_FILE"
                fi
                ;;
            "WARN")
                if [[ "$level" != "DEBUG" && "$level" != "INFO" ]]; then
                    echo "[$timestamp] [$level] $message" >> "$LOG_FILE"
                fi
                ;;
            "ERROR")
                if [[ "$level" == "ERROR" ]]; then
                    echo "[$timestamp] [$level] $message" >> "$LOG_FILE"
                fi
                ;;
        esac
    fi
}

# 不同级别的日志函数
log_debug() {
    log_message "DEBUG" "$1"
}

log_info() {
    log_message "INFO" "$1"
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_warn() {
    log_message "WARN" "$1"
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    log_message "ERROR" "$1"
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示主菜单
show_main_menu() {
    show_title
    echo -e "${WHITE}主菜单:${NC}"
    echo -e "  ${GREEN}1.${NC} 控制运动模式"
    echo -e "  ${GREEN}2.${NC} 控制跳跃"
    echo -e "  ${GREEN}3.${NC} 控制跑步"
    echo -e "  ${GREEN}4.${NC} 控制站立"
    echo -e "  ${GREEN}5.${NC} 控制行走"
    echo -e "  ${GREEN}6.${NC} 控制爬坡"
    echo -e "  ${GREEN}7.${NC} 控制停止行走"
    echo -e "  ${GREEN}8.${NC} 控制停止爬坡"
    echo -e "  ${GREEN}9.${NC} 控制停止跳跃"
    echo -e "  ${GREEN}10.${NC} 控制停止跑步"
    echo -e "  ${GREEN}11.${NC} 控制停止站立"
    echo -e "  ${GREEN}12.${NC} 查看当前状态"
    echo -e "  ${GREEN}13.${NC} 查看日志文件"
    echo -e "  ${GREEN}14.${NC} 导出日志报告"
    echo -e "  ${GREEN}0.${NC} 退出系统"
    echo ""
}

let_robot_down() {
    clear
    echo ""
   
    echo -e "${YELLOW}重要安全提示：${NC}"
    echo "请确保："
    echo "1. 将机器人平稳地放置在地面上"
    echo "2. 牵引绳不要拖住或绊住机器人"
    echo "3. 确保机器人周围有足够的活动空间"
    echo ""
    
    read_line confirm "机器人是否已正确放置在地面上？[y/N]: "
    
    if [[ $confirm != "y" && $confirm != "Y" ]]; then
        echo -e "${RED}操作已取消。请先将机器人正确放置在地面上。${NC}"
        echo "按任意键返回..."
        read_key _terminal_key
        safe_exit
    fi
    
    echo -e "${GREEN}确认完成，继续执行后续操作...${NC}"
    echo ""
}

# 询问是否为第一次启动的函数
check_first_start() {
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          首次启动检查${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    echo -e "${YELLOW}请确认机器人是否第一次启动:${NC}"
    echo -e "${CYAN}这是机器人启动后的第一次运行吗？${NC}"
    echo ""
    echo -e "${GREEN}1.${NC} 是第一次启动（需要进入准备姿态）"
    echo -e "${RED}0.${NC} 不是第一次启动（已处于准备姿态）"
    echo ""
    
    while true; do
        read_line first_start "请选择 [0-1]: "
        
        case $first_start in
            1)  # 第一次启动，发布两次0进入准备姿态
                echo -e "\n${YELLOW}正在进入准备姿态...${NC}"
                log_info "机器人首次启动，开始进入准备姿态"
                
                echo -e "${GREEN}步骤1:${NC} 发送第一次准备姿态命令(0)..."
                send_control_mode $MODE_PREPARE "第一次准备姿态"
                log_info "发送准备姿态命令(模式: 0) - 第一次"
                
                echo -e "${GREEN}步骤2:${NC} 等待0.2秒..."
                sleep 0.2
                
                echo -e "${GREEN}步骤3:${NC} 发送第二次准备姿态命令(0)..."
                send_control_mode $MODE_PREPARE "第二次准备姿态"
                log_info "发送准备姿态命令(模式: 0) - 第二次"
                
                echo -e "\n${GREEN}✓ 机器人已进入准备姿态${NC}"
                log_info "准备姿态完成"
                sleep 1
                let_robot_down
                return
                ;;
            0)  # 不是第一次启动，直接继续
                echo -e "\n${YELLOW}跳过准备姿态，直接进入系统...${NC}"
                log_info "非首次启动，跳过准备姿态"
                return
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                ;;
        esac
    done
}

# 发送控制模式命令
send_control_mode() {
    local mode=$1
    local description=$2

    if [[ "$mode" -eq "$MODE_JUMP" ]]; then
        log_error "当前框架没有独立跳跃控制模式，拒绝发送: $description"
        echo -e "${RED}✗ 当前框架不支持独立跳跃模式，未发送控制命令${NC}"
        return 1
    fi
    
    # 发送 ROS2 消息
    local response
    response="$(ros2 service call "$CONTROL_TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: $mode}" 2>&1)"
    printf '%s\n' "$response"
    
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        CURRENT_MODE=$mode
        
        # 记录日志
        local mode_description=""
        case $mode in
            3) mode_description="行走模式";;
            2) mode_description="站立模式";;
            4) mode_description="跑步模式";;
            6) mode_description="模仿模式";;
            0) mode_description="准备姿态模式";;
            *) mode_description="模式 $mode";;
        esac
        
        log_info "发送控制模式命令: 模式=$mode ($mode_description), 描述=$description"
        
        # 显示消息
        case $mode in
            3) echo -e "${GREEN}✓ 已发送命令: 行走模式${NC}" ;;
            2) echo -e "${GREEN}✓ 已发送命令: 站立模式${NC}" ;;
            4) echo -e "${GREEN}✓ 已发送命令: 跑步模式${NC}" ;;
            6) echo -e "${GREEN}✓ 已发送命令: 模仿模式${NC}" ;;
            0) echo -e "${GREEN}✓ 已发送命令: 准备姿态模式${NC}" ;;
            *) echo -e "${BLUE}✓ 已发送命令: 模式 $mode${NC}" ;;
        esac
        
        if [[ -n "$description" ]]; then
            echo -e "说明: $description${NC}"
        fi
        return 0
    else
        log_error "发送控制模式命令失败: 模式=$mode, 描述=$description"
        echo -e "${RED}✗ 发送命令失败${NC}"
        return 1
    fi
}

# 发送速度命令
send_velocity() {
    local linear_x=$1
    local linear_y=$2
    local angular_z=$3
    
    # 记录原始速度
    local original_x=$CURRENT_VEL_X
    local original_y=$CURRENT_VEL_Y
    local original_z=$CURRENT_VEL_Z
    
    # 限制速度范围
    if (( $(echo "$linear_x > $MAX_SPEED" | bc -l) )); then
        log_warn "X方向速度超出最大值: $linear_x -> $MAX_SPEED"
        linear_x=$MAX_SPEED
    elif (( $(echo "$linear_x < -$MAX_SPEED" | bc -l) )); then
        log_warn "X方向速度超出最小值: $linear_x -> -$MAX_SPEED"
        linear_x=-$MAX_SPEED
    fi
    
    if (( $(echo "$linear_y > $MAX_SPEED" | bc -l) )); then
        log_warn "Y方向速度超出最大值: $linear_y -> $MAX_SPEED"
        linear_y=$MAX_SPEED
    elif (( $(echo "$linear_y < -$MAX_SPEED" | bc -l) )); then
        log_warn "Y方向速度超出最小值: $linear_y -> -$MAX_SPEED"
        linear_y=-$MAX_SPEED
    fi
    
    if (( $(echo "$angular_z > $MAX_SPEED" | bc -l) )); then
        log_warn "Z方向角速度超出最大值: $angular_z -> $MAX_SPEED"
        angular_z=$MAX_SPEED
    elif (( $(echo "$angular_z < -$MAX_SPEED" | bc -l) )); then
        log_warn "Z方向角速度超出最小值: $angular_z -> -$MAX_SPEED"
        angular_z=-$MAX_SPEED
    fi
    
    # 更新全局变量
    CURRENT_VEL_X=$linear_x
    CURRENT_VEL_Y=$linear_y
    CURRENT_VEL_Z=$angular_z
    
    # 发送 ROS2 消息
    ros2 topic pub -1 $VELOCITY_TOPIC geometry_msgs/msg/Twist "
    linear:
      x: $CURRENT_VEL_X
      y: $CURRENT_VEL_Y
      z: 0.0
    angular:
      x: 0.0
      y: 0.0
      z: $CURRENT_VEL_Z
    " 2>/dev/null
    
    if [ $? -eq 0 ]; then
        # 记录速度变化
        log_info "发送速度命令: X=$CURRENT_VEL_X m/s, Y=$CURRENT_VEL_Y m/s, Z=$CURRENT_VEL_Z rad/s (原始: X=$original_x, Y=$original_y, Z=$original_z)"
        
        # 获取方向描述
        local direction_desc=""
        if (( $(echo "$CURRENT_VEL_X > 0" | bc -l) )); then
            direction_desc="${direction_desc}前进,"
        elif (( $(echo "$CURRENT_VEL_X < 0" | bc -l) )); then
            direction_desc="${direction_desc}后退,"
        fi
        
        if (( $(echo "$CURRENT_VEL_Y > 0" | bc -l) )); then
            direction_desc="${direction_desc}左移,"
        elif (( $(echo "$CURRENT_VEL_Y < 0" | bc -l) )); then
            direction_desc="${direction_desc}右移,"
        fi
        
        if (( $(echo "$CURRENT_VEL_Z > 0" | bc -l) )); then
            direction_desc="${direction_desc}左转,"
        elif (( $(echo "$CURRENT_VEL_Z < 0" | bc -l) )); then
            direction_desc="${direction_desc}右转,"
        fi
        
        if [ -z "$direction_desc" ]; then
            direction_desc="停止"
        else
            direction_desc=${direction_desc%,}  # 移除最后一个逗号
        fi
        
        log_debug "速度方向: $direction_desc"
        
        # 显示消息
        echo -e "${GREEN}✓ 已发送速度: X=${CURRENT_VEL_X} m/s, Y=${CURRENT_VEL_Y} m/s, Z=${CURRENT_VEL_Z} rad/s${NC}"
    else
        log_error "发送速度命令失败: X=$linear_x, Y=$linear_y, Z=$angular_z"
        echo -e "${RED}✗ 发送速度失败${NC}"
        return 1
    fi
    return 0
}

# 发送两次站立命令
send_stand_twice() {
    log_info "开始切换到站立模式"
    echo -e "${YELLOW}正在切换到站立模式...${NC}"
    send_control_mode $MODE_STAND "准备切换运动模式"
    sleep 0.1
    send_control_mode $MODE_STAND "确认站立状态"
    log_info "站立模式切换完成"
    return 0
}

# 退出控制模式时的安全返回
safe_exit_to_stand() {
    log_info "开始安全退出控制模式"
    echo -e "${YELLOW}正在安全退出控制模式...${NC}"
    echo -e "${YELLOW}步骤1: 重置所有速度为0${NC}"
    send_velocity 0.0 0.0 0.0
    sleep 0.2
    
    echo -e "${YELLOW}步骤2: 切换到站立模式${NC}"
    send_stand_twice
    sleep 0.2
    
    echo -e "${GREEN}✓ 已安全退出控制模式${NC}"
    log_info "安全退出控制模式完成"
    return 0
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
                    if ! [[ $MAX_SPEED =~ ^[0-9]+\.?[0-9]*$ ]] || (( $(echo "$MAX_SPEED <= 0" | bc -l) )); then
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
                    if ! [[ $STEP =~ ^[0-9]+\.?[0-9]*$ ]] || (( $(echo "$STEP <= 0" | bc -l) )); then
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

# 显示帮助信息
show_help() {
    echo "机器人运动控制系统 v1.5"
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -t, --topic TOPIC       设置速度控制话题 (默认: /cmd_vel)"
    echo "  -c, --ctrl SERVICE      设置控制模式服务 (默认: /hhros2_core/set_control_mode)"
    echo "  -l, --linear SPEED      设置最大线速度 (默认: 2.0)"
    echo "  -s, --step STEP         设置速度增量 (默认: 0.1)"
    echo "  -m, --mode MODE         设置控制模式 (keyboard/manual)"
    echo "  -h, --help              显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                        # 正常启动"
    echo "  $0 -l 1.5 -s 0.2          # 设置最大速度1.5，增量0.2"
    echo ""
}

# 显示当前状态
show_current_status() {
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          系统当前状态${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    # 记录状态查看
    log_info "查看当前状态"
    
    # 显示当前模式
    echo -e "当前控制模式: ${GREEN}$CURRENT_MODE${NC}"
    case $CURRENT_MODE in
        0) echo -e "模式描述: ${YELLOW}被动/准备模式${NC}" ;;
        2) echo -e "模式描述: ${GREEN}站立模式${NC}" ;;
        3) echo -e "模式描述: ${GREEN}行走模式${NC}" ;;
        4) echo -e "模式描述: ${GREEN}跑步模式${NC}" ;;
        5) echo -e "模式描述: ${GREEN}WBC模式${NC}" ;;
        6) echo -e "模式描述: ${GREEN}模仿模式${NC}" ;;
        *) echo -e "模式描述: ${BLUE}未知模式${NC}" ;;
    esac
    
    # 显示当前速度
    echo ""
    echo -e "当前速度状态:"
    echo -e "  X方向(前进/后退): ${GREEN}$CURRENT_VEL_X${NC} m/s"
    echo -e "  Y方向(左移/右移): ${GREEN}$CURRENT_VEL_Y${NC} m/s"
    echo -e "  Z方向(左转/右转): ${GREEN}$CURRENT_VEL_Z${NC} rad/s"
    
    # 显示方向判断
    echo ""
    echo -e "方向判断:"
    if (( $(echo "$CURRENT_VEL_X > 0" | bc -l) )); then
        echo -e "  X方向: ${GREEN}前进${NC}"
    elif (( $(echo "$CURRENT_VEL_X < 0" | bc -l) )); then
        echo -e "  X方向: ${GREEN}后退${NC}"
    else
        echo -e "  X方向: ${YELLOW}停止${NC}"
    fi
    
    if (( $(echo "$CURRENT_VEL_Y > 0" | bc -l) )); then
        echo -e "  Y方向: ${GREEN}左移${NC}"
    elif (( $(echo "$CURRENT_VEL_Y < 0" | bc -l) )); then
        echo -e "  Y方向: ${GREEN}右移${NC}"
    else
        echo -e "  Y方向: ${YELLOW}停止${NC}"
    fi
    
    if (( $(echo "$CURRENT_VEL_Z > 0" | bc -l) )); then
        echo -e "  Z方向: ${GREEN}左转${NC}"
    elif (( $(echo "$CURRENT_VEL_Z < 0" | bc -l) )); then
        echo -e "  Z方向: ${GREEN}右转${NC}"
    else
        echo -e "  Z方向: ${YELLOW}停止${NC}"
    fi
    
    # 显示日志信息
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${CYAN}日志信息:${NC}"
    echo -e "  日志文件: ${GREEN}$LOG_FILE${NC}"
    echo -e "  日志级别: ${GREEN}$LOG_LEVEL${NC}"
    echo -e "  日志目录: ${GREEN}$LOG_DIR${NC}"
    
    # 显示当前日志文件大小
    if [ -f "$LOG_FILE" ]; then
        local log_size=$(stat -c%s "$LOG_FILE" 2>/dev/null || stat -f%z "$LOG_FILE" 2>/dev/null)
        if [ -n "$log_size" ]; then
            if [ "$log_size" -gt 1048576 ]; then
                local size_mb=$(echo "scale=2; $log_size / 1048576" | bc)
                echo -e "  日志大小: ${GREEN}${size_mb} MB${NC}"
            elif [ "$log_size" -gt 1024 ]; then
                local size_kb=$(echo "scale=2; $log_size / 1024" | bc)
                echo -e "  日志大小: ${GREEN}${size_kb} KB${NC}"
            else
                echo -e "  日志大小: ${GREEN}${log_size} bytes${NC}"
            fi
        fi
    fi
    
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
}

# 查看日志文件
view_log_files() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          日志文件管理${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    # 检查日志目录
    if [ ! -d "$LOG_DIR" ]; then
        echo -e "${RED}错误: 日志目录 $LOG_DIR 不存在${NC}"
        read_key_prompt _terminal_key "按任意键返回..."
        return
    fi
    
    # 获取日志文件列表
    local log_files=($(ls -1t "$LOG_DIR"/robot_control_*.log 2>/dev/null))
    local num_files=${#log_files[@]}
    
    if [ $num_files -eq 0 ]; then
        echo -e "${YELLOW}没有找到日志文件${NC}"
        echo -e "${CYAN}日志目录: $LOG_DIR${NC}"
        echo ""
        read_key_prompt _terminal_key "按任意键返回..."
        return
    fi
    
    echo -e "${GREEN}找到 $num_files 个日志文件:${NC}"
    echo ""
    
    for i in "${!log_files[@]}"; do
        local filename=$(basename "${log_files[$i]}")
        local timestamp=${filename#robot_control_}
        timestamp=${timestamp%.log}
        local display_time=$(echo "$timestamp" | sed 's/_/ /; s/_/:/; s/_/:/')
        
        # 获取文件大小
        local file_size=$(stat -c%s "${log_files[$i]}" 2>/dev/null || stat -f%z "${log_files[$i]}" 2>/dev/null)
        if [ -n "$file_size" ]; then
            if [ "$file_size" -gt 1048576 ]; then
                local size_display=$(echo "scale=1; $file_size / 1048576" | bc)"MB"
            elif [ "$file_size" -gt 1024 ]; then
                local size_display=$(echo "scale=1; $file_size / 1024" | bc)"KB"
            else
                local size_display="${file_size}B"
            fi
        else
            local size_display="未知"
        fi
        
        echo -e "  ${GREEN}$((i+1)).${NC} ${log_files[$i]}"
        echo -e "      时间: $display_time, 大小: $size_display"
    done
    
    echo ""
    echo -e "${CYAN}请选择操作:${NC}"
    echo -e "  ${GREEN}1-${num_files}${NC} 查看对应日志文件"
    echo -e "  ${GREEN}c${NC} 查看当前日志文件 ($(basename "$LOG_FILE"))"
    echo -e "  ${GREEN}d${NC} 删除旧日志文件"
    echo -e "  ${GREEN}0${NC} 返回主菜单"
    echo ""
    
    read_line choice "请选择: "
    
    case $choice in
        [1-9]|[1-9][0-9])
            if [ $choice -ge 1 ] && [ $choice -le $num_files ]; then
                local selected_file="${log_files[$((choice-1))]}"
                view_single_log "$selected_file"
            else
                echo -e "${RED}无效的选择${NC}"
                read_key_prompt _terminal_key "按任意键继续..."
            fi
            ;;
        c|C)
            view_single_log "$LOG_FILE"
            ;;
        d|D)
            delete_old_logs
            ;;
        0)
            return
            ;;
        *)
            echo -e "${RED}无效的选择${NC}"
            read_key_prompt _terminal_key "按任意键继续..."
            ;;
    esac
}

# 查看单个日志文件
view_single_log() {
    local log_file="$1"
    
    if [ ! -f "$log_file" ]; then
        echo -e "${RED}错误: 日志文件不存在${NC}"
        read_key_prompt _terminal_key "按任意键继续..."
        return
    fi
    
    clear
    echo ""
    echo -e "${PURPLE}日志文件查看器${NC}"
    echo -e "${PURPLE}文件: $(basename "$log_file")${NC}"
    echo -e "${PURPLE}大小: $(du -h "$log_file" | cut -f1)${NC}"
    echo -e "${PURPLE}最后修改: $(stat -c "%y" "$log_file" 2>/dev/null || stat -f "%Sm" "$log_file" 2>/dev/null)${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    
    # 显示最后50行日志
    echo -e "${CYAN}=== 最后50行日志 ===${NC}"
    echo ""
    tail -n 50 "$log_file"
    
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    echo -e "${CYAN}请选择操作:${NC}"
    echo -e "  ${GREEN}f${NC} 查看完整日志"
    echo -e "  ${GREEN}s${NC} 搜索关键字"
    echo -e "  ${GREEN}e${NC} 导出为文本文件"
    echo -e "  ${GREEN}0${NC} 返回"
    echo ""
    
    read_line action "请选择: "
    
    case $action in
        f|F)
            clear
            echo -e "${CYAN}=== 完整日志内容 ===${NC}"
            echo ""
            cat "$log_file"
            echo ""
            read_key_prompt _terminal_key "按任意键继续..."
            ;;
        s|S)
            read_line keyword "请输入搜索关键字: "
            clear
            echo -e "${CYAN}=== 包含 '$keyword' 的日志行 ===${NC}"
            echo ""
            grep -i "$keyword" "$log_file" || echo -e "${YELLOW}未找到匹配项${NC}"
            echo ""
            read_key_prompt _terminal_key "按任意键继续..."
            ;;
        e|E)
            export_log_report "$log_file"
            ;;
        0)
            return
            ;;
        *)
            return
            ;;
    esac
}

# 导出日志报告
export_log_report() {
    local log_file="$1"
    local export_file="${log_file%.log}_report.txt"
    
    echo -e "\n${YELLOW}正在生成日志报告...${NC}"
    
    # 创建报告文件
    {
        echo "=== 机器人控制日志报告 ==="
        echo "生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "日志文件: $(basename "$log_file")"
        echo "系统版本: $VERSION"
        echo "开发者: $DEVELOPER"
        echo "======================================"
        echo ""
        echo "=== 摘要统计 ==="
        echo ""
        
        # 统计不同类型的日志
        local total_lines=$(wc -l < "$log_file")
        echo "总日志行数: $total_lines"
        
        local info_count=$(grep -c "\[INFO\]" "$log_file")
        local warn_count=$(grep -c "\[WARN\]" "$log_file")
        local error_count=$(grep -c "\[ERROR\]" "$log_file")
        local debug_count=$(grep -c "\[DEBUG\]" "$log_file")
        
        echo "信息日志: $info_count"
        echo "警告日志: $warn_count"
        echo "错误日志: $error_count"
        echo "调试日志: $debug_count"
        
        echo ""
        echo "=== 控制模式切换统计 ==="
        echo ""
        
        # 统计模式切换
        local mode_changes=$(grep "发送控制模式命令" "$log_file" | wc -l)
        echo "总模式切换次数: $mode_changes"
        
        # 统计各个模式
        for mode in 0 2 3 4 5 6; do
            case $mode in
                0) mode_name="准备姿态模式";;
                3) mode_name="行走模式";;
                2) mode_name="站立模式";;
                4) mode_name="跑步模式";;
                5) mode_name="WBC模式";;
                6) mode_name="模仿模式";;
                *) mode_name="未知模式($mode)";;
            esac
            
            local count=$(grep -c "发送控制模式命令: 模式=$mode" "$log_file")
            if [ $count -gt 0 ]; then
                echo "$mode_name: $count 次"
            fi
        done
        
        echo ""
        echo "=== 错误和警告列表 ==="
        echo ""
        
        # 提取所有错误和警告
        grep -E "\[ERROR\]|\[WARN\]" "$log_file" | head -20
        
        echo ""
        echo "=== 最近的20条操作 ==="
        echo ""
        
        # 显示最近20条操作
        tail -n 20 "$log_file"
        
        echo ""
        echo "=== 报告结束 ==="
        
    } > "$export_file"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ 日志报告已导出到: $export_file${NC}"
        log_info "导出日志报告: $export_file"
    else
        echo -e "${RED}✗ 导出日志报告失败${NC}"
        log_error "导出日志报告失败"
    fi
    
    read_key_prompt _terminal_key "按任意键继续..."
}

# 删除旧日志文件
delete_old_logs() {
    clear
    echo ""
    echo -e "${PURPLE}删除旧日志文件${NC}"
    echo ""
    echo -e "${RED}警告: 这将删除旧的日志文件${NC}"
    echo -e "${YELLOW}注意: 当前日志文件不会被删除${NC}"
    echo ""
    
    # 列出除当前文件外的所有日志文件
    local old_logs=($(ls -1t "$LOG_DIR"/robot_control_*.log 2>/dev/null | grep -v "$(basename "$LOG_FILE")"))
    local num_old_logs=${#old_logs[@]}
    
    if [ $num_old_logs -eq 0 ]; then
        echo -e "${YELLOW}没有找到旧的日志文件${NC}"
        read_key_prompt _terminal_key "按任意键返回..."
        return
    fi
    
    echo -e "${CYAN}找到 $num_old_logs 个旧的日志文件:${NC}"
    echo ""
    
    for i in "${!old_logs[@]}"; do
        local filename=$(basename "${old_logs[$i]}")
        local file_size=$(stat -c%s "${old_logs[$i]}" 2>/dev/null || stat -f%z "${old_logs[$i]}" 2>/dev/null)
        if [ -n "$file_size" ] && [ "$file_size" -gt 1048576 ]; then
            local size_display=$(echo "scale=1; $file_size / 1048576" | bc)"MB"
        elif [ -n "$file_size" ] && [ "$file_size" -gt 1024 ]; then
            local size_display=$(echo "scale=1; $file_size / 1024" | bc)"KB"
        else
            local size_display="${file_size}B"
        fi
        
        echo -e "  ${GREEN}$((i+1)).${NC} ${old_logs[$i]} ($size_display)"
    done
    
    echo ""
    read_line confirm "确认删除所有旧的日志文件? (y/N): "
    
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
        local total_size=0
        local deleted_count=0
        
        for log_file in "${old_logs[@]}"; do
            local file_size=$(stat -c%s "$log_file" 2>/dev/null || stat -f%z "$log_file" 2>/dev/null)
            if rm -f "$log_file"; then
                echo -e "${GREEN}✓ 已删除: $(basename "$log_file")${NC}"
                total_size=$((total_size + file_size))
                deleted_count=$((deleted_count + 1))
            else
                echo -e "${RED}✗ 删除失败: $(basename "$log_file")${NC}"
            fi
        done
        
        echo ""
        if [ $total_size -gt 1048576 ]; then
            local freed_space=$(echo "scale=2; $total_size / 1048576" | bc)
            echo -e "${GREEN}已释放 ${freed_space} MB 磁盘空间${NC}"
        elif [ $total_size -gt 1024 ]; then
            local freed_space=$(echo "scale=2; $total_size / 1024" | bc)
            echo -e "${GREEN}已释放 ${freed_space} KB 磁盘空间${NC}"
        else
            echo -e "${GREEN}已释放 ${total_size} bytes 磁盘空间${NC}"
        fi
        
        log_info "删除 $deleted_count 个旧日志文件，释放 ${total_size} bytes 空间"
    else
        echo -e "${YELLOW}操作已取消${NC}"
    fi
    
    read_key_prompt _terminal_key "按任意键继续..."
}

safe_exit() {
    echo -e "${RED}退出系统程序${NC}"
    
    # 记录系统退出
    log_info "系统退出"
    log_info "=========================================="
    log_info "机器人运动控制系统结束运行"
    log_info "结束时间: $(date '+%Y-%m-%d %H:%M:%S')"
    log_info "=========================================="
    
    # 标记清理已被调用
    CLEANUP_CALLED=true
    
    exit 0
}

# 键盘控制循环
keyboard_control_loop() {
    local mode_name=$1
    local additional_prompt=$2
    
    # 记录进入控制模式
    log_info "进入 $mode_name 控制模式"
    
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          $mode_name 控制模式${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    if [[ -n "$additional_prompt" ]]; then
        echo -e "${YELLOW}提示: $additional_prompt${NC}"
        echo ""
    fi
    
    echo -e "${CYAN}键盘控制命令:${NC}"
    echo -e "  ${GREEN}W${NC}: 增加前进速度 (+X方向)"
    echo -e "  ${GREEN}S${NC}: 减少前进速度 (-X方向)"
    echo -e "  ${GREEN}A${NC}: 增加左移速度 (+Y方向)"
    echo -e "  ${GREEN}D${NC}: 减少左移速度 (-Y方向)"
    echo -e "  ${GREEN}Q${NC}: 增加左转角速度 (+Z方向)"
    echo -e "  ${GREEN}E${NC}: 减少左转角速度 (-Z方向)"
    echo -e "  ${GREEN}R${NC}: 重置所有速度为0"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${GREEN}L${NC}: 查看当前日志"
    echo -e "  ${RED}X${NC}: 返回主菜单(安全退出)"
    echo ""
    echo -e "当前速度: X=${GREEN}$CURRENT_VEL_X${NC}, Y=${GREEN}$CURRENT_VEL_Y${NC}, Z=${GREEN}$CURRENT_VEL_Z${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    echo -e "${YELLOW}等待键盘输入...${NC}"
    
    while true; do
        read_key key
        
        case $key in
            w|W)  # 增加前进速度
                log_debug "键盘输入: W - 增加前进速度"
                CURRENT_VEL_X=$(echo "$CURRENT_VEL_X + $STEP" | bc)
                send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                ;;
            s|S)  # 减少前进速度
                log_debug "键盘输入: S - 减少前进速度"
                CURRENT_VEL_X=$(echo "$CURRENT_VEL_X - $STEP" | bc)
                send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                ;;
            a|A)  # 增加左移速度
                log_debug "键盘输入: A - 增加左移速度"
                CURRENT_VEL_Y=$(echo "$CURRENT_VEL_Y + $STEP" | bc)
                send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                ;;
            d|D)  # 减少左移速度
                log_debug "键盘输入: D - 减少左移速度"
                CURRENT_VEL_Y=$(echo "$CURRENT_VEL_Y - $STEP" | bc)
                send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                ;;
            q|Q)  # 增加左转角速度
                log_debug "键盘输入: Q - 增加左转角速度"
                CURRENT_VEL_Z=$(echo "$CURRENT_VEL_Z + $STEP" | bc)
                send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                ;;
            e|E)  # 减少左转角速度
                log_debug "键盘输入: E - 减少左转角速度"
                CURRENT_VEL_Z=$(echo "$CURRENT_VEL_Z - $STEP" | bc)
                send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                ;;
            r|R)  # 重置所有速度
                log_debug "键盘输入: R - 重置所有速度"
                CURRENT_VEL_X=0.0
                CURRENT_VEL_Y=0.0
                CURRENT_VEL_Z=0.0
                send_velocity 0.0 0.0 0.0
                echo -e "${YELLOW}✓ 已重置所有速度为0${NC}"
                ;;
            c|C)  # 显示状态
                log_debug "键盘输入: C - 显示状态"
                echo ""
                echo -e "当前速度: X=${GREEN}$CURRENT_VEL_X${NC}, Y=${GREEN}$CURRENT_VEL_Y${NC}, Z=${GREEN}$CURRENT_VEL_Z${NC}"
                ;;
            l|L)  # 查看当前日志
                log_debug "键盘输入: L - 查看当前日志"
                view_single_log "$LOG_FILE"
                # 重新显示控制界面
                keyboard_control_loop "$mode_name" "$additional_prompt"
                return
                ;;
            x|X)  # 返回主菜单，安全退出
                log_debug "键盘输入: X - 退出 $mode_name 控制模式"
                echo -e "\n${YELLOW}正在退出 $mode_name 控制...${NC}"
                # 安全退出：停止运动并切换到站立模式
                safe_exit_to_stand
                log_info "退出 $mode_name 控制模式"
                return
                ;;
            *)
                if [ -n "$key" ]; then
                        echo -e "${YELLOW}无效按键，按 C 查看状态，L 查看日志，X 返回主菜单${NC}"
                fi
                ;;
        esac
    done
}

# 1. 控制运动模式
control_motion_mode() {
    while true; do
        clear
        echo ""
        echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
        echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
        echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
        echo ""
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${WHITE}          运动模式选择${NC}"
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${CYAN}请选择运动模式:${NC}"
        echo -e "  ${GREEN}1.${NC} 行走模式"
        echo -e "  ${GREEN}2.${NC} 站立模式"
        echo -e "  ${GREEN}3.${NC} 跑步模式"
        echo -e "  ${GREEN}4.${NC} 跳跃模式（当前框架不支持）"
        echo -e "  ${GREEN}5.${NC} 模仿模式"
        echo -e "  ${GREEN}0.${NC} 返回主菜单"
        echo ""
        echo -e "${YELLOW}注意: 选择1、3、5时将先发送站立命令(2)，延迟1秒后再发送相应模式；选择4不会发送命令${NC}"
        echo ""
        read_line choice "请选择操作 [0-5]: "
        
        case $choice in
            1)  # 行走模式
                log_info "选择运动模式: 行走模式"
                echo -e "\n${YELLOW}正在切换到行走模式...${NC}"
                echo -e "${YELLOW}步骤1: 发送站立命令...${NC}"
                send_stand_twice
                echo -e "${YELLOW}步骤2: 延迟1秒...${NC}"
                sleep 1
                echo -e "${YELLOW}步骤3: 发送行走模式命令...${NC}"
                send_control_mode $MODE_WALK "进入行走模式"
                keyboard_control_loop "行走"
                ;;
            2)  # 站立模式
                log_info "选择运动模式: 站立模式"
                echo -e "\n${YELLOW}正在切换到站立模式...${NC}"
                send_control_mode $MODE_STAND "进入站立模式"
                sleep 0.5
                send_control_mode $MODE_STAND "确认站立状态"
                echo -e "${GREEN}✓ 已进入站立模式${NC}"
                read_key_prompt _terminal_key "按任意键继续..."
                # 返回前不需要再次发送站立命令，因为已经是站立状态
                ;;
            3)  # 跑步模式
                log_info "选择运动模式: 跑步模式"
                echo -e "\n${YELLOW}正在切换到跑步模式...${NC}"
                echo -e "${YELLOW}步骤1: 发送站立命令...${NC}"
                send_stand_twice
                echo -e "${YELLOW}步骤2: 延迟1秒...${NC}"
                sleep 1
                echo -e "${YELLOW}步骤3: 发送跑步模式命令...${NC}"
                send_control_mode $MODE_RUN "进入跑步模式"
                keyboard_control_loop "跑步"
                ;;
            4)  # 跳跃模式
                log_warn "当前框架没有独立跳跃控制模式"
                echo -e "\n${RED}当前框架不支持独立跳跃模式，未发送控制命令${NC}"
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            5)  # 模仿模式
                log_info "选择运动模式: 模仿模式"
                echo -e "\n${YELLOW}正在切换到模仿模式...${NC}"
                echo -e "${YELLOW}步骤1: 发送站立命令...${NC}"
                send_stand_twice
                echo -e "${YELLOW}步骤2: 延迟1秒...${NC}"
                sleep 1
                echo -e "${YELLOW}步骤3: 发送模仿模式命令...${NC}"
                send_control_mode $MODE_IMITATE "进入模仿模式"
                echo -e "${GREEN}✓ 已进入模仿模式${NC}"
                echo -e "${YELLOW}注意: 模仿模式已激活${NC}"
                read_key_prompt _terminal_key "按任意键继续..."
                # 返回前安全退出到站立模式
                echo -e "${YELLOW}正在安全退出模仿模式...${NC}"
                sleep 1
                safe_exit_to_stand
                ;;
            0)  # 返回主菜单
                log_info "返回主菜单"
                echo -e "\n${YELLOW}正在返回主菜单...${NC}"
                sleep 1
                safe_exit_to_stand
                return
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 2. 控制跳跃
control_jump() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          跳跃控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制跳跃功能"
    
    echo -e "${RED}当前框架不支持独立跳跃模式，未发送控制命令${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 返回前安全退出到站立模式
    echo -e "${YELLOW}正在安全退出跳跃控制...${NC}"
    safe_exit_to_stand
}

# 3. 控制跑步
control_run() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          跑步控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制跑步功能"
    
    echo -e "${YELLOW}正在执行跑步控制...${NC}"
    echo -e "${YELLOW}步骤1: 发送站立命令...${NC}"
    send_stand_twice
    echo -e "${YELLOW}步骤2: 延迟1秒...${NC}"
    sleep 1
    echo -e "${YELLOW}步骤3: 发送跑步模式命令...${NC}"
    send_control_mode $MODE_RUN "进入跑步模式"
    keyboard_control_loop "跑步"
    # keyboard_control_loop返回时会自动调用safe_exit_to_stand
}

# 4. 控制站立
control_stand() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          站立控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制站立功能"
    
    echo -e "${YELLOW}正在执行站立控制...${NC}"
    send_stand_twice
    echo -e "${GREEN}✓ 站立命令已发送${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 已经是站立状态，不需要再次发送站立命令
}

# 5. 控制行走
control_walk() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          行走控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制行走功能"
    
    echo -e "${YELLOW}正在执行行走控制...${NC}"
    echo -e "${YELLOW}步骤1: 发送站立命令...${NC}"
    send_stand_twice
    echo -e "${YELLOW}步骤2: 延迟1秒...${NC}"
    sleep 1
    echo -e "${YELLOW}步骤3: 发送行走模式命令...${NC}"
    send_control_mode $MODE_WALK "进入行走模式"
    keyboard_control_loop "行走"
    # keyboard_control_loop返回时会自动调用safe_exit_to_stand
}

# 6. 控制爬坡
control_climb() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          爬坡控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${CYAN}重要提示: 请确保机器人对准斜坡中心${NC}"
    echo -e "${CYAN}          保持机器人重心稳定${NC}"
    echo ""
    
    log_info "执行控制爬坡功能"
    
    echo -e "${YELLOW}正在执行爬坡控制...${NC}"
    echo -e "${YELLOW}步骤1: 发送站立命令...${NC}"
    send_stand_twice
    echo -e "${YELLOW}步骤2: 延迟1秒...${NC}"
    sleep 1
    echo -e "${YELLOW}步骤3: 发送行走模式命令...${NC}"
    send_control_mode $MODE_WALK "进入爬坡模式"
    keyboard_control_loop "爬坡" "请确保机器人对准斜坡"
    # keyboard_control_loop返回时会自动调用safe_exit_to_stand
}

# 7. 控制停止行走
control_stop_walk() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          停止行走控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制停止行走功能"
    
    echo -e "${YELLOW}正在停止行走...${NC}"
    send_stand_twice
    echo -e "${GREEN}✓ 行走已停止${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 已经是站立状态，不需要再次发送站立命令
}

# 8. 控制停止爬坡
control_stop_climb() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          停止爬坡控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制停止爬坡功能"
    
    echo -e "${YELLOW}正在停止爬坡...${NC}"
    send_stand_twice
    echo -e "${GREEN}✓ 爬坡已停止${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 已经是站立状态，不需要再次发送站立命令
}

# 9. 控制停止跳跃
control_stop_jump() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          停止跳跃控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${RED}警告: 停止跳跃控制${NC}"
    echo -e "${YELLOW}注意: 请勿在机器人跳跃过程中执行此操作${NC}"
    echo -e "${YELLOW}      确保机器人已安全落地${NC}"
    echo ""
    
    log_info "执行控制停止跳跃功能"
    
    read_line confirm "确认机器人已安全落地? (y/n): "
    
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
        echo -e "${YELLOW}正在停止跳跃...${NC}"
        send_stand_twice
        echo -e "${GREEN}✓ 跳跃已停止${NC}"
    else
        echo -e "${RED}操作已取消${NC}"
    fi
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 已经是站立状态，不需要再次发送站立命令
}

# 10. 控制停止跑步
control_stop_run() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          停止跑步控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    log_info "执行控制停止跑步功能"
    
    echo -e "${YELLOW}正在停止跑步...${NC}"
    send_stand_twice
    echo -e "${GREEN}✓ 跑步已停止${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 已经是站立状态，不需要再次发送站立命令
}

# 11. 控制停止站立
control_stop_stand() {
    clear
    echo ""
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          停止站立控制${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${RED}警告: 停止站立控制${NC}"
    echo -e "${YELLOW}重要安全提示:${NC}"
    echo -e "${YELLOW}1. 确保机器人已被安全固定${NC}"
    echo -e "${YELLOW}2. 确保机器人已挂好安全绳${NC}"
    echo -e "${YELLOW}3. 确保周围没有障碍物${NC}"
    echo ""
    
    log_info "执行控制停止站立功能"
    
    read_line confirm "确认已采取安全措施? (y/n): "
    
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
        echo -e "${YELLOW}正在切换到站立模式...${NC}"
        send_control_mode $MODE_STOP_STAND "切换到站立模式 - 第一次"
        sleep 0.1
        send_control_mode $MODE_STOP_STAND "切换到站立模式 - 第二次"
        echo -e "${GREEN}✓ 已切换到站立模式${NC}"
    else
        echo -e "${RED}操作已取消${NC}"
    fi
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
    # 发送停止站立命令后，不需要再发送站立命令
}

# 12. 查看当前状态
show_current_status_menu() {
    show_title
    show_current_status
}

# 13. 查看日志文件
show_log_files() {
    view_log_files
}

# 14. 导出日志报告
export_log_report_menu() {
    if [ -f "$LOG_FILE" ]; then
        export_log_report "$LOG_FILE"
    else
        echo -e "${RED}当前日志文件不存在${NC}"
        echo -e "${YELLOW}请先操作以生成日志${NC}"
        read_key_prompt _terminal_key "按任意键返回..."
    fi
}

# 主循环
main_loop() {
    while $SYSTEM_RUNNING; do
        show_main_menu
        
        echo -n -e "${WHITE}请选择操作[0-14]: ${NC}"
        read_line choice ""
        
        # 如果没有输入，则重新显示菜单
        if [[ -z "$choice" ]]; then
            continue
        fi
        
        case $choice in
            1) 
                log_info "用户选择: 控制运动模式"
                control_motion_mode 
                ;;
            2) 
                log_info "用户选择: 控制跳跃"
                control_jump 
                ;;
            3) 
                log_info "用户选择: 控制跑步"
                control_run 
                ;;
            4) 
                log_info "用户选择: 控制站立"
                control_stand 
                ;;
            5) 
                log_info "用户选择: 控制行走"
                control_walk 
                ;;
            6) 
                log_info "用户选择: 控制爬坡"
                control_climb 
                ;;
            7) 
                log_info "用户选择: 控制停止行走"
                control_stop_walk 
                ;;
            8) 
                log_info "用户选择: 控制停止爬坡"
                control_stop_climb 
                ;;
            9) 
                log_info "用户选择: 控制停止跳跃"
                control_stop_jump 
                ;;
            10) 
                log_info "用户选择: 控制停止跑步"
                control_stop_run 
                ;;
            11) 
                log_info "用户选择: 控制停止站立"
                control_stop_stand 
                ;;
            12) 
                log_info "用户选择: 查看当前状态"
                show_current_status_menu 
                ;;
            13) 
                log_info "用户选择: 查看日志文件"
                show_log_files
                ;;
            14) 
                log_info "用户选择: 导出日志报告"
                export_log_report_menu
                ;;
            0)
                log_info "用户选择: 退出系统"
                echo -e "${YELLOW}正在退出系统...${NC}"
                # 发送停止命令确保安全
                send_velocity 0.0 0.0 0.0
                sleep 0.2
                send_stand_twice
                sleep 0.2
                SYSTEM_RUNNING=false
                safe_exit
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 初始化
initialize_system() {
    clear
    echo -e "${PURPLE}运控主板运动管理系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者:$DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${CYAN}初始化中...${NC}"
    
    # 初始化日志系统
    init_log_system
    
    # 检查ROS2环境
    if ! command -v ros2 &> /dev/null; then
        log_error "未找到ROS2环境"
        echo -e "${RED}错误: 未找到ROS2环境${NC}"
        echo -e "${YELLOW}请确保ROS2已正确安装并配置${NC}"
        exit 1
    fi
    
    # 检查控制模式服务是否存在
    echo -e "${CYAN}检查系统连接...${NC}"
    sleep 1
    
    # 发送初始状态消息
    send_velocity 0.0 0.0 0.0
    send_control_mode $MODE_STAND "系统初始化 - 进入站立模式"
    
    echo -e "${GREEN}系统初始化完成${NC}"
    echo -e "${GREEN}日志文件: $LOG_FILE${NC}"
    sleep 1
}

# 主程序
parse_args "$@"
check_first_start
initialize_system
main_loop
