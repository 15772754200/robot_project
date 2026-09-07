#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"


PID_FILE="$BASE_DIR/pids/remotecontrol.pid"

if [ -f "$PID_FILE" ]; then
    echo "遥控功能已在运行"
    exit 0
fi

cd $BASE_DIR/services/remotecontrol
g++ -std=c++17 -O2 websocket_server.cpp -o websocket_server -lboost_system -lpthread 
./websocket_server &
echo "遥控功能启动完成"