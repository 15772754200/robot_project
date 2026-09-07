#!/bin/bash

SET_COMMAND_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SET_COMMAND_SCRIPT_DIR/../common/terminal_io.sh"

# 机器人运动模式切换控制脚本
# 当前框架通过 hhros2_core 的 SetControlMode 服务切换模式。
# 模式切换前仍要求先进入站立模式。

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 保留原脚本的命令行参数变量。
TOPIC="/hhros2_core/set_control_mode"
UNIQUE_ID=""
EXE_PATH=""

# 当前框架模式定义：
# 0 passive, 1 damping, 2 stand, 3 walk, 4 run, 5 wbc, 6 motion
MODE_PASSIVE=0
MODE_DAMPING=1
MODE_STAND=2
MODE_WALK=3
MODE_RUN=4
MODE_WBC=5
MODE_MOTION=6

current_mode=0
last_stand_time=0
FORCE_STAND_FIRST=1

show_help() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人运动模式切换控制脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [选项] [UUID] [执行路径]"
    echo ""
    echo -e "${YELLOW}选项:${NC}"
    echo "  -t, --topic    设置模式服务名称 (默认: /hhros2_core/set_control_mode)"
    echo "  -h, --help     显示帮助信息"
    echo ""
    echo -e "${YELLOW}运动模式说明:${NC}"
    echo -e "  ${GREEN}0${NC}: 被动模式"
    echo -e "  ${GREEN}1${NC}: 阻尼模式"
    echo -e "  ${GREEN}2${NC}: 站立模式"
    echo -e "  ${GREEN}3${NC}: 行走模式"
    echo -e "  ${GREEN}4${NC}: 跑步模式"
    echo -e "  ${GREEN}5${NC}: WBC 模式"
    echo -e "  ${GREEN}6${NC}: 模仿"
    echo ""
    echo -e "${RED}重要安全规则:${NC}"
    echo "  任何非站立模式切换前必须先切换到站立模式(2)"
    echo "  不允许直接从其他运动模式切换到非站立模式"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 -t /hhros2_core/set_control_mode"
    echo -e "${GREEN}========================================${NC}\n"
}

parse_args() {
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
            --)
                shift
                REMAINING_ARGS=("$@")
                break
                ;;
            -*)
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
            *)
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

get_mode_description() {
    case "$1" in
        "$MODE_PASSIVE") echo "被动模式" ;;
        "$MODE_DAMPING") echo "阻尼模式" ;;
        "$MODE_STAND") echo "站立模式" ;;
        "$MODE_WALK") echo "行走模式" ;;
        "$MODE_RUN") echo "跑步模式" ;;
        "$MODE_WBC") echo "WBC 模式" ;;
        "$MODE_MOTION") echo "模仿" ;;
        *) echo "未知模式" ;;
    esac
}

send_control_command() {
    local mode="$1"
    local description="$2"
    local response

    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}         发送模式切换请求${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo -e "目标服务: ${YELLOW}$TOPIC${NC}"
    echo -e "控制模式: ${GREEN}$description${NC}"
    echo -e "消息类型: ${GREEN}hhros2_interfaces/srv/SetControlMode${NC}"
    echo -e "模式值:   ${GREEN}$mode${NC}"
    echo -e "${YELLOW}发送控制请求...${NC}"

    response="$(
        ros2 service call "$TOPIC" \
            hhros2_interfaces/srv/SetControlMode "{mode: $mode}" 2>&1
    )"
    local result=$?
    printf '%s\n' "$response"

    if [ $result -ne 0 ]; then
        echo -e "${RED}模式切换请求执行失败${NC}"
        return 1
    fi

    if grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        echo -e "${GREEN}模式切换请求成功${NC}"
        echo -e "${GREEN}服务: $TOPIC${NC}"
        echo -e "${GREEN}模式: $description${NC}"
        current_mode=$mode
        if [ "$mode" -eq "$MODE_STAND" ]; then
            last_stand_time=$(date +%s)
            FORCE_STAND_FIRST=0
        fi
        echo -e "${CYAN}========================================${NC}"
        return 0
    fi

    echo -e "${RED}模式切换被拒绝${NC}"
    echo -e "${YELLOW}请根据服务返回的 active_mode 和 message 检查当前系统状态${NC}"
    return 1
}

can_switch_to() {
    local target_mode="$1"

    if [ "$target_mode" -eq "$MODE_STAND" ]; then
        return 0
    fi

    if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
        echo -e "${RED}错误: 必须先切换到站立模式(2)${NC}"
        return 1
    fi

    if [ "$current_mode" -ne "$MODE_STAND" ]; then
        echo -e "${RED}错误: 当前不是站立模式，无法直接切换${NC}"
        return 1
    fi

    local current_time
    local time_since_stand
    current_time=$(date +%s)
    time_since_stand=$((current_time - last_stand_time))

    if [ "$time_since_stand" -gt 10 ]; then
        echo -e "${YELLOW}警告: 上次切换到站立模式已经超过 10 秒${NC}"
        echo -e "${YELLOW}是否继续? (y=继续，其他键取消): ${NC}"
        local confirm
        read_key confirm
        if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
            return 1
        fi
    fi

    return 0
}

show_status() {
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${CYAN}          运动模式控制状态${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo -e "模式服务: ${YELLOW}$TOPIC${NC}"
    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "会话 ID: ${BLUE}$UNIQUE_ID${NC}"
    fi
    echo ""

    if [ "$current_mode" -eq 0 ]; then
        echo -e "当前模式: ${RED}未初始化${NC}"
    else
        echo -e "当前模式: ${GREEN}$(get_mode_description "$current_mode") (模式$current_mode)${NC}"
    fi

    echo ""
    echo -e "${YELLOW}可用运动模式:${NC}"
    echo -e "  ${GREEN}0${NC}: 被动"
    echo -e "  ${GREEN}1${NC}: 阻尼"
    echo -e "  ${GREEN}2${NC}: 站立"
    echo -e "  ${GREEN}3${NC}: 行走"
    echo -e "  ${GREEN}4${NC}: 跑步"
    echo -e "  ${GREEN}5${NC}: WBC"
    echo -e "  ${GREEN}6${NC}: 模仿"
    echo ""

    if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
        echo -e "${RED}必须先切换到站立模式(2)${NC}"
    elif [ "$current_mode" -eq "$MODE_STAND" ]; then
        echo -e "${GREEN}当前为站立模式，可以切换到其他模式${NC}"
    else
        echo -e "${RED}必须先切换到站立模式，才能切换到其他运动模式${NC}"
    fi

    echo -e "${CYAN}========================================${NC}"
}

test_topic_existence() {
    echo -e "${YELLOW}检查服务 '$TOPIC' 是否可用...${NC}"

    if ! command -v ros2 >/dev/null 2>&1; then
        echo -e "${RED}ROS2 命令未找到${NC}"
        return 1
    fi

    if ros2 service list 2>/dev/null | grep -q "^${TOPIC}$"; then
        echo -e "${GREEN}服务 '$TOPIC' 可用${NC}"
        return 0
    fi

    echo -e "${RED}服务 '$TOPIC' 不存在${NC}"
    echo -e "${YELLOW}当前相关服务:${NC}"
    ros2 service list 2>/dev/null |
        grep -i "control_mode\|system_state" |
        head -10 ||
        echo "  (没有找到相关服务)"
    return 1
}

require_stand_mode_first() {
    if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
        echo -e "\n${RED}========================================${NC}"
        echo -e "${RED}          系统初始化要求${NC}"
        echo -e "${RED}========================================${NC}"
        echo -e "${YELLOW}必须先切换到站立模式才能继续${NC}"
        echo -e "${GREEN}请按 2 切换到站立模式，或按 X 退出${NC}\n"
        return 1
    fi
    return 0
}

initialize_stand_mode() {
    if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
        echo -e "\n${RED}========================================${NC}"
        echo -e "${RED}          系统初始化要求${NC}"
        echo -e "${RED}========================================${NC}"
        echo -e "${YELLOW}首先切换到站立模式进行系统初始化${NC}\n"

        local description
        description=$(get_mode_description "$MODE_STAND")
        send_control_command "$MODE_STAND" "$description"
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}系统初始化完成，可以切换到其他模式${NC}"
            return 0
        fi

        echo -e "${RED}系统初始化失败${NC}"
        return 1
    fi
    return 0
}

control_loop() {
    show_instructions

    echo -e "${YELLOW}检查模式服务连接...${NC}"
    if test_topic_existence; then
        echo -e "${GREEN}服务连接正常${NC}"
    else
        echo -e "${YELLOW}服务不可用，但继续进入控制界面...${NC}"
    fi
    echo ""

    if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
        echo -e "${RED}系统要求必须先切换到站立模式(2)${NC}"
    fi

    while true; do
        if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
            echo -e "${YELLOW}等待输入 (必须按 2 初始化，S:状态, T:检查, X:退出)...${NC}"
        else
            echo -e "${YELLOW}等待输入 (0-6:模式, S:状态, T:检查, X:退出)...${NC}"
        fi

        read_key key

        case "$key" in
            0)
                if can_switch_to "$MODE_PASSIVE"; then
                    send_control_command "$MODE_PASSIVE" "$(get_mode_description "$MODE_PASSIVE")"
                fi
                ;;
            1)
                if can_switch_to "$MODE_DAMPING"; then
                    send_control_command "$MODE_DAMPING" "$(get_mode_description "$MODE_DAMPING")"
                fi
                ;;
            2)
                send_control_command "$MODE_STAND" "$(get_mode_description "$MODE_STAND")"
                ;;
            3)
                if can_switch_to "$MODE_WALK"; then
                    send_control_command "$MODE_WALK" "$(get_mode_description "$MODE_WALK")"
                fi
                ;;
            4)
                if can_switch_to "$MODE_RUN"; then
                    send_control_command "$MODE_RUN" "$(get_mode_description "$MODE_RUN")"
                fi
                ;;
            5)
                if can_switch_to "$MODE_WBC"; then
                    send_control_command "$MODE_WBC" "$(get_mode_description "$MODE_WBC")"
                fi
                ;;
            6)
                if can_switch_to "$MODE_MOTION"; then
                    send_control_command "$MODE_MOTION" "$(get_mode_description "$MODE_MOTION")"
                fi
                ;;
            s|S)
                show_status
                ;;
            t|T)
                test_topic_existence
                ;;
            x|X)
                echo -e "${RED}退出运动模式控制程序${NC}"
                exit 0
                ;;
            *)
                if [ -n "$key" ]; then
                    echo -e "${YELLOW}无效按键，请按 0-6 选择模式${NC}"
                fi
                ;;
        esac
    done
}

safe_exit() {
    echo -e "${RED}退出运动模式控制程序${NC}"
    exit 0
}

show_instructions() {
    clear
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}      机器人运动模式切换控制${NC}"
    echo -e "${GREEN}========================================${NC}\n"

    if [[ -n "$UNIQUE_ID" ]]; then
        echo -e "${BLUE}[会话 ID: $UNIQUE_ID]${NC}\n"
    fi

    echo -e "${YELLOW}[控制命令]${NC}"
    echo -e "  ${GREEN}0${NC}: 被动模式"
    echo -e "  ${GREEN}1${NC}: 阻尼模式"
    echo -e "  ${GREEN}2${NC}: 站立模式"
    echo -e "  ${GREEN}3${NC}: 行走模式"
    echo -e "  ${GREEN}4${NC}: 跑步模式"
    echo -e "  ${GREEN}5${NC}: WBC 模式"
    echo -e "  ${GREEN}6${NC}: 模仿"
    echo -e "  ${GREEN}S${NC}: 查看状态"
    echo -e "  ${GREEN}T${NC}: 检查模式服务"
    echo -e "  ${RED}X${NC}: 退出"
    echo ""
    echo -e "${YELLOW}[控制服务]${NC}"
    echo -e "  模式服务: ${YELLOW}$TOPIC${NC}"
    echo ""

    if [ "$FORCE_STAND_FIRST" -eq 1 ]; then
        echo -e "${RED}当前状态: 必须先切换到站立模式(2)${NC}"
    else
        echo -e "${GREEN}当前状态: $(get_mode_description "$current_mode") (模式$current_mode)${NC}"
    fi

    echo -e "${GREEN}========================================${NC}\n"
}

cleanup() {
    echo -e "${RED}程序退出，运动模式控制结束${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

clear
echo -e "${GREEN}机器人运动模式切换控制脚本启动中...${NC}"

parse_args "$@"

echo -e "模式服务: ${YELLOW}$TOPIC${NC}"
echo -e "可用模式: ${GREEN}0:被动, 1:阻尼, 2:站立, 3:行走, 4:跑步, 5:WBC, 6:模仿${NC}"

control_loop
