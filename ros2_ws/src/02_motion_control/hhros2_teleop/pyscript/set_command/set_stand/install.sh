#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人站立控制脚本
# 功能：控制机器人进入站立状态 (模式2)，必须用户确认

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
    echo -e "${GREEN}      机器人站立控制脚本 (安全模式)${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic    设置控制模式服务 (默认: /hhros2_core/set_control_mode)"
    echo "  -h, --help     显示此帮助信息"
    echo ""
    echo -e "${YELLOW}功能:${NC}"
    echo "  启动后向指定话题发布站立控制消息 (模式2)"
    echo "  必须用户确认机器人处于准备姿态"
    echo ""
    echo -e "${YELLOW}安全要求:${NC}"
    echo "  ⚠ 发送站立命令前，请确保："
    echo "  1. 机器人处于准备姿态"
    echo "  2. 地面平整无障碍物"
    echo "  3. 周围有足够空间"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /robot1/hhros2_core/set_control_mode"
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

# 发送站立控制命令
send_stand_command() {
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${CYAN}         发布站立控制命令${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "目标话题: ${YELLOW}$TOPIC${NC}"
    echo -e "控制模式: ${GREEN}站立 (模式2)${NC}"
    echo -e "服务类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
    echo -e "数据值: ${GREEN}2${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    # 发送 ROS2 消息
    echo -e "${YELLOW}发布站立控制命令...${NC}"
    
    local response
    response="$(ros2 service call "$TOPIC" \
        hhros2_interfaces/srv/SetControlMode \
        "{mode: 2}" 2>&1)"
    printf '%s\n' "$response"
    
    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        echo -e "${GREEN}✓ 成功发布站立控制命令${NC}"
        echo -e "${GREEN}✓ 话题: $TOPIC${NC}"
        echo -e "${GREEN}✓ 消息: 站立 (模式2)${NC}"
    else
        echo -e "${RED}✗ 发布站立控制命令失败${NC}"
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
    echo -e "${CYAN}          站立控制状态${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""
    echo -e "控制模式: ${GREEN}站立 (模式2)${NC}"
    echo -e "描述: 机器人进入站立状态${NC}"
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
    if ros2 service list 2>/dev/null | grep -q "^${TOPIC}$"; then
        echo -e "${GREEN}✓ 话题 '$TOPIC' 存在${NC}"
        return 0
    else
        echo -e "${RED}✗ 话题 '$TOPIC' 不存在${NC}"
        echo -e "${YELLOW}可用的控制话题:${NC}"
        ros2 service list 2>/dev/null | grep -i "control\|gait\|mode" | head -10 || echo "  (没有找到相关服务)"
        return 1
    fi
}

# 必须的安全确认
require_safety_confirmation() {
    clear
    echo -e "${RED}═══════════════════════════════════════════════════${NC}"
    echo -e "${RED}                    ⚠ 重要安全警告 ⚠                     ${NC}"
    echo -e "${RED}═══════════════════════════════════════════════════${NC}\n"
    
    echo -e "${YELLOW}发送站立命令前，必须确认以下安全事项:${NC}\n"
    
    echo -e "${GREEN}1. 机器人当前姿态${NC}"
    echo -e "   ☐ 机器人处于准备姿态"
    echo -e "   ☐ 机器人处于安全位置"
    echo -e "   ☐ 机器人关节已初始化\n"
    
    echo -e "${GREEN}2. 环境安全${NC}"
    echo -e "   ☐ 地面平整无障碍物"
    echo -e "   ☐ 周围有足够站立空间 (至少1米半径)"
    echo -e "   ☐ 无人员在机器人运动范围内\n"
    
    echo -e "${GREEN}3. 系统状态${NC}"
    echo -e "   ☐ 电源连接稳定"
    echo -e "   ☐ 关节无异常报警"
    echo -e "   ☐ 控制系统正常"
    echo -e "   ☐ 急停按钮未被触发\n"
    
    echo -e "${RED}⚠ 如果以上任何条件不满足，请按N取消操作！${NC}\n"
    
    while true; do
        read_key_prompt robot_ready "机器人是否处于准备姿态？(y=是, n=否): "
        echo ""
        
        case $robot_ready in
            y|Y)
                echo -e "${GREEN}✓ 确认机器人处于准备姿态${NC}\n"
                break
                ;;
            n|N)
                echo -e "${RED}✗ 操作取消：机器人未处于准备姿态${NC}"
                return 1
                ;;
            *)
                echo -e "${YELLOW}请输入 y 或 n${NC}\n"
                ;;
        esac
    done
    
    while true; do
        read_key_prompt env_safe "环境是否安全？(y=是, n=否): "
        echo ""
        
        case $env_safe in
            y|Y)
                echo -e "${GREEN}✓ 确认环境安全${NC}\n"
                break
                ;;
            n|N)
                echo -e "${RED}✗ 操作取消：环境不安全${NC}"
                return 1
                ;;
            *)
                echo -e "${YELLOW}请输入 y 或 n${NC}\n"
                ;;
        esac
    done
    
    while true; do
        read_key_prompt system_ok "系统状态是否正常？(y=是, n=否): "
        echo ""
        
        case $system_ok in
            y|Y)
                echo -e "${GREEN}✓ 确认系统状态正常${NC}\n"
                break
                ;;
            n|N)
                echo -e "${RED}✗ 操作取消：系统状态异常${NC}"
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
    
    echo -e "${YELLOW}即将发送站立命令，请再次确认：${NC}\n"
    
    while true; do
        read_key_prompt final_confirm "发送站立命令？(y=确认发送, n=取消): "
        echo ""
        
        case $final_confirm in
            y|Y)
                echo -e "\n${GREEN}✓ 最终确认，开始发送站立命令...${NC}"
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
    echo -e "${GREEN}      机器人站立控制 (强制确认模式)${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}\n"
    
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话ID: $UNIQUE_ID]${NC}\n"
    fi
    
    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}S${NC}: 发送站立命令 (必须安全确认)"
    echo -e "  ${GREEN}C${NC}: 显示当前状态"
    echo -e "  ${GREEN}T${NC}: 检查话题是否存在"
    echo -e "  ${RED}X${NC}: 退出程序"
    echo ""
    echo -e "${YELLOW}[控制信息]${NC}"
    echo -e "  控制话题: ${YELLOW}$TOPIC${NC}"
    echo -e "  控制模式: ${GREEN}站立 (模式2)${NC}"
    echo ""
    echo -e "${RED}[安全要求]${NC}"
    echo -e "  ⚠ 发送站立命令前，必须确保："
    echo -e "  1. 机器人处于准备姿态"
    echo -e "  2. 地面平整无障碍物"
    echo -e "  3. 周围有足够空间"
    echo -e "  4. 机器人状态正常"
    echo -e "  (发送命令前会有多次确认)"
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
    
    # 首次启动不自动发送，等待用户确认
    echo -e "\n${RED}⚠ 注意: 脚本启动完成，需要手动发送站立命令${NC}"
    echo -e "${YELLOW}按 S 键发送站立命令 (必须通过安全确认)${NC}"
    
    while true; do
        echo -e "\n${YELLOW}等待输入命令 (S:发送站立命令, C:状态, T:检查话题, X:退出)...${NC}"
        read_key key
        
        case $key in
            s|S)  # 发送站立命令，必须安全确认
                echo -e "\n${YELLOW}准备发送站立命令，开始安全确认流程...${NC}"
                
                if require_safety_confirmation; then
                    if send_stand_command; then
                        echo -e "\n${GREEN}✓ 站立命令已成功发送${NC}"
                        echo -e "${YELLOW}请观察机器人状态，确保站立过程正常${NC}"
                    else
                        echo -e "\n${RED}✗ 发送站立命令失败${NC}"
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
                echo -e "${RED}退出站立控制程序${NC}"
                
                exit 0
                ;;
            *)
                if [ -n "$key" ]; then
                    echo -e "${YELLOW}无效按键，按 S 发送站立命令${NC}"
                fi
                ;;
        esac
    done
}

# 安全的退出函数
safe_exit() {
    echo -e "${RED}退出站立控制程序${NC}"
    exit 0
}

# 确保退出
cleanup() {
    echo -e "${RED}程序退出，站立控制结束${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# 主程序
clear
echo -e "${GREEN}机器人站立控制脚本 (强制确认模式) 启动中...${NC}"

# 首先解析所有参数
parse_args "$@"

echo -e "控制话题: ${YELLOW}$TOPIC${NC}"
echo -e "控制模式: ${GREEN}站立 (模式2)${NC}"

control_loop
