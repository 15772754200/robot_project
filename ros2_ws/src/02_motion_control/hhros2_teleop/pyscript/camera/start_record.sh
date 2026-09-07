echo "🔍 开始记录:"
UNIQUE_ID=$1
PATH_INSTALL=$2

python3  $PATH_INSTALL/pyscript/camera/record.py 

echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

python3 $PATH_INSTALL/pyscript/camera/kill_terminal.py $UNIQUE_ID
