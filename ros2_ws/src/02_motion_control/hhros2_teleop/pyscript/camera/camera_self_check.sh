echo "🔍 相机测试:"
UNIQUE_ID=$1
PATH_INSTALL=$2

python3  $PATH_INSTALL/pyscript/camera/camera_self_check.py 

echo ""
read -p "按任意键返回主控终端 " -n 1 -r
echo

python3 $PATH_INSTALL/pyscript/camera/kill_terminal.py $UNIQUE_ID