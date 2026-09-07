运行前准备（Ubuntu）

在终端运行以下命令安装依赖：

sudo apt update
sudo apt install -y python3-dbus python3-gi
# （如果你用的是较新的环境或虚拟环境，也可以用 pip 安装 dbus-next，但上面 apt 的 dbus + gi 是最稳妥的）


将脚本设置为可执行：

chmod +x bt_logger.py


然后以普通用户运行（不一定需要 root，但需能访问 system bus；通常普通用户即可）：

./bt_logger.py


运行后你会看到：

Bluetooth logger running. Logging to: /path/to/bluetooth_connections.txt


此时脚本在前台监听 DBus，任何设备一旦连接（Connected 变成 True），会把一行写入 bluetooth_connections.txt，格式例如：

2025-10-15T15:12:03.123456 | MyPhone | AA:BB:CC:11:22:33


如果你需要后台运行（示例）：

nohup ./bt_logger.py >/dev/null 2>&1 &


（注意：nohup 只是示例；你也可以写 systemd service 来长期运行。）