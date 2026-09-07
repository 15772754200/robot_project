#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 开发人员控制系统 v0.0
# 功能：机器人运动控制系统
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

# 日志文件配置
LOG_DIR="./run_logs/robot_set"
LOG_FILE="$LOG_DIR/robot_control_system.log"


# 控制话题
CONTROL_TOPIC="/hhros2_core/set_control_mode"
VELOCITY_TOPIC="/cmd_vel"
JOINT_COMMAND_TOPIC="/humanoid_base_controller/reference"

# 全局变量
CURRENT_MODE=0
CURRENT_VEL_X=0.0
CURRENT_VEL_Y=0.0
CURRENT_VEL_Z=0.0
MAX_SPEED=2.0
MAX_JOINT_ANGULAR_VELOCITY=0.1
STEP=0.1
SYSTEM_RUNNING=true
DEVELOPER="王崇超"
VERSION="v0.0"

JOINT_NAMES=(
    "left_hip_pitch_joint" "left_hip_roll_joint" "left_hip_yaw_joint" "left_knee_joint" "left_ankle_pitch_joint" "left_ankle_roll_joint"
    "right_hip_pitch_joint" "right_hip_roll_joint" "right_hip_yaw_joint" "right_knee_joint" "right_ankle_pitch_joint" "right_ankle_roll_joint"
    "waist_yaw_joint" "waist_pitch_joint" "head_yaw_joint"
    "left_shoulder_pitch_joint" "left_shoulder_roll_joint" "left_shoulder_yaw_joint" "left_elbow_joint"
    "right_shoulder_pitch_joint" "right_shoulder_roll_joint" "right_shoulder_yaw_joint" "right_elbow_joint"
)

JOINT_LOWER_LIMITS=(
    -2.6180 -0.1746 -0.5873 -1.4835 -1.0 -0.400
    -2.6180 -1.0472 -0.5873 -0.0873 -1.0 -0.400
    -2.6180 -0.5240 -0.5236
    -2.8798 -1.4486 -3.1416 -2.3562
    -2.3562 -1.5708 -3.1416 -1.50
)

JOINT_UPPER_LIMITS=(
    2.6180 1.0472 2.3562 0.0873 1.0 0.400
    2.6180 0.1746 2.3562 1.4835 1.0 0.400
    2.6180 0.5240 0.5236
    2.3562 1.5708 3.1416 1.50
    2.8798 1.4486 3.1416 2.3562
)

CURRENT_JOINT_INDEX=-1
CURRENT_JOINT_ANGULAR_VELOCITY=0.0

# 控制模式定义
MODE_PREPARE=0
MODE_WALK=3
MODE_STAND=2
MODE_RUN=4
MODE_JUMP=-1
MODE_STOP_STAND=2
MODE_WBC=5
MODE_IMITATE=6

# 创建日志目录
create_log_dir() {
    if [ ! -d "$LOG_DIR" ]; then
        mkdir -p "$LOG_DIR"
        echo -e "${GREEN}创建日志目录: $LOG_DIR${NC}"
    fi
}



# 初始化日志系统
init_log_system() {
    create_log_dir
    LOG_FILE="$LOG_DIR/robot_control.log"
    # 写入日志头
    {
        echo "════════════════════════════════════════"
        echo "机器人控制日志文件"
        echo "启动时间: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "系统版本: $VERSION"
        echo "开发者: $DEVELOPER"
        echo "日志文件: $LOG_FILE"
        echo "════════════════════════════════════════"
        echo ""
    } >> "$LOG_FILE"
    
    echo -e "${GREEN}日志系统已初始化${NC}"
    echo -e "${CYAN}日志文件: $LOG_FILE${NC}"
}


# 记录日志函数
log_message() {
    local level="$1"     # 日志级别: INFO, WARN, ERROR, DEBUG
    local action="$2"    # 操作描述
    local details="$3"   # 详细信息
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    # 颜色代码映射
    local color
    case $level in
        "INFO") color="$GREEN" ;;
        "WARN") color="$YELLOW" ;;
        "ERROR") color="$RED" ;;
        "DEBUG") color="$BLUE" ;;
        *) color="$WHITE" ;;
    esac
    
    # 格式化输出
    local log_entry="$timestamp [$level] $action"
    if [ -n "$details" ]; then
        log_entry="$log_entry | $details"
    fi
    
    # 写入日志文件
    echo "$log_entry" >> "$LOG_FILE"
    
    # 同时在控制台显示（调试时可启用）
    # echo -e "${color}$log_entry${NC}"
}

# 记录控制模式变更
log_control_mode() {
    local mode="$1"
    local description="$2"
    
    # 模式描述映射
    local mode_desc
    case $mode in
        0) mode_desc="被动/准备模式" ;;
        1) mode_desc="阻尼模式" ;;
        2) mode_desc="站立模式" ;;
        3) mode_desc="行走模式" ;;
        4) mode_desc="跑步模式" ;;
        5) mode_desc="WBC模式" ;;
        6) mode_desc="模仿模式" ;;
        *) mode_desc="未知模式($mode)" ;;
    esac
    
    log_message "INFO" "控制模式变更" "模式=$mode($mode_desc) 说明=$description"
}

# 记录速度设置
log_velocity() {
    local linear_x="$1"
    local linear_y="$2"
    local angular_z="$3"
    local operation="$4"
    
    # 方向判断
    local x_direction
    if (( $(echo "$linear_x > 0" | bc -l) )); then
        x_direction="前进"
    elif (( $(echo "$linear_x < 0" | bc -l) )); then
        x_direction="后退"
    else
        x_direction="停止"
    fi
    
    local y_direction
    if (( $(echo "$linear_y > 0" | bc -l) )); then
        y_direction="左移"
    elif (( $(echo "$linear_y < 0" | bc -l) )); then
        y_direction="右移"
    else
        y_direction="停止"
    fi
    
    local z_direction
    if (( $(echo "$angular_z > 0" | bc -l) )); then
        z_direction="左转"
    elif (( $(echo "$angular_z < 0" | bc -l) )); then
        z_direction="右转"
    else
        z_direction="停止"
    fi
    
    local details="X=$linear_x m/s($x_direction) Y=$linear_y m/s($y_direction) Z=$angular_z rad/s($z_direction)"
    if [ -n "$operation" ]; then
        details="$operation | $details"
    fi
    
    log_message "INFO" "速度设置" "$details"
}




# 记录系统事件
log_system_event() {
    local event="$1"
    local details="$2"
    
    log_message "SYSTEM" "$event" "$details"
}

# 记录错误
log_error() {
    local error_msg="$1"
    local context="$2"
    
    local details="错误: $error_msg"
    if [ -n "$context" ]; then
        details="$details 上下文: $context"
    fi
    
    log_message "ERROR" "系统错误" "$details"
}

# 记录调试信息
log_debug() {
    local debug_msg="$1"
    local data="$2"
    
    local details="$debug_msg"
    if [ -n "$data" ]; then
        details="$details 数据: $data"
    fi
    
    log_message "DEBUG" "调试信息" "$details"
}


# 显示日志文件信息
show_log_info() {
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          日志系统信息${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    if [ -f "$LOG_FILE" ]; then
        local file_size=$(du -h "$LOG_FILE" | cut -f1)
        local line_count=$(wc -l < "$LOG_FILE")
        local last_modified=$(date -r "$LOG_FILE" '+%Y-%m-%d %H:%M:%S')
        
        echo -e "日志文件: ${GREEN}$LOG_FILE${NC}"
        echo -e "文件大小: ${CYAN}$file_size${NC}"
        echo -e "日志行数: ${CYAN}$line_count${NC}"
        echo -e "最后修改: ${CYAN}$last_modified${NC}"
        
        echo ""
        echo -e "${YELLOW}最近5条日志:${NC}"
        echo "------------------------------------------------"
        tail -n 5 "$LOG_FILE"
        echo "------------------------------------------------"
        
        echo ""
        echo -e "${CYAN}日志操作:${NC}"
        echo -e "  ${GREEN}1.${NC} 查看完整日志"
        echo -e "  ${GREEN}2.${NC} 清空当前日志"
        echo -e "  ${GREEN}3.${NC} 查看日志目录"
        echo -e "  ${GREEN}0.${NC} 返回主菜单"
        echo ""
        
        read_line log_choice "请选择操作 [0-3]: "
        
        case $log_choice in
            1)  # 查看完整日志
                clear
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          完整日志内容${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                cat "$LOG_FILE" | less
                ;;
            2)  # 清空日志
                read_line confirm_clear "确认清空日志文件? (y/n): "
                if [[ "$confirm_clear" == "y" || "$confirm_clear" == "Y" ]]; then
                    > "$LOG_FILE"
                    echo -e "${GREEN}日志文件已清空${NC}"
                    # 重新写入日志头
                    {
                        echo "════════════════════════════════════════"
                        echo "机器人控制日志文件 (已清空)"
                        echo "清空时间: $(date '+%Y-%m-%d %H:%M:%S')"
                        echo "系统版本: $VERSION"
                        echo "开发者: $DEVELOPER"
                        echo "日志文件: $LOG_FILE"
                        echo "════════════════════════════════════════"
                        echo ""
                    } >> "$LOG_FILE"
                else
                    echo -e "${YELLOW}取消清空操作${NC}"
                fi
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            3)  # 查看日志目录
                clear
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          日志目录内容${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "${CYAN}日志目录: $LOG_DIR${NC}"
                echo ""
                ls -la "$LOG_DIR" | grep -E "\.log$|total"
                echo ""
                echo -e "${YELLOW}按 q 退出列表${NC}"
                echo ""
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            0)  # 返回
                return
                ;;
            *)
                echo -e "${RED}无效选择${NC}"
                ;;
        esac
    else
        echo -e "${RED}日志文件不存在${NC}"
        echo -e "${YELLOW}正在重新初始化日志系统...${NC}"
        init_log_system
    fi
}

# 显示系统标题
show_title() {
    clear
    echo ""
    echo -e "${PURPLE}设定控制模式与速度系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者: $DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
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
        
        # 记录用户选择
 
        
        case $first_start in
            1)  # 第一次启动，发布两次0进入准备姿态
                echo -e "\n${YELLOW}正在进入准备姿态...${NC}"
                log_system_event "首次启动处理" "进入准备姿态"
                
                # 定义准备姿态模式
                MODE_PREPARE=0
                
                echo -e "${GREEN}步骤1:${NC} 发送第一次准备姿态命令(0)..."
                send_control_mode $MODE_PREPARE "第一次准备姿态"
                
                echo -e "${GREEN}步骤2:${NC} 等待0.2秒..."
                sleep 0.2
                
                echo -e "${GREEN}步骤3:${NC} 发送第二次准备姿态命令(0)..."
                send_control_mode $MODE_PREPARE "第二次准备姿态"
                
                echo -e "\n${GREEN}✓ 机器人已进入准备姿态${NC}"
                log_system_event "准备姿态完成" "机器人已进入准备姿态"
                sleep 1
                let_robot_down
                return
                ;;
            0)  # 不是第一次启动，直接继续
                echo -e "\n${YELLOW}跳过准备姿态，直接进入系统...${NC}"
                log_system_event "非首次启动" "跳过准备姿态"
                return
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                ;;
        esac
    done
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
        log_system_event "机器人放置检查" "用户取消操作，机器人未正确放置"
        echo "按任意键返回..."
        read_key _terminal_key
        stop
    fi
    
    echo -e "${GREEN}确认完成，继续执行后续操作...${NC}"
    log_system_event "机器人放置检查" "机器人已正确放置，继续操作"
    echo ""
}

# 发送控制模式命令
send_control_mode() {
    local mode=$1
    local description=$2
    
    if [ "$mode" -eq "$MODE_JUMP" ]; then
        echo -e "${RED}当前框架没有独立跳跃模式，未发送控制命令。${NC}"
        return 1
    fi

    local response
    response="$(ros2 service call "$CONTROL_TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: $mode}" 2>&1)"
    printf '%s\n' "$response"
    
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        CURRENT_MODE=$mode
        case $mode in
            0) echo -e "${GREEN}✓ 已发送命令: 被动/准备模式${NC}" ;;
            1) echo -e "${GREEN}✓ 已发送命令: 阻尼模式${NC}" ;;
            2) echo -e "${GREEN}✓ 已发送命令: 站立模式${NC}" ;;
            3) echo -e "${GREEN}✓ 已发送命令: 行走模式${NC}" ;;
            4) echo -e "${GREEN}✓ 已发送命令: 跑步模式${NC}" ;;
            5) echo -e "${GREEN}✓ 已发送命令: WBC模式${NC}" ;;
            6) echo -e "${GREEN}✓ 已发送命令: 模仿模式${NC}" ;;
            *) echo -e "${BLUE}✓ 已发送命令: 模式 $mode${NC}" ;;
        esac
        if [[ -n "$description" ]]; then
            echo -e "说明: $description${NC}"
        fi
        log_control_mode "$mode" "$description"
        return 0
    else
        echo -e "${RED}✗ 发送命令失败${NC}"
        log_error "发送控制模式失败" "mode=$mode, description=$description"
        return 1
    fi
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

# 发送速度命令
send_velocity() {
    local linear_x=$1
    local linear_y=$2
    local angular_z=$3
    
    # 限制速度范围
    if (( $(echo "$linear_x > $MAX_SPEED" | bc -l) )); then
        linear_x=$MAX_SPEED
    elif (( $(echo "$linear_x < -$MAX_SPEED" | bc -l) )); then
        linear_x=-$MAX_SPEED
    fi
    
    if (( $(echo "$linear_y > $MAX_SPEED" | bc -l) )); then
        linear_y=$MAX_SPEED
    elif (( $(echo "$linear_y < -$MAX_SPEED" | bc -l) )); then
        linear_y=-$MAX_SPEED
    fi
    
    if (( $(echo "$angular_z > $MAX_SPEED" | bc -l) )); then
        angular_z=$MAX_SPEED
    elif (( $(echo "$angular_z < -$MAX_SPEED" | bc -l) )); then
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
        echo -e "${GREEN}✓ 速度已发送: X=${CURRENT_VEL_X} m/s, Y=${CURRENT_VEL_Y} m/s, Z=${CURRENT_VEL_Z} rad/s${NC}"
        log_velocity "$linear_x" "$linear_y" "$angular_z" "手动设置"
    else
        echo -e "${RED}✗ 发送速度失败${NC}"
        log_error "发送速度失败" "linear_x=$linear_x, linear_y=$linear_y, angular_z=$angular_z"
        return 1
    fi
    return 0
}

is_number() {
    local value="$1"
    [[ "$value" =~ ^-?([0-9]+([.][0-9]*)?|[.][0-9]+)$ ]]
}

joint_positive_velocity_allowed() {
    local joint_idx="$1"
    local upper="${JOINT_UPPER_LIMITS[$joint_idx]}"
    (( $(echo "$upper > $MAX_JOINT_ANGULAR_VELOCITY" | bc -l) ))
}

joint_negative_velocity_allowed() {
    local joint_idx="$1"
    local lower="${JOINT_LOWER_LIMITS[$joint_idx]}"
    (( $(echo "$lower < -$MAX_JOINT_ANGULAR_VELOCITY" | bc -l) ))
}

joint_allowed_direction() {
    local joint_idx="$1"
    local positive_allowed=false
    local negative_allowed=false

    if joint_positive_velocity_allowed "$joint_idx"; then
        positive_allowed=true
    fi
    if joint_negative_velocity_allowed "$joint_idx"; then
        negative_allowed=true
    fi

    if [[ "$positive_allowed" == true && "$negative_allowed" == true ]]; then
        echo "+/-"
    elif [[ "$positive_allowed" == true ]]; then
        echo "仅正向"
    elif [[ "$negative_allowed" == true ]]; then
        echo "仅负向"
    else
        echo "仅0"
    fi
}

show_joint_limit_table() {
    echo -e "${WHITE}编号  关节名                            下限(rad)   上限(rad)   允许方向${NC}"
    echo "--------------------------------------------------------------------------------"
    for i in "${!JOINT_NAMES[@]}"; do
        printf " %2d   %-34s %9s   %9s   %s\n" \
            "$i" "${JOINT_NAMES[$i]}" "${JOINT_LOWER_LIMITS[$i]}" "${JOINT_UPPER_LIMITS[$i]}" "$(joint_allowed_direction "$i")"
    done
}

build_joint_names_yaml() {
    local yaml="["
    for i in "${!JOINT_NAMES[@]}"; do
        if [[ "$i" -gt 0 ]]; then
            yaml+=", "
        fi
        yaml+="'${JOINT_NAMES[$i]}'"
    done
    yaml+="]"
    echo "$yaml"
}

build_zero_array_yaml() {
    local yaml="["
    for i in "${!JOINT_NAMES[@]}"; do
        if [[ "$i" -gt 0 ]]; then
            yaml+=", "
        fi
        yaml+="0.0"
    done
    yaml+="]"
    echo "$yaml"
}

build_joint_velocity_array_yaml() {
    local joint_idx="$1"
    local joint_velocity="$2"
    local yaml="["

    for i in "${!JOINT_NAMES[@]}"; do
        if [[ "$i" -gt 0 ]]; then
            yaml+=", "
        fi

        if [[ "$i" -eq "$joint_idx" ]]; then
            yaml+="$joint_velocity"
        else
            yaml+="0.0"
        fi
    done

    yaml+="]"
    echo "$yaml"
}

validate_joint_angular_velocity() {
    local joint_idx="$1"
    local joint_velocity="$2"
    local lower="${JOINT_LOWER_LIMITS[$joint_idx]}"
    local upper="${JOINT_UPPER_LIMITS[$joint_idx]}"

    if (( $(echo "$joint_velocity > $MAX_JOINT_ANGULAR_VELOCITY" | bc -l) )) ||
       (( $(echo "$joint_velocity < -$MAX_JOINT_ANGULAR_VELOCITY" | bc -l) )); then
        echo -e "${RED}错误: 最大输入关节角速度限制为 ±${MAX_JOINT_ANGULAR_VELOCITY} rad/s${NC}"
        return 1
    fi

    if (( $(echo "$joint_velocity == 0" | bc -l) )); then
        return 0
    fi

    if (( $(echo "$joint_velocity > 0" | bc -l) )) && ! joint_positive_velocity_allowed "$joint_idx"; then
        echo -e "${RED}错误: ${JOINT_NAMES[$joint_idx]} 正向角度上限为 ${upper} rad，不允许设置正角速度${NC}"
        echo -e "${YELLOW}提示: 像 0.0873 rad 这类接近零位的正向限位，只允许设置负向或 0 角速度${NC}"
        return 1
    fi

    if (( $(echo "$joint_velocity < 0" | bc -l) )) && ! joint_negative_velocity_allowed "$joint_idx"; then
        echo -e "${RED}错误: ${JOINT_NAMES[$joint_idx]} 负向角度下限为 ${lower} rad，不允许设置负角速度${NC}"
        echo -e "${YELLOW}提示: 像 -0.0873 rad 这类接近零位的负向限位，只允许设置正向或 0 角速度${NC}"
        return 1
    fi

    return 0
}

log_joint_angular_velocity() {
    local joint_idx="$1"
    local joint_velocity="$2"

    log_message "INFO" "关节角速度设置" \
        "index=$joint_idx joint=${JOINT_NAMES[$joint_idx]} velocity=${joint_velocity}rad/s lower=${JOINT_LOWER_LIMITS[$joint_idx]} upper=${JOINT_UPPER_LIMITS[$joint_idx]}"
}

send_joint_angular_velocity() {
    local joint_idx="$1"
    local joint_velocity="$2"
    local joint_names_yaml
    local zero_array_yaml
    local velocity_array_yaml
    local message

    joint_names_yaml=$(build_joint_names_yaml)
    zero_array_yaml=$(build_zero_array_yaml)
    velocity_array_yaml=$(build_joint_velocity_array_yaml "$joint_idx" "$joint_velocity")

    message="{header: {stamp: {sec: 0, nanosec: 0}, frame_id: ''}, joint_names: $joint_names_yaml, kp: $zero_array_yaml, kd: $zero_array_yaml, position: $zero_array_yaml, velocity: $velocity_array_yaml, effort: $zero_array_yaml}"

    ros2 topic pub -1 "$JOINT_COMMAND_TOPIC" hhros2_interfaces/msg/JointMotor "$message" 2>/dev/null

    if [ $? -eq 0 ]; then
        CURRENT_JOINT_INDEX="$joint_idx"
        CURRENT_JOINT_ANGULAR_VELOCITY="$joint_velocity"
        echo -e "${GREEN}✓ 已发送关节角速度: [${joint_idx}] ${JOINT_NAMES[$joint_idx]} = ${joint_velocity} rad/s${NC}"
        log_joint_angular_velocity "$joint_idx" "$joint_velocity"
        return 0
    fi

    echo -e "${RED}✗ 发送关节角速度失败${NC}"
    log_error "发送关节角速度失败" "joint=${JOINT_NAMES[$joint_idx]}, velocity=$joint_velocity"
    return 1
}

# 发送两次站立命令
send_stand_twice() {
    echo -e "${YELLOW}正在切换到站立模式...${NC}"
    send_control_mode $MODE_STAND "准备切换运动模式"
    sleep 0.1
    send_control_mode $MODE_STAND "确认站立状态"
    return 0
}

# 安全停止所有运动
safe_stop() {
    echo -e "${YELLOW}正在安全停止...${NC}"
    send_velocity 0.0 0.0 0.0
    sleep 0.2
    send_stand_twice
    safe_exit
    echo -e "${GREEN}✓ 机器人已安全停止${NC}"
    
    return 0
}
# 用于启动机器人选择不是第一次启动  



# 修改安全退出函数，记录退出事件
safe_exit() {
    echo -e "${RED}退出系统程序${NC}"
    log_system_event "系统强制退出" "用户触发安全退出"
    
    # 标记清理已被调用
    CLEANUP_CALLED=true
    
    exit 0
}








# 1. 设定机器人前进速度
set_forward_velocity() {
    while true; do
        clear
        show_title
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${WHITE}          前进/后退速度控制${NC}"
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo ""
        echo -e "当前前进/后退速度: ${GREEN}$CURRENT_VEL_X${NC} m/s"
        echo ""
        echo -e "${CYAN}请选择操作:${NC}"
        echo -e "  ${GREEN}1.${NC} 键盘控制 (W/S键调整)"
        echo -e "  ${GREEN}2.${NC} 直接设置速度值"
        echo -e "  ${GREEN}3.${NC} 显示当前状态"
        echo -e "  ${GREEN}0.${NC} 返回主菜单"
        echo ""
        
        read_line choice "请选择 [0-3]: "
        
        case $choice in
            1)  # 键盘控制
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          X方向速度键盘控制${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${CYAN}控制说明:${NC}"
                echo -e "  ${GREEN}W${NC}: 增加前进速度 (+0.1 m/s)"
                echo -e "  ${GREEN}S${NC}: 减少前进速度 (-0.1 m/s)"
                echo -e "  ${GREEN}R${NC}: 重置速度为0"
                echo -e "  ${RED}X${NC}: 返回上级菜单"
                echo ""
                echo -e "当前速度: X=${GREEN}$CURRENT_VEL_X${NC} m/s"
                echo -e "范围限制: ${GREEN}-${MAX_SPEED}${NC} 到 ${GREEN}+${MAX_SPEED}${NC} m/s"
                echo ""
                
                while true; do
                    read_key key
                    
                    case $key in
                        w|W)  # 增加前进速度
                            CURRENT_VEL_X=$(echo "$CURRENT_VEL_X + $STEP" | bc)
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "当前速度: X=${GREEN}$CURRENT_VEL_X${NC} m/s"
                            ;;
                        s|S)  # 减少前进速度
                            CURRENT_VEL_X=$(echo "$CURRENT_VEL_X - $STEP" | bc)
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "当前速度: X=${GREEN}$CURRENT_VEL_X${NC} m/s"
                            ;;
                        r|R)  # 重置速度为0
                            CURRENT_VEL_X=0.0
                            send_velocity 0.0 $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "${YELLOW}✓ 已重置X方向速度为0${NC}"
                            echo -e "当前速度: X=${GREEN}0.0${NC} m/s"
                            ;;
                        x|X)  # 返回
                            echo -e "\n${YELLOW}返回上级菜单...${NC}"
                            break
                            ;;
                        *)
                            if [ -n "$key" ]; then
                                echo -e "${YELLOW}无效按键，请使用 W/S/R/X${NC}"
                            fi
                            ;;
                    esac
                done
                ;;
            2)  # 直接设置速度
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          直接设置前进/后退速度${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "当前速度: X=${GREEN}$CURRENT_VEL_X${NC} m/s"
                echo -e "范围限制: ${GREEN}-${MAX_SPEED}${NC} 到 ${GREEN}+${MAX_SPEED}${NC} m/s"
                echo ""
                echo -e "${CYAN}输入正数为前进，负数为后退${NC}"
                echo ""
                
                read_line input_vel "请输入X方向速度(m/s): "
                
                if [[ "$input_vel" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
                    # 检查速度范围
                    if (( $(echo "$input_vel > $MAX_SPEED" | bc -l) )); then
                        echo -e "${YELLOW}速度超过上限，将设置为最大值 ${MAX_SPEED} m/s${NC}"
                        input_vel=$MAX_SPEED
                    elif (( $(echo "$input_vel < -$MAX_SPEED" | bc -l) )); then
                        echo -e "${YELLOW}速度超过下限，将设置为最小值 -${MAX_SPEED} m/s${NC}"
                        input_vel=-$MAX_SPEED
                    fi
                    
                    send_velocity $input_vel $CURRENT_VEL_Y $CURRENT_VEL_Z
                else
                    echo -e "${RED}无效输入，请输入数字${NC}"
                fi
                
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            3)  # 显示状态
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          当前状态${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "控制模式: ${GREEN}$CURRENT_MODE${NC}"
                case $CURRENT_MODE in
                    0) echo -e "模式描述: ${GREEN}被动/准备模式${NC}" ;;
                    1) echo -e "模式描述: ${GREEN}阻尼模式${NC}" ;;
                    2) echo -e "模式描述: ${GREEN}站立模式${NC}" ;;
                    3) echo -e "模式描述: ${GREEN}行走模式${NC}" ;;
                    4) echo -e "模式描述: ${GREEN}跑步模式${NC}" ;;
                    5) echo -e "模式描述: ${GREEN}WBC模式${NC}" ;;
                    6) echo -e "模式描述: ${GREEN}模仿模式${NC}" ;;
                    *) echo -e "模式描述: ${BLUE}未知模式${NC}" ;;
                esac
                echo ""
                echo -e "当前速度:"
                echo -e "  X方向(前进/后退): ${GREEN}$CURRENT_VEL_X${NC} m/s"
                echo -e "  Y方向(左移/右移): ${GREEN}$CURRENT_VEL_Y${NC} m/s"
                echo -e "  Z方向(左转/右转): ${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                echo ""
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            0)  # 返回主菜单
                return
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 2. 设定机器人左右速度
set_lateral_velocity() {
    while true; do
        clear
        show_title
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${WHITE}          左移/右移速度控制${NC}"
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo ""
        echo -e "当前左移/右移速度: ${GREEN}$CURRENT_VEL_Y${NC} m/s"
        echo ""
        echo -e "${CYAN}请选择操作:${NC}"
        echo -e "  ${GREEN}1.${NC} 键盘控制 (A/D键调整)"
        echo -e "  ${GREEN}2.${NC} 直接设置速度值"
        echo -e "  ${GREEN}3.${NC} 显示当前状态"
        echo -e "  ${GREEN}0.${NC} 返回主菜单"
        echo ""
        
        read_line choice "请选择 [0-3]: "
        
        case $choice in
            1)  # 键盘控制
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          Y方向速度键盘控制${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${CYAN}控制说明:${NC}"
                echo -e "  ${GREEN}A${NC}: 增加左移速度 (+0.1 m/s)"
                echo -e "  ${GREEN}D${NC}: 减少左移速度 (-0.1 m/s)"
                echo -e "  ${GREEN}R${NC}: 重置速度为0"
                echo -e "  ${RED}X${NC}: 返回上级菜单"
                echo ""
                echo -e "当前速度: Y=${GREEN}$CURRENT_VEL_Y${NC} m/s"
                echo -e "范围限制: ${GREEN}-${MAX_SPEED}${NC} 到 ${GREEN}+${MAX_SPEED}${NC} m/s"
                echo ""
                
                while true; do
                    read_key key
                    
                    case $key in
                        a|A)  # 增加左移速度
                            CURRENT_VEL_Y=$(echo "$CURRENT_VEL_Y + $STEP" | bc)
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "当前速度: Y=${GREEN}$CURRENT_VEL_Y${NC} m/s"
                            ;;
                        d|D)  # 减少左移速度
                            CURRENT_VEL_Y=$(echo "$CURRENT_VEL_Y - $STEP" | bc)
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "当前速度: Y=${GREEN}$CURRENT_VEL_Y${NC} m/s"
                            ;;
                        r|R)  # 重置速度为0
                            CURRENT_VEL_Y=0.0
                            send_velocity $CURRENT_VEL_X 0.0 $CURRENT_VEL_Z
                            echo -e "${YELLOW}✓ 已重置Y方向速度为0${NC}"
                            echo -e "当前速度: Y=${GREEN}0.0${NC} m/s"
                            ;;
                        x|X)  # 返回
                            echo -e "\n${YELLOW}返回上级菜单...${NC}"
                            break
                            ;;
                        *)
                            if [ -n "$key" ]; then
                                echo -e "${YELLOW}无效按键，请使用 A/D/R/X${NC}"
                            fi
                            ;;
                    esac
                done
                ;;
            2)  # 直接设置速度
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          直接设置左移/右移速度${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "当前速度: Y=${GREEN}$CURRENT_VEL_Y${NC} m/s"
                echo -e "范围限制: ${GREEN}-${MAX_SPEED}${NC} 到 ${GREEN}+${MAX_SPEED}${NC} m/s"
                echo ""
                echo -e "${CYAN}输入正数为左移，负数为右移${NC}"
                echo ""
                
                read_line input_vel "请输入Y方向速度(m/s): "
                
                if [[ "$input_vel" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
                    # 检查速度范围
                    if (( $(echo "$input_vel > $MAX_SPEED" | bc -l) )); then
                        echo -e "${YELLOW}速度超过上限，将设置为最大值 ${MAX_SPEED} m/s${NC}"
                        input_vel=$MAX_SPEED
                    elif (( $(echo "$input_vel < -$MAX_SPEED" | bc -l) )); then
                        echo -e "${YELLOW}速度超过下限，将设置为最小值 -${MAX_SPEED} m/s${NC}"
                        input_vel=-$MAX_SPEED
                    fi
                    
                    send_velocity $CURRENT_VEL_X $input_vel $CURRENT_VEL_Z
                else
                    echo -e "${RED}无效输入，请输入数字${NC}"
                fi
                
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            3)  # 显示状态
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          当前状态${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "控制模式: ${GREEN}$CURRENT_MODE${NC}"
                case $CURRENT_MODE in
                    0) echo -e "模式描述: ${GREEN}被动/准备模式${NC}" ;;
                    1) echo -e "模式描述: ${GREEN}阻尼模式${NC}" ;;
                    2) echo -e "模式描述: ${GREEN}站立模式${NC}" ;;
                    3) echo -e "模式描述: ${GREEN}行走模式${NC}" ;;
                    4) echo -e "模式描述: ${GREEN}跑步模式${NC}" ;;
                    5) echo -e "模式描述: ${GREEN}WBC模式${NC}" ;;
                    6) echo -e "模式描述: ${GREEN}模仿模式${NC}" ;;
                    *) echo -e "模式描述: ${BLUE}未知模式${NC}" ;;
                esac
                echo ""
                echo -e "当前速度:"
                echo -e "  X方向(前进/后退): ${GREEN}$CURRENT_VEL_X${NC} m/s"
                echo -e "  Y方向(左移/右移): ${GREEN}$CURRENT_VEL_Y${NC} m/s"
                echo -e "  Z方向(左转/右转): ${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                echo ""
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            0)  # 返回主菜单
                return
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 3. 设定机器人偏航速度
set_yaw_velocity() {
    while true; do
        clear
        show_title
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${WHITE}          左转/右转速度控制${NC}"
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo ""
        echo -e "当前左转/右转速度: ${GREEN}$CURRENT_VEL_Z${NC} rad/s"
        echo ""
        echo -e "${CYAN}请选择操作:${NC}"
        echo -e "  ${GREEN}1.${NC} 键盘控制 (Q/E键调整)"
        echo -e "  ${GREEN}2.${NC} 直接设置速度值"
        echo -e "  ${GREEN}3.${NC} 显示当前状态"
        echo -e "  ${GREEN}0.${NC} 返回主菜单"
        echo ""
        
        read_line choice "请选择 [0-3]: "
        
        case $choice in
            1)  # 键盘控制
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          Z方向速度键盘控制${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${CYAN}控制说明:${NC}"
                echo -e "  ${GREEN}Q${NC}: 增加左转角速度 (+0.1 rad/s)"
                echo -e "  ${GREEN}E${NC}: 减少左转角速度 (-0.1 rad/s)"
                echo -e "  ${GREEN}R${NC}: 重置速度为0"
                echo -e "  ${RED}X${NC}: 返回上级菜单"
                echo ""
                echo -e "当前速度: Z=${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                echo -e "范围限制: ${GREEN}-${MAX_SPEED}${NC} 到 ${GREEN}+${MAX_SPEED}${NC} rad/s"
                echo ""
                
                while true; do
                    read_key key
                    
                    case $key in
                        q|Q)  # 增加左转角速度
                            CURRENT_VEL_Z=$(echo "$CURRENT_VEL_Z + $STEP" | bc)
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "当前速度: Z=${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                            ;;
                        e|E)  # 减少左转角速度
                            CURRENT_VEL_Z=$(echo "$CURRENT_VEL_Z - $STEP" | bc)
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $CURRENT_VEL_Z
                            echo -e "当前速度: Z=${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                            ;;
                        r|R)  # 重置速度为0
                            CURRENT_VEL_Z=0.0
                            send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y 0.0
                            echo -e "${YELLOW}✓ 已重置Z方向速度为0${NC}"
                            echo -e "当前速度: Z=${GREEN}0.0${NC} rad/s"
                            ;;
                        x|X)  # 返回
                            echo -e "\n${YELLOW}返回上级菜单...${NC}"
                            break
                            ;;
                        *)
                            if [ -n "$key" ]; then
                                echo -e "${YELLOW}无效按键，请使用 Q/E/R/X${NC}"
                            fi
                            ;;
                    esac
                done
                ;;
            2)  # 直接设置速度
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          直接设置左转/右转速度${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "当前速度: Z=${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                echo -e "范围限制: ${GREEN}-${MAX_SPEED}${NC} 到 ${GREEN}+${MAX_SPEED}${NC} rad/s"
                echo ""
                echo -e "${CYAN}输入正数为左转，负数为右转${NC}"
                echo ""
                
                read_line input_vel "请输入Z方向角速度(rad/s): "
                
                if [[ "$input_vel" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
                    # 检查速度范围
                    if (( $(echo "$input_vel > $MAX_SPEED" | bc -l) )); then
                        echo -e "${YELLOW}速度超过上限，将设置为最大值 ${MAX_SPEED} rad/s${NC}"
                        input_vel=$MAX_SPEED
                    elif (( $(echo "$input_vel < -$MAX_SPEED" | bc -l) )); then
                        echo -e "${YELLOW}速度超过下限，将设置为最小值 -${MAX_SPEED} rad/s${NC}"
                        input_vel=-$MAX_SPEED
                    fi
                    
                    send_velocity $CURRENT_VEL_X $CURRENT_VEL_Y $input_vel
                else
                    echo -e "${RED}无效输入，请输入数字${NC}"
                fi
                
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            3)  # 显示状态
                clear
                show_title
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo -e "${WHITE}          当前状态${NC}"
                echo -e "${WHITE}════════════════════════════════════════${NC}"
                echo ""
                echo -e "控制模式: ${GREEN}$CURRENT_MODE${NC}"
                case $CURRENT_MODE in
                    0) echo -e "模式描述: ${GREEN}被动/准备模式${NC}" ;;
                    1) echo -e "模式描述: ${GREEN}阻尼模式${NC}" ;;
                    2) echo -e "模式描述: ${GREEN}站立模式${NC}" ;;
                    3) echo -e "模式描述: ${GREEN}行走模式${NC}" ;;
                    4) echo -e "模式描述: ${GREEN}跑步模式${NC}" ;;
                    5) echo -e "模式描述: ${GREEN}WBC模式${NC}" ;;
                    6) echo -e "模式描述: ${GREEN}模仿模式${NC}" ;;
                    *) echo -e "模式描述: ${BLUE}未知模式${NC}" ;;
                esac
                echo ""
                echo -e "当前速度:"
                echo -e "  X方向(前进/后退): ${GREEN}$CURRENT_VEL_X${NC} m/s"
                echo -e "  Y方向(左移/右移): ${GREEN}$CURRENT_VEL_Y${NC} m/s"
                echo -e "  Z方向(左转/右转): ${GREEN}$CURRENT_VEL_Z${NC} rad/s"
                echo ""
                read_key_prompt _terminal_key "按任意键继续..."
                ;;
            0)  # 返回主菜单
                return
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 6. 设定机器人关节角速度
set_joint_angular_velocity() {
    while true; do
        clear
        show_title
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${WHITE}          机器人关节角速度设置${NC}"
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo ""
        echo -e "${RED}开发人员注意:${NC}"
        echo -e "  ${YELLOW}1. 关节角速度必须根据电机实际型号、减速比、控制模式和现场安全条件设定。${NC}"
        echo -e "  ${YELLOW}2. 本菜单只基于当前软件关节限位做方向保护，不替代电机侧限速和硬件保护。${NC}"
        echo -e "  ${YELLOW}3. 最大输入角速度限制为 ±${MAX_JOINT_ANGULAR_VELOCITY} rad/s。${NC}"
        echo -e "  ${YELLOW}4. 若正向上限 <= ${MAX_JOINT_ANGULAR_VELOCITY} rad，则禁止正角速度；若负向下限 >= -${MAX_JOINT_ANGULAR_VELOCITY} rad，则禁止负角速度。${NC}"
        echo -e "  ${YELLOW}5. 当前命令只写 velocity 字段，kp/kd/position/effort 置 0；若电机控制器需要速度模式或增益，请先确认。${NC}"
        echo ""
        show_joint_limit_table
        echo ""

        read_line joint_choice "请输入要设置角速度的关节编号 [0-22]，或 q 返回: "

        if [[ "$joint_choice" == "q" || "$joint_choice" == "Q" ]]; then
            return
        fi

        if ! [[ "$joint_choice" =~ ^[0-9]+$ ]]; then
            echo -e "${RED}无效输入，请输入关节编号${NC}"
            read_key_prompt _terminal_key "按任意键继续..."
            continue
        fi

        local joint_idx=$((10#$joint_choice))
        if (( joint_idx < 0 || joint_idx >= ${#JOINT_NAMES[@]} )); then
            echo -e "${RED}无效关节编号，请输入 0-22${NC}"
            read_key_prompt _terminal_key "按任意键继续..."
            continue
        fi

        echo ""
        echo -e "${CYAN}当前选择:${NC}"
        echo -e "  编号: ${GREEN}${joint_idx}${NC}"
        echo -e "  关节: ${GREEN}${JOINT_NAMES[$joint_idx]}${NC}"
        echo -e "  角度下限: ${GREEN}${JOINT_LOWER_LIMITS[$joint_idx]}${NC} rad"
        echo -e "  角度上限: ${GREEN}${JOINT_UPPER_LIMITS[$joint_idx]}${NC} rad"
        echo -e "  允许方向: ${GREEN}$(joint_allowed_direction "$joint_idx")${NC}"
        echo ""

        read_line joint_velocity "请输入目标关节角速度(rad/s，最大 ±${MAX_JOINT_ANGULAR_VELOCITY}): "

        if ! is_number "$joint_velocity"; then
            echo -e "${RED}无效输入，请输入数字${NC}"
            read_key_prompt _terminal_key "按任意键继续..."
            continue
        fi

        joint_velocity=$(printf "%.4f" "$joint_velocity")

        if ! validate_joint_angular_velocity "$joint_idx" "$joint_velocity"; then
            read_key_prompt _terminal_key "按任意键继续..."
            continue
        fi

        echo ""
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo -e "${GREEN}关节角速度设置计划:${NC}"
        echo -e "  关节: ${GREEN}[${joint_idx}] ${JOINT_NAMES[$joint_idx]}${NC}"
        echo -e "  角速度: ${GREEN}${joint_velocity}${NC} rad/s"
        echo -e "  发布话题: ${GREEN}${JOINT_COMMAND_TOPIC}${NC}"
        echo -e "${WHITE}════════════════════════════════════════${NC}"
        echo ""
        read_line confirm "确认发送该关节角速度命令? (y/n): "

        if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
            send_joint_angular_velocity "$joint_idx" "$joint_velocity"
        else
            echo -e "${YELLOW}操作已取消${NC}"
        fi

        echo ""
        read_key_prompt _terminal_key "按任意键返回主菜单..."
        return
    done
}

# 4. 机器人前进后退指定距离
move_forward_backward() {
    clear
    show_title
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          前进/后退指定距离${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    
    # 输入距离
    echo -e "${CYAN}请输入移动距离:${NC}"
    echo -e "  ${GREEN}正数${NC}: 前进距离(m)"
    echo -e "  ${RED}负数${NC}: 后退距离(m)"
    echo ""
    read_line distance "距离(米): "
    
    if ! [[ "$distance" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        echo -e "${RED}无效输入，请输入数字${NC}"
        log_error "无效输入" "前进/后退距离输入: $distance"
        read_key_prompt _terminal_key "按任意键返回主菜单..."
        return
    fi
    
    # 输入速度
    echo ""
    echo -e "${CYAN}请输入移动速度:${NC}"
    echo -e "  ${GREEN}正数${NC}: 前进速度(m/s)"
    echo -e "  ${RED}负数${NC}: 后退速度(m/s)"
    echo -e "  ${YELLOW}默认: 0.5 m/s${NC}"
    echo ""
    read_line speed "速度(m/s) [默认0.5]: "
    
    if [ -z "$speed" ]; then
        speed=0.5
    elif ! [[ "$speed" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        echo -e "${RED}无效输入，将使用默认速度 0.5 m/s${NC}"
        log_error "无效输入" "前进/后退速度输入: $speed，使用默认值0.5"
        speed=0.5
    fi
    
    # 计算方向
    if (( $(echo "$distance >= 0" | bc -l) )); then
        direction_text="前进"
    else
        direction_text="后退"
    fi
    
    # 确保速度与距离方向一致
    if (( $(echo "$distance * $speed < 0" | bc -l) )); then
        echo -e "${YELLOW}警告: 速度方向与距离方向不一致${NC}"
        echo -e "${YELLOW}将调整速度方向以匹配距离方向${NC}"
        speed=$(echo "-1 * $speed" | bc)
    fi
    
    # 计算时间
    if (( $(echo "$speed == 0" | bc -l) )); then
        echo -e "${RED}错误: 速度不能为0${NC}"
        log_error "速度为零" "前进/后退速度为零，无法计算时间"
        read_key_prompt _terminal_key "按任意键返回主菜单..."
        return
    fi
    
    time_needed=$(echo "scale=2; $distance / $speed" | bc)
    
    # 显示计划
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${GREEN}移动计划:${NC}"
    echo -e "  方向: ${GREEN}$direction_text${NC}"
    echo -e "  距离: ${GREEN}$distance${NC} 米"
    echo -e "  速度: ${GREEN}$speed${NC} 米/秒"
    echo -e "  预计时间: ${GREEN}$time_needed${NC} 秒"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    
    read_line confirm "确认执行? (y/n): "
    
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
   
        # 发送速度命令
        echo -e "${YELLOW}开始移动...${NC}"
        send_velocity $speed 0.0 0.0
        
        # 计算并等待
        echo -e "${YELLOW}移动中，预计 ${time_needed} 秒...${NC}"
        
        # 使用sleep等待
        sleep $(echo "if ($time_needed < 0) -$time_needed else $time_needed" | bc)
        
        # 停止运动
        echo -e "${YELLOW}移动完成，停止...${NC}"
        send_velocity 0.0 0.0 0.0
        
        echo -e "${GREEN}✓ 移动完成${NC}"
    else
        echo -e "${RED}操作已取消${NC}"
    fi
    
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
}

# 5. 机器人左右移动指定距离
move_left_right() {
    clear
    show_title
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          左移/右移指定距离${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    
    # 输入距离
    echo -e "${CYAN}请输入移动距离:${NC}"
    echo -e "  ${GREEN}正数${NC}: 左移距离(m)"
    echo -e "  ${RED}负数${NC}: 右移距离(m)"
    echo ""
    read_line distance "距离(米): "
    
    if ! [[ "$distance" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        echo -e "${RED}无效输入，请输入数字${NC}"
        log_error "无效输入" "左移/右移距离输入: $distance"
        read_key_prompt _terminal_key "按任意键返回主菜单..."
        return
    fi
    
    # 输入速度
    echo ""
    echo -e "${CYAN}请输入移动速度:${NC}"
    echo -e "  ${GREEN}正数${NC}: 左移速度(m/s)"
    echo -e "  ${RED}负数${NC}: 右移速度(m/s)"
    echo -e "  ${YELLOW}默认: 0.5 m/s${NC}"
    echo ""
    read_line speed "速度(m/s) [默认0.5]: "
    
    if [ -z "$speed" ]; then
        speed=0.5
    elif ! [[ "$speed" =~ ^-?[0-9]+\.?[0-9]*$ ]]; then
        echo -e "${RED}无效输入，将使用默认速度 0.5 m/s${NC}"
        log_error "无效输入" "左移/右移速度输入: $speed，使用默认值0.5"
        speed=0.5
    fi
    
    # 计算方向
    if (( $(echo "$distance >= 0" | bc -l) )); then
        direction_text="左移"
    else
        direction_text="右移"
    fi
    
    # 确保速度与距离方向一致
    if (( $(echo "$distance * $speed < 0" | bc -l) )); then
        echo -e "${YELLOW}警告: 速度方向与距离方向不一致${NC}"
        echo -e "${YELLOW}将调整速度方向以匹配距离方向${NC}"
        speed=$(echo "-1 * $speed" | bc)
    fi
    
    # 计算时间
    if (( $(echo "$speed == 0" | bc -l) )); then
        echo -e "${RED}错误: 速度不能为0${NC}"
        log_error "速度为零" "左移/右移速度为零，无法计算时间"
        read_key_prompt _terminal_key "按任意键返回主菜单..."
        return
    fi
    
    time_needed=$(echo "scale=2; $distance / $speed" | bc)
    
    # 显示计划
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${GREEN}移动计划:${NC}"
    echo -e "  方向: ${GREEN}$direction_text${NC}"
    echo -e "  距离: ${GREEN}$distance${NC} 米"
    echo -e "  速度: ${GREEN}$speed${NC} 米/秒"
    echo -e "  预计时间: ${GREEN}$time_needed${NC} 秒"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    
    read_line confirm "确认执行? (y/n): "
    
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
        # 记录移动开始
        # 发送速度命令
        echo -e "${YELLOW}开始移动...${NC}"
        send_velocity 0.0 $speed 0.0
        
        # 计算并等待
        echo -e "${YELLOW}移动中，预计 ${time_needed} 秒...${NC}"
        
        # 使用sleep等待
        sleep $(echo "if ($time_needed < 0) -$time_needed else $time_needed" | bc)
        
        # 停止运动
        echo -e "${YELLOW}移动完成，停止...${NC}"
        send_velocity 0.0 0.0 0.0
        
        echo -e "${GREEN}✓ 移动完成${NC}"
    else
        echo -e "${RED}操作已取消${NC}"
    fi
    
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
}

# 显示主菜单
show_main_menu() {
    show_title
    echo -e "${WHITE}主菜单:${NC}"
    echo -e "  ${RED}请注意,1-5需要在行走或者跑步模式下!${NC}"
    echo -e "  ${GREEN}1.${NC} 设定前进/后退速度(键盘:W/S)"
    echo -e "  ${GREEN}2.${NC} 设定左移/右移速度(键盘:A/D)"
    echo -e "  ${GREEN}3.${NC} 设定左转/右转速度(键盘:Q/E)"
    echo -e "  ${GREEN}4.${NC} 前进/后退指定距离"
    echo -e "  ${GREEN}5.${NC} 左移/右移指定距离"
    echo -e "  ${GREEN}6.${NC} 设定机器人关节角速度"
    echo -e "  ${GREEN}7.${NC} 显示当前状态"
    echo -e "  ${CYAN}8.${NC} 查看日志信息"
    echo -e "  ${GREEN}9.${NC} 安全停止所有运动"
    echo -e "  ${RED}0.${NC} 退出系统"
    echo ""
}

# 显示当前状态
show_current_status() {
    echo ""
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo -e "${WHITE}          系统当前状态${NC}"
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    
    # 显示当前模式
    echo -e "当前控制模式: ${GREEN}$CURRENT_MODE${NC}"
    case $CURRENT_MODE in
        0) echo -e "模式描述: ${GREEN}被动/准备模式${NC}" ;;
        1) echo -e "模式描述: ${GREEN}阻尼模式${NC}" ;;
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
    if (( CURRENT_JOINT_INDEX >= 0 )); then
        echo -e "  最近关节角速度: ${GREEN}[${CURRENT_JOINT_INDEX}] ${JOINT_NAMES[$CURRENT_JOINT_INDEX]} = ${CURRENT_JOINT_ANGULAR_VELOCITY} rad/s${NC}"
    else
        echo -e "  最近关节角速度: ${YELLOW}未设置${NC}"
    fi
    
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
    
    echo -e "${WHITE}════════════════════════════════════════${NC}"
    echo ""
    read_key_prompt _terminal_key "按任意键返回主菜单..."
}

# 初始化
initialize_system() {
    clear
    echo -e "${PURPLE}运控主板运动控制系统 $VERSION${NC}"
    echo -e "${PURPLE}开发者: $DEVELOPER${NC}"
    echo -e "${PURPLE}当前时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo ""
    echo -e "${CYAN}初始化中...${NC}"
    
    # 初始化日志系统
    init_log_system
    
    # 检查ROS2环境
    if ! command -v ros2 &> /dev/null; then
        echo -e "${RED}错误: 未找到ROS2环境${NC}"
        echo -e "${YELLOW}请确保ROS2已正确安装并配置${NC}"
        log_error "ROS2环境未找到" "ros2命令不存在"
        exit 1
    fi
    
    # 检查bc是否安装
    if ! command -v bc &> /dev/null; then
        echo -e "${RED}错误: 未找到bc计算器${NC}"
        echo -e "${YELLOW}请安装bc: sudo apt-get install bc${NC}"
        log_error "bc计算器未找到" "bc命令不存在"
        exit 1
    fi
    
    # 发送初始状态消息
    send_velocity 0.0 0.0 0.0
    send_control_mode $MODE_STAND "系统初始化 - 进入站立模式"
    
    echo -e "${CYAN}检查系统连接...${NC}"
    log_system_event "系统连接检查" "开始"
    sleep 1
    
    echo -e "${GREEN}系统初始化完成${NC}"
    log_system_event "系统初始化完成" "所有组件就绪"
    sleep 1
}

# 主循环
main_loop() {
    # 记录系统启动
    log_system_event "系统启动" "控制脚本开始运行"
    
    while $SYSTEM_RUNNING; do
        show_main_menu
        
        echo -n -e "${WHITE}请选择操作[0-9]: ${NC}"
        read_line choice ""
        
     
        # 如果没有输入，则重新显示菜单
        if [[ -z "$choice" ]]; then
            continue
        fi
        
        case $choice in

            1) 
                set_forward_velocity 

                ;;
            2) 
                set_lateral_velocity 

                ;;
            3) 
                set_yaw_velocity 

                ;;
            4) 
                move_forward_backward 

                ;;
            5) 
                move_left_right 

                ;;
            6) 
                set_joint_angular_velocity

                ;;
            7)
                show_current_status

                ;;
            8)
                show_log_info

                ;;
            9)
                safe_stop 

                ;;
            0)
                echo -e "${YELLOW}正在退出系统...${NC}"
                # 记录系统关闭
                log_system_event "系统关闭" "控制脚本正常退出"
                # 安全停止
                safe_stop
                echo -e "${GREEN}系统已安全退出${NC}"
                SYSTEM_RUNNING=false
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 主程序
parse_args "$@"
check_first_start
initialize_system
main_loop
