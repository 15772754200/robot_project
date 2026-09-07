
import os
import sys
import time
import json
import subprocess
from datetime import datetime
from pathlib import Path
import logging
import getpass
from typing import Dict, List, Tuple, Optional

def close_specific_terminal(unique_id):
    try:
        result = subprocess.run(['pgrep', '-f', f'终端管理器_{unique_id}'], capture_output=True, text=True, check=True)
        pids = result.stdout.strip().split()
        print(f"匹配到的进程 ID 列表: {pids}")
        if pids:
            for pid in pids:
                print(f"正在关闭终端窗口（ID: {pid}，标识符: {unique_id}）")
                subprocess.run(['kill', pid], check=True)
            return True
        else:
            print(f"未找到标识符为 {unique_id} 的终端窗口")
    except subprocess.CalledProcessError as e:
        print(f"关闭终端命令执行失败: {e.stderr}")
    except Exception as e:
        print(f"关闭终端失败: {e}")
    return False    

def main():
    if len(sys.argv) < 2:
        print("请提供终端唯一标识符作为参数（如：python3 script.py 8318e7d9）")
        sys.exit(1)
    unique_id = sys.argv[1]   
    close_specific_terminal(unique_id)

if __name__ == "__main__":
    main()