#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人系统停止控制脚本
# 功能：控制机器人停止 (发送话题/system，消息类型std_msgs/msg::Int32，消息值0)

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # 无颜色

# 默认值
TOPIC="/system"
MESSAGE_VALUE="0"  # 停止消息值为0
UNIQUE_ID=""
EXE_PATH=""

# 显示帮助
show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人系统停止控制脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic    设置话题名称 (默认: /system)"
    echo "  -h, --help     显示此帮助信息"
    echo ""
    echo -e "${YELLOW}功能:${NC}"
    echo "  启动后向/system话题发布停止控制消息"
    echo "  消息类型: std_msgs/msg/Int32"
    echo "  消息值: 0 (停止命令)"
    echo ""
    echo -e "${YELLOW}安全警告:${NC}"
    echo "  ⚠ 此命令将停止机器人系统，请谨慎使用！"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /robot1/system"
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

# 发送系统停止命令
send_system_stop_command() {
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}         发布系统停止命令${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "目标话题: ${YELLOW}$TOPIC${NC}"
    echo -e "消息类型: ${GREEN}std_msgs/msg/Int32${NC}"
    echo -e "消息值: ${RED}$MESSAGE_VALUE${NC}"
    echo -e "功能描述: ${RED}停止机器人系统${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    # 发送 ROS2 消息
    echo -e "${YELLOW}发布系统停止命令...${NC}"
    
    ros2 topic pub -1 $TOPIC std_msgs/msg/Int32 "{data: $MESSAGE_VALUE}" 2>/dev/null
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ 成功发布系统停止命令${NC}"
        echo -e "${GREEN}✓ 话题: $TOPIC${NC}"
        echo -e "${RED}✓ 消息值: $MESSAGE_VALUE${NC}"
        echo -e "${RED}✓ 功能: 停止机器人系统${NC}"
    else
        echo -e "${RED}✗ 发布系统停止命令失败${NC}"
        echo -e "${YELLOW}可能的原因:${NC}"
        echo -e "  1. ROS2 未启动"
        echo -e "  2. 话题 '$TOPIC' 不存在"
        echo -e "  3. 权限不足"
        return 1
    fi
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    return 0
}

# 显示当前状态
show_status() {
    echo -e "\n${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}          系统停止控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "消息类型: ${GREEN}std_msgs/msg/Int32${NC}"
    echo -e "消息值: ${RED}$MESSAGE_VALUE${NC}"
    echo -e "功能: ${RED}停止机器人系统${NC}"
    echo -e "当前状态: ${YELLOW}待发送...${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 测试话题是否存在
test_topic_existence() {
    echo -e "${YELLOW}检查话题 '$TOPIC' 是否存在...${NC}"
    
    if ! command -v ros2 &> /dev/null; then
        echo -e "${RED}✗ ROS2 命令未找到${NC}"
        return 1
    fi
    
    # 尝试列出话题
    if ros2 topic list 2>/dev/null | grep -q "^${TOPIC}$"; then
        echo -e "${GREEN}✓ 话题 '$TOPIC' 存在${NC}"
        return 0
    else
        echo -e "${RED}✗ 话题 '$TOPIC' 不存在${NC}"
        echo -e "${YELLOW}可用的系统话题:${NC}"
        ros2 topic list 2>/dev/null | grep -i "system\|stop\|shutdown\|power" | head -10 || echo "  (没有找到相关话题)"
        return 1
    fi
}

# 发送前的安全确认
require_safety_confirmation() {
    echo -e "${RED}═══════════════════════════════════════════════════${NC}"
    echo -e "${RED}                ⚠ 危险操作确认 ⚠                     ${NC}"
    echo -e "${RED}═══════════════════════════════════════════════════${NC}\n"
    
    echo -e "${YELLOW}即将发送系统停止命令，这将导致机器人系统停止运行！${NC}\n"
    
    echo -e "${RED}⚠ 重要警告:${NC}"
    echo -e "  - 机器人将立即停止所有运动"
    echo -e "  - 控制系统将关闭"
    echo -e "  - 可能需要重新启动才能恢复正常\n"
    
    echo -e "${GREEN}确认信息:${NC}"
    echo -e "  目标话题: $TOPIC"
    echo -e "  消息类型: std_msgs/msg/Int32"
    echo -e "  消息值: $MESSAGE_VALUE (停止命令)"
    echo -e "  功能: 停止机器人系统\n"
    
    # 第一层确认
    while true; do
        read_key_prompt confirm1 "确认您了解此操作的后果？(y=是, n=否): "
        echo ""
        
        case $confirm1 in
            y|Y)
                echo -e "${GREEN}✓ 确认了解操作后果${NC}\n"
                break
                ;;
            n|N)
                echo -e "${RED}✗ 操作取消：用户不了解操作后果${NC}"
                return 1
                ;;
            *)
                echo -e "${YELLOW}请输入 y 或 n${NC}\n"
                ;;
        esac
    done
    
    # 第二层确认
    while true; do
        read_key_prompt confirm2 "机器人当前是否已停止运动？(y=是, n=否): "
        echo ""
        
        case $confirm2 in
            y|Y)
                echo -e "${GREEN}✓ 确认机器人已停止运动${NC}\n"
                break
                ;;
            n|N)
                echo -e "${RED}✗ 操作取消：机器人未停止运动${NC}"
                return 1
                ;;
            *)
                echo -e "${YELLOW}请输入 y 或 n${NC}\n"
                ;;
        esac
    done
    
    # 最终确认
    echo -e "${RED}════════════════════════════════════════${NC}"
    echo -e "${RED}             ⚠ 最终确认 ⚠              ${NC}"
    echo -e "${RED}════════════════════════════════════════${NC}\n"
    
    while true; do
        read_key_prompt final_confirm "最后确认：发送系统停止命令？(y=确认发送, n=取消): "
        echo ""
        
        case $final_confirm in
            y|Y)
                echo -e "\n${GREEN}✓ 最终确认，发送系统停止命令...${NC}"
                return 0
                ;;
            n|N)
                echo -e "\n${RED}✗ 用户取消操作${NC}"
                return 1
                ;;
            *)
                echo -e "${YELLOW}请输入 y 或 n${NC}\n"
                ;;
        esac
    done
}

# 显示控制说明
show_instructions() {
    clear
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}      机器人系统停止控制${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${RED}S${NC}: 发送系统停止命令"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${GREEN}T${NC}: 检查话题是否存在"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo ""
    echo -e "${YELLOW}[控制信息]${NC}"
    echo -e "  控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "  消息类型: ${GREEN}std_msgs/msg/Int32${NC}"
    echo -e "  消息值: ${RED}$MESSAGE_VALUE${NC}"
    echo -e "  功能: ${RED}停止机器人系统${NC}"
    echo ""
    echo -e "${RED}[重要警告]${NC}"
    echo -e "  ⚠ 此命令将停止机器人系统！"
    echo -e "  ⚠ 操作不可逆，请谨慎使用！"
    echo -e "  ⚠ 发送前需要多重安全确认"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
}

# 主控制函数
control_loop() {
    show_instructions
    
    # 检查话题是否存在
    echo -e "${YELLOW}检查话题连接...${NC}"
    if test_topic_existence; then
        echo -e "${GREEN}✓ 话题连接正常${NC}"
    else
        echo -e "${YELLOW}⚠ 话题不存在，但继续尝试...${NC}"
    fi
    
    echo -e "\n${RED}⚠ 警告: 这是危险操作，请谨慎使用！${NC}"
    echo -e "${YELLOW}按 S 键发送系统停止命令 (需要多重安全确认)${NC}"
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (S:发送停止命令, C:状态, T:检查话题, X:退出)...${NC}"
        read_key key
        
        case $key in
            s|S)  # 发送系统停止命令
                echo -e "\n${RED}准备发送系统停止命令，开始安全确认流程...${NC}"
                
                if require_safety_confirmation; then
                    if send_system_stop_command; then
                        echo -e "\n${GREEN}✓ 系统停止命令已成功发送${NC}"
                        echo -e "${YELLOW}机器人系统将停止运行${NC}"
                        echo -e "${YELLOW}请等待系统完全停止${NC}"
                    else
                        echo -e "\n${RED}✗ 发送系统停止命令失败${NC}"
                    fi
                else
                    echo -e "\n${YELLOW}操作已取消，返回主菜单${NC}"
                fi
                ;;
            c|C)  # 显示状态
                show_status
                ;;
            t|T)  # 检查话题
                test_topic_existence
                ;;
            x|X)  # 退出
                echo -e "${RED}退出系统停止控制程序${NC}"
                
                exit 0
                ;;
            *)
                if [ -n "$key" ]; then
                    echo -e "${YELLOW}无效按键，按 S 发送系统停止命令${NC}"
                fi
                ;;
        esac
    done
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出系统停止控制程序${NC}"
    exit 0
}

# 确保退出
cleanup() {
    echo -e "${RED}程序退出，系统停止控制结束${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}机器人系统停止控制脚本启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
echo -e "消息类型: ${GREEN}std_msgs/msg/Int32${NC}"
echo -e "消息值: ${RED}$MESSAGE_VALUE${NC}"
echo -e "功能: ${RED}停止机器人系统"

control_loop