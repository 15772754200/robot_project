
import os

# 最简单的读取方式
log_file = "run_logs/bluetooth/bluetooth_connections.log"

if os.path.exists(log_file):
    with open(log_file, 'r', encoding='utf-8') as f:
        print(f.read())
else:
    print(f"文件不存在: {log_file}")
    print(f"当前目录: {os.getcwd()}")