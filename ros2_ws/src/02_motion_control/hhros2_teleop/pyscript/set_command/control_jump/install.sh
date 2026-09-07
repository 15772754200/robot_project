#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人跳跃控制脚本
# 功能：控制机器人跳跃 (发送控制模式4)

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # 无颜色

# 默认值
TOPIC="/hhros2_core/set_control_mode"
UNIQUE_ID=""
EXE_PATH=""

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人跳跃控制脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic    设置控制模式服务名称 (默认: /hhros2_core/set_control_mode)"
    echo "  -h, --help     显示此帮助信息"
    echo ""
    echo -e "${YELLOW}功能:${NC}"
    echo "  当前框架没有独立跳跃模式，因此本脚本不会发送控制命令"
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

# 发送跳跃控制命令
send_jump_command() {
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}         发布跳跃控制命令${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "目标服务: ${YELLOW}$TOPIC${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    echo -e "${RED}✗ 当前框架没有独立跳跃模式，未发送控制命令${NC}"
    return 1
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    return 0
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          跳跃控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制模式服务: ${YELLOW}$TOPIC${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "控制模式: ${GREEN}跳跃 (模式4)${NC}"
    echo -e "描述: 机器人执行跳跃动作${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 测试服务是否存在
test_topic_existence() {
    echo -e "${YELLOW}检查服务 '$TOPIC' 是否存在...${NC}"
    
    if ! command -v ros2 &> /dev/null; then
        echo -e "${RED}✗ ROS2 命令未找到${NC}"
        return 1
    fi
    
    if ros2 service list 2>/dev/null | grep -q "^${TOPIC}$"; then
        echo -e "${GREEN}✓ 服务 '$TOPIC' 存在${NC}"
        return 0
    else
        echo -e "${RED}✗ 服务 '$TOPIC' 不存在${NC}"
        echo -e "${YELLOW}可用的控制模式服务:${NC}"
        ros2 service list 2>/dev/null | grep -i "control\|gait\|mode" | head -10 || echo "  (没有找到相关服务)"
        return 1
    fi
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}      机器人跳跃控制脚本${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}S${NC}: 重新发送跳跃命令"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${GREEN}T${NC}: 检查服务是否存在"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo ""
    echo -e "${YELLOW}[控制信息]${NC}"
    echo -e "  控制模式服务: ${YELLOW}$TOPIC${NC}"
    echo -e "  控制模式: ${GREEN}跳跃 (模式4)${NC}"
    echo ""
    echo -e "${YELLOW}[注意]${NC}"
    echo -e "  脚本启动后已自动发送跳跃命令"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
}

# 主控制函数
control_loop() {
    show_instructions
    
    # 检查服务是否存在
    echo -e "${YELLOW}检查服务连接...${NC}"
    if test_topic_existence; then
        echo -e "${GREEN}✓ 服务连接正常${NC}"
    else
        echo -e "${YELLOW}⚠ 服务不存在，但继续尝试...${NC}"
    fi
    
    echo ""
    
    # 脚本启动后立即发送跳跃命令
    echo -e "${GREEN}脚本启动，自动发送跳跃控制命令...${NC}"
    send_jump_command
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ 跳跃命令已发送${NC}"
    else
        echo -e "${RED}✗ 发送跳跃命令失败，继续控制程序${NC}"
    fi
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (S:重新发送, C:状态, T:检查服务, X:退出)...${NC}"
        read_key key
        
        case $key in
            s|S)  # 重新发送跳跃命令
                send_jump_command
                ;;
            c|C)  # 显示状态
                show_status
                ;;
            t|T)  # 检查服务
                test_topic_existence
                ;;
            x|X)  # 退出
                echo -e "${RED}退出跳跃控制程序${NC}"
                
                exit 0
                ;;
            *)
                if [ -n "$key" ]; then
                    echo -e "${YELLOW}无效按键，按 S 重新发送跳跃命令${NC}"
                fi
                ;;
        esac
    done
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出跳跃控制程序${NC}"
    exit 0
}

# 确保退出
cleanup() {
    echo -e "${RED}程序退出，跳跃控制结束${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}机器人跳跃控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制模式服务: ${YELLOW}$TOPIC${NC}"
echo -e "控制模式: ${GREEN}跳跃 (模式4)${NC}"

control_loop
