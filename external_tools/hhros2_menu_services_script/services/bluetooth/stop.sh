SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"


PID_FILE="$BASE_DIR/pids/bluetooth.pid"
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if [ -n "$PID" ] && ps -p "$PID" > /dev/null 2>&1; then
        kill "$PID"
    fi
    rm -f "$PID_FILE"
    echo "蓝牙已停止"
else
    echo "蓝牙未在运行"
fi
 