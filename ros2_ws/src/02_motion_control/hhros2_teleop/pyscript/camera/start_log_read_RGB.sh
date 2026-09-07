echo "🔍 读取log:"
UNIQUE_ID=$1
PATH_INSTALL=$2

python3  $PATH_INSTALL/pyscript/camera/camera_log_read_RGB.py


echo ""
read -p "按任意键返回主控终端 " -r dummy_var
echo

python3 $PATH_INSTALL/pyscript/camera/kill_terminal.py $UNIQUE_ID