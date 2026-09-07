#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PID_FILE="$BASE_DIR/pids/enthernet.pid"

if [ ! -f "$PID_FILE" ]; then
    echo "有线网络功能未运行"
    exit 0
fi

PID="$(cat "$PID_FILE")"

if [ -z "$PID" ] || ! kill -0 "$PID" 2>/dev/null; then
    echo "进程不存在，清理 PID 文件"
    rm -f "$PID_FILE"
    exit 0
fi

kill "$PID"
rm -f "$PID_FILE"

echo "有线网络功能已停止"
