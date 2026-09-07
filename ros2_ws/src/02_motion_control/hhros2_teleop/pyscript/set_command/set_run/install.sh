#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人跑步步态控制脚本
# 功能：控制机器人启动跑步步态模式

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
DEFAULT_MODE=4
CURRENT_MODE=0
UNIQUE_ID=""
EXE_PATH=""
CLEANUP_CALLED=false  # 标记是否已调用清理

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人跑步步态控制${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic      设置控制模式服务 (默认: /hhros2_core/set_control_mode)"
    echo "  -h, --help       显示此帮助信息"
    echo ""
    echo -e "${YELLOW}控制命令:${NC}"
    echo -e "  ${GREEN}3${NC}    切换到跑步步态模式 (模式4)"
    echo -e "  ${GREEN}2${NC}    切换到停止步态模式 (模式2)"
    echo -e "  ${GREEN}c${NC}    显示当前状态"
    echo -e "  ${RED}x${NC}    退出程序"
    echo ""
    echo -e "${YELLOW}工作流程:${NC}"
    echo "  1. 先发布模式2（停止步态）"
    echo "  2. 等待2秒"
    echo "  3. 再发布模式4（跑步步态）"
    echo "  4. 机器人切换到跑步步态模式"
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

# 发送控制模式命令的函数
send_control_mode() {
    local mode=$1
    local description=$2
    
    local response
    response="$(ros2 service call "$TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: $mode}" 2>&1)"
    printf '%s\n' "$response"
    
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        CURRENT_MODE=$mode
        case $mode in
            2)
                echo -e "${YELLOW}✓ 发送控制模式: 停止步态 (模式=$mode)${NC}"
                ;;
            4)
                echo -e "${GREEN}✓ 发送控制模式: 跑步步态 (模式=$mode)${NC}"
                ;;
            *)
                echo -e "${BLUE}✓ 发送控制模式: 模式=$mode${NC}"
                ;;
        esac
        if [[ -n "$description" ]]; then
            echo -e "描述: $description${NC}"
        fi
    else
        echo -e "${RED}✗ 发送控制模式失败${NC}"
        return 1
    fi
    return 0
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          跑步步态控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    
    echo -e "当前控制模式: ${GREEN}$CURRENT_MODE${NC}"
    
    case $CURRENT_MODE in
        2)
            echo -e "状态: ${YELLOW}停止步态模式${NC}"
            echo -e "描述: 机器人处于停止状态${NC}"
            ;;
        3)
            echo -e "状态: ${GREEN}跑步步态模式${NC}"
            echo -e "描述: 机器人正在使用跑步步态移动${NC}"
            ;;
        *)
            echo -e "状态: ${BLUE}未知模式${NC}"
            echo -e "描述: 当前模式值: $CURRENT_MODE${NC}"
            ;;
    esac
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 切换到跑步步态模式（完整的切换流程）
switch_to_running_gait() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          切换到跑步步态模式${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "切换流程:"
    echo -e "  1. 先发布停止步态 (模式2)"
    echo -e "  2. 等待2秒"
    echo -e "  3. 再发布跑步步态 (模式4)"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    # 步骤1: 发布停止步态 (模式2)
    echo -e "\n${YELLOW}步骤1: 发布停止步态命令...${NC}"
    send_control_mode 2 "停止步态模式"
    
    # 步骤2: 等待2秒
    echo -e "\n${YELLOW}步骤2: 等待2秒...${NC}"
    for i in {2..1}; do
        echo -e "倒计时: ${RED}$i${NC} 秒"
        sleep 1
    done
    echo -e "等待完成"
    
    # 步骤3: 发布跑步步态 (模式4)
    echo -e "\n${YELLOW}步骤3: 发布跑步步态命令...${NC}"
    send_control_mode 4 "跑步步态模式"
    
    echo -e "\n${GREEN}✓ 切换流程完成！机器人已切换到跑步步态模式${NC}"
}

# 切换到停止步态模式
switch_to_stop_gait() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          切换到停止步态模式${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "模式值: ${YELLOW}2${NC}"
    echo -e "功能: 切换到停止步态模式${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    send_control_mode 2 "停止步态模式"
    echo -e "${GREEN}✓ 已切换到停止步态模式${NC}"
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}      机器人跑步步态控制${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}3${NC}: 切换到跑步步态模式 (模式4)"
    echo -e "  ${GREEN}2${NC}: 切换到停止步态模式 (模式2)"
    echo -e "  ${GREEN}C${NC}: 显示状态"
    echo -e "  ${RED}X${NC}: 退出"
    echo ""
    echo -e "${YELLOW}[模式说明]${NC}"
    echo -e "  ${YELLOW}模式2${NC}: 停止步态 - 机器人停止移动"
    echo -e "  ${GREEN}模式4${NC}: 跑步步态 - 机器人使用跑步步态移动"
    echo ""
    echo -e "${YELLOW}[切换流程]${NC}"
    echo -e "  选择按键3时会执行完整切换流程:"
    echo -e "  1. 先发布模式2 (停止步态)"
    echo -e "  2. 等待2秒"
    echo -e "  3. 再发布模式4 (跑步步态)"
    echo ""
    echo -e "${YELLOW}[默认设置]${NC}"
    echo -e "  控制话题: ${YELLOW}$TOPIC${NC}"
    echo ""
    
    case $CURRENT_MODE in
        2)
            echo -e "${YELLOW}[状态] 停止步态模式${NC}"
            ;;
        4)
            echo -e "${GREEN}[状态] 跑步步态模式${NC}"
            ;;
        *)
            echo -e "${BLUE}[状态] 当前模式: $CURRENT_MODE${NC}"
            ;;
    esac
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出跑步步态控制程序${NC}"
    
    # 标记清理已被调用
    CLEANUP_CALLED=true
    
    # 发送停止命令
    echo -e "${YELLOW}发送停止命令确保机器人安全停止...${NC}"
    send_control_mode 2 "安全停止"
    
    # 等待一小会儿确保消息发送完成
    sleep 0.5
    
    exit 0
}

# 主控制函数
control_loop() {
    show_instructions
    
    # 脚本启动后自动执行完整的切换流程
    echo -e "${GREEN}脚本启动，开始执行步态切换流程...${NC}"
    switch_to_running_gait
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (3:跑步步态, 2:停止步态, C:状态, X:退出)...${NC}"
        read_key key
        
        case $key in
            3)  # 切换到跑步步态模式（完整流程）
                switch_to_running_gait
                show_instructions
                ;;
            2)  # 切换到停止步态模式
                switch_to_stop_gait
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
    
    # 发送停止消息
    echo -e "${RED}程序退出，发送停止命令...${NC}"
    send_control_mode 2 "安全停止"
    echo -e "${RED}已停止跑步步态并退出${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}机器人跑步步态控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
echo -e "注意: 脚本启动后将自动执行完整的步态切换流程"
echo ""

control_loop
