#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人停止步态控制脚本
# 功能：直接发布停止步态消息

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # 无颜色

# 默认值
TOPIC="/hhros2_core/set_control_mode"
UNIQUE_ID=""
EXE_PATH=""

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人停止步态控制脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic      设置控制模式服务名称 (默认: /hhros2_core/set_control_mode)"
    echo "  -h, --help       显示此帮助信息"
    echo ""
    echo -e "${YELLOW}功能:${NC}"
    echo "  启动后直接调用指定控制模式服务切换到站立状态 (模式2)"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /hhros2_core/set_control_mode"
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

# 发送停止步态命令的函数
send_stop_gait() {
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}         发布停止步态命令${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "目标服务: ${YELLOW}$TOPIC${NC}"
    echo -e "控制模式: ${RED}停止步态 (模式2)${NC}"
    echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    # 发送 ROS2 消息
    echo -e "${YELLOW}发布停止步态命令...${NC}"
    
    local response
    response="$(ros2 service call "$TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: 2}" 2>&1)"
    printf '%s\n' "$response"
    
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        echo -e "${GREEN}✓ 成功发布停止步态命令${NC}"
        echo -e "${GREEN}✓ 服务: $TOPIC${NC}"
        echo -e "${GREEN}✓ 消息: 停止步态 (模式2)${NC}"
    else
        echo -e "${RED}✗ 发布停止步态命令失败${NC}"
        echo -e "${YELLOW}可能的原因:${NC}"
        echo -e "  1. ROS2 未启动"
        echo -e "  2. 服务 '$TOPIC' 不存在"
        echo -e "  3. 权限不足"
        return 1
    fi
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    return 0
}

# 多次发送以确保消息到达
send_stop_gait_repeatedly() {
    local count=3
    local success=0
    
    echo -e "${YELLOW}发送停止步态命令 (重复 $count 次确保到达)...${NC}"
    
    for i in $(seq 1 $count); do
        echo -e "\n${YELLOW}发送第 $i 次...${NC}"
        local response
        response="$(ros2 service call "$TOPIC" \
            hhros2_interfaces/srv/SetControlMode \
            "{mode: 2}" 2>&1)"
        printf '%s\n' "$response"
        
        if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
            echo -e "  ${GREEN}✓ 第 $i 次发送成功${NC}"
            success=$((success+1))
        else
            echo -e "  ${RED}✗ 第 $i 次发送失败${NC}"
        fi
        
        # 除了最后一次，每次发送后等待0.1秒
        if [ $i -lt $count ]; then
            sleep 0.1
        fi
    done
    
    if [ $success -gt 0 ]; then
        echo -e "\n${GREEN}✓ 成功发送 $success/$count 次停止步态命令${NC}"
        return 0
    else
        echo -e "\n${RED}✗ 所有发送尝试都失败${NC}"
        return 1
    fi
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          停止步态控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制模式服务: ${YELLOW}$TOPIC${NC}"
    echo -e "发送消息: ${RED}停止步态 (模式2)${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "服务类型: hhros2_interfaces/srv/SetControlMode"
    echo -e "数据值: 2"
    echo -e "描述: 机器人切换到停止步态模式"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 测试服务是否存在
test_topic_existence() {
    echo -e "${YELLOW}检查服务 '$TOPIC' 是否存在...${NC}"
    
    if ros2 service list 2>/dev/null | grep -q "^${TOPIC}$"; then
        echo -e "${GREEN}✓ 服务 '$TOPIC' 存在${NC}"
        return 0
    else
        echo -e "${RED}✗ 服务 '$TOPIC' 不存在${NC}"
        echo -e "${YELLOW}可用的控制模式服务:${NC}"
        ros2 service list 2>/dev/null | grep -i "control\|gait\|mode" || true
        return 1
    fi
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出停止步态控制程序${NC}"
    
    exit 0
}

# 主控制函数
control_loop() {
    echo -e "${GREEN}机器人停止步态控制脚本启动中...${NC}"
    echo ""
    
    # 检查服务是否存在
    if ! test_topic_existence; then
        echo -e "\n${YELLOW}继续尝试发送停止步态命令...${NC}"
    fi
    
    # 显示控制说明
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          停止步态控制菜单${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "  ${GREEN}1${NC}: 发送一次停止步态命令"
    echo -e "  ${GREEN}2${NC}: 多次发送停止步态命令"
    echo -e "  ${GREEN}S${NC}: 显示当前状态"
    echo -e "  ${GREEN}R${NC}: 检查服务是否存在"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (1:单次发送, 2:多次发送, S:状态, R:检查服务, X:退出)...${NC}"
        read_key key
        
        case $key in
            1)  # 发送一次停止步态命令
                send_stop_gait
                ;;
            2)  # 多次发送停止步态命令
                send_stop_gait_repeatedly
                ;;
            s|S)  # 显示状态
                show_status
                ;;
            r|R)  # 检查服务
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

# 确保退出时停止机器人
cleanup() {
    echo -e "${RED}程序退出，停止步态控制结束${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}════════════════════════════════════════${NC}"
echo -e "${GREEN}      机器人停止步态控制脚本${NC}"
echo -e "${GREEN}════════════════════════════════════════${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制模式服务: ${YELLOW}$TOPIC${NC}"
echo -e "控制模式: ${RED}停止步态 (模式2)${NC}"
echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
echo -e "${GREEN}════════════════════════════════════════${NC}"
echo ""

# 立即发送一次停止步态命令
send_stop_gait

echo ""
control_loop
