#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人跑步步态停止脚本
# 功能：直接发布停止跑步步态消息

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m' # 无颜色

# 默认值
TOPIC="/hhros2_core/set_control_mode"
UNIQUE_ID=""
EXE_PATH=""
MESSAGE_COUNT=1  # 默认发送1次消息
DELAY_BETWEEN_MSGS=0.1  # 消息之间的延迟（秒）

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      跑步步态停止控制脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic      设置控制模式服务名称 (默认: /hhros2_core/set_control_mode)"
    echo "  -c, --count      设置消息发送次数 (默认: 1)"
    echo "  -d, --delay      设置消息间延迟 (默认: 0.1 秒)"
    echo "  -h, --help       显示此帮助信息"
    echo ""
    echo -e "${YELLOW}功能:${NC}"
    echo "  启动后直接向指定话题发布停止步态消息 (模式2)"
    echo "  用于立即停止机器人的跑步步态"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0                          # 使用默认设置发送一次停止命令"
    echo "  $0 -t /hhros2_core/set_control_mode  # 发送到指定服务"
    echo "  $0 -c 3 -d 0.2              # 发送3次，间隔0.2秒"
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
            -c|--count)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    if [[ $2 =~ ^[0-9]+$ ]] && [ $2 -gt 0 ]; then
                        MESSAGE_COUNT="$2"
                        shift 2
                    else
                        echo -e "${RED}错误: 发送次数必须是正整数${NC}"
                        exit 1
                    fi
                else
                    echo -e "${RED}错误: -c/--count 需要一个参数${NC}"
                    exit 1
                fi
                ;;
            -d|--delay)
                if [[ -n "$2" && ! "$2" =~ ^- ]]; then
                    if [[ $2 =~ ^[0-9]*\.?[0-9]+$ ]] && (( $(echo "$2 > 0" | bc -l) )); then
                        DELAY_BETWEEN_MSGS="$2"
                        shift 2
                    else
                        echo -e "${RED}错误: 延迟必须是正数${NC}"
                        exit 1
                    fi
                else
                    echo -e "${RED}错误: -d/--delay 需要一个参数${NC}"
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

# 发送跑步步态停止命令的函数
send_stop_running_command() {
    local attempt=$1
    local total=$2
    
    echo -e "${YELLOW}发送第 $attempt/$total 次停止跑步命令...${NC}"
    
    # 发送 ROS2 消息
    local response
    response="$(ros2 service call "$TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: 2}" 2>&1)"
    printf '%s\n' "$response"
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        echo -e "  ${GREEN}✓ 成功发送停止跑步命令${NC}"
        echo -e "  ${GREEN}  服务: $TOPIC${NC}"
        echo -e "  ${GREEN}  消息: 停止跑步步态 (模式2)${NC}"
        return 0
    else
        echo -e "  ${RED}✗ 发送失败${NC}"
        return 1
    fi
}

# 单次发送停止命令
send_single_stop() {
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}         发布跑步停止命令${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "目标服务: ${YELLOW}$TOPIC${NC}"
    echo -e "控制模式: ${RED}停止跑步步态 (模式2)${NC}"
    echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
    echo -e "数据值: ${PURPLE}2${NC}"
    echo -e "发送次数: ${GREEN}1${NC} 次"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    send_stop_running_command 1 1
    
    if [ $? -eq 0 ]; then
        echo -e "\n${GREEN}✓ 跑步步态停止命令已发送${NC}"
    else
        echo -e "\n${RED}✗ 发送跑步步态停止命令失败${NC}"
    fi
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    return 0
}

# 多次发送以确保消息到达
send_multiple_stop_commands() {
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}    发布跑步停止命令 (多次发送)${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "目标服务: ${YELLOW}$TOPIC${NC}"
    echo -e "控制模式: ${RED}停止跑步步态 (模式2)${NC}"
    echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
    echo -e "数据值: ${PURPLE}2${NC}"
    echo -e "发送次数: ${GREEN}$MESSAGE_COUNT${NC} 次"
    echo -e "发送间隔: ${GREEN}$DELAY_BETWEEN_MSGS${NC} 秒"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    local success=0
    
    for i in $(seq 1 $MESSAGE_COUNT); do
        send_stop_running_command $i $MESSAGE_COUNT
        
        if [ $? -eq 0 ]; then
            success=$((success+1))
        fi
        
        # 除了最后一次，每次发送后等待
        if [ $i -lt $MESSAGE_COUNT ]; then
            sleep $DELAY_BETWEEN_MSGS
        fi
    done
    
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    if [ $success -gt 0 ]; then
        echo -e "\n${GREEN}✓ 成功发送 $success/$MESSAGE_COUNT 次跑步停止命令${NC}"
        return 0
    else
        echo -e "\n${RED}✗ 所有发送尝试都失败${NC}"
        return 1
    fi
}

# 显示当前配置
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          跑步步态停止控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制模式服务: ${YELLOW}$TOPIC${NC}"
    echo -e "发送次数: ${GREEN}$MESSAGE_COUNT${NC}"
    echo -e "发送间隔: ${GREEN}$DELAY_BETWEEN_MSGS${NC} 秒"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
    echo -e "数据值: ${PURPLE}2${NC}"
    echo -e "描述: ${RED}停止机器人的跑步步态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 测试话题是否存在
test_topic_existence() {
    echo -e "${YELLOW}检查服务 '$TOPIC' 是否存在...${NC}"
    
    # 尝试列出话题
    if ros2 service list 2>/dev/null | grep -q "^${TOPIC}$"; then
        echo -e "${GREEN}✓ 话题 '$TOPIC' 存在${NC}"
        return 0
    else
        echo -e "${RED}✗ 话题 '$TOPIC' 不存在${NC}"
        echo -e "${YELLOW}可用的步态控制模式服务:${NC}"
        ros2 service list 2>/dev/null | grep -i "control\|gait\|mode" || true
        return 1
    fi
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}      跑步步态停止控制脚本${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}1${NC}: 发送一次跑步停止命令"
    echo -e "  ${GREEN}2${NC}: 发送多次跑步停止命令"
    echo -e "  ${GREEN}S${NC}: 显示当前状态"
    echo -e "  ${GREEN}T${NC}: 检查服务是否存在"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo ""
    echo -e "${YELLOW}[配置信息]${NC}"
    echo -e "  控制模式服务: ${YELLOW}$TOPIC${NC}"
    echo -e "  数据值: ${PURPLE}2${NC} (停止跑步步态)"
    echo -e "  发送次数: ${GREEN}$MESSAGE_COUNT${NC} 次"
    echo -e "  发送间隔: ${GREEN}$DELAY_BETWEEN_MSGS${NC} 秒"
    echo ""
    echo -e "${YELLOW}[模式说明]${NC}"
    echo -e "  ${PURPLE}模式2${NC}: 停止跑步步态 - 立即停止机器人的跑步动作"
    echo -e "  ${GREEN}模式3${NC}: 跑步步态 - 机器人的跑步模式"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出跑步步态停止控制程序${NC}"
    
    # 发送最后一次停止命令
    echo -e "${YELLOW}发送最终停止命令...${NC}"
    send_stop_running_command 1 1
    
    # 等待一小会儿确保消息发送完成
    sleep 0.5
    
    exit 0
}

# 主控制函数
control_loop() {
    show_instructions
    
    # 脚本启动后自动发送一次停止命令
    echo -e "${GREEN}脚本启动，自动发送跑步步态停止命令...${NC}"
    send_single_stop
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (1:单次发送, 2:多次发送, S:状态, T:检查服务, X:退出)...${NC}"
        read_key key
        
        case $key in
            1)  # 发送一次跑步停止命令
                send_single_stop
                ;;
            2)  # 发送多次跑步停止命令
                read_line count_input "请输入发送次数 (当前: $MESSAGE_COUNT): "
                if [[ $count_input =~ ^[0-9]+$ ]] && [ $count_input -gt 0 ]; then
                    MESSAGE_COUNT=$count_input
                else
                    echo -e "${RED}错误: 发送次数必须是正整数${NC}"
                fi
                
                read_line delay_input "请输入发送间隔秒数 (当前: $DELAY_BETWEEN_MSGS): "
                if [[ $delay_input =~ ^[0-9]*\.?[0-9]+$ ]] && (( $(echo "$delay_input > 0" | bc -l) )); then
                    DELAY_BETWEEN_MSGS=$delay_input
                else
                    echo -e "${RED}错误: 延迟必须是正数${NC}"
                fi
                
                send_multiple_stop_commands
                ;;
            s|S)  # 显示状态
                show_status
                ;;
            t|T)  # 检查服务
                test_topic_existence
                ;;
            x|X)  # 退出
                safe_exit
                ;;
            *)
                if [ -n "$key" ]; then
                    echo -e "${YELLOW}无效按键，按 1 发送停止命令${NC}"
                fi
                ;;
        esac
    done
}

# 确保退出时发送停止命令
cleanup() {
    echo -e "${RED}程序退出，跑步步态停止控制结束${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}════════════════════════════════════════${NC}"
echo -e "${GREEN}      跑步步态停止控制脚本${NC}"
echo -e "${GREEN}════════════════════════════════════════${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制模式服务: ${YELLOW}$TOPIC${NC}"
echo -e "控制模式: ${RED}停止跑步步态 (模式2)${NC}"
echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
echo -e "${GREEN}════════════════════════════════════════${NC}"
echo ""

# 检查服务是否存在
echo -e "${YELLOW}检查服务连接...${NC}"
if test_topic_existence; then
    echo -e "${GREEN}✓ 服务连接正常${NC}"
else
    echo -e "${YELLOW}⚠ 服务不存在，但继续尝试...${NC}"
fi

echo ""

control_loop
