#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"


PID_FILE="$BASE_DIR/pids/enthernet.pid"

if [ -f "$PID_FILE" ]; then
    echo "有线网络功能已在运行"
    exit 0
fi

cd $BASE_DIR/services/enthernet
./network.py &

echo "有线网络功能启动完成"