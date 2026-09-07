#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

PID_FILE="$BASE_DIR/pids/bluetooth.pid"

# 防止重复启动（如果 PID 存在但进程不在，清理掉）
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if [ -n "$PID" ] && ps -p "$PID" > /dev/null 2>&1; then
        echo "蓝牙已在运行"
        exit 0
    else
        rm -f "$PID_FILE"
    fi
fi

cd "$BASE_DIR/services/bluetooth"

python3 bluetooth.py &

echo "蓝牙启动完成"
