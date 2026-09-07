SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"


PID_FILE="$BASE_DIR/pids/RTC.pid"
 if [ ! -f "$PID_FILE" ]; then
     echo "RTC时钟修改功能未运行"
     exit 0
 fi 

 PID=$(cat $PID_FILE)
 kill $PID
 rm -f $PID_FILE
 echo "RTC时钟修改功能已停止"