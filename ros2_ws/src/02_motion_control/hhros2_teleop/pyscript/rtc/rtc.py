#!/usr/bin/env python3
"""
rtc_server.py
简单 TCP 服务：支持 READ / SET YYYY-MM-DD HH:MM:SS / QUIT
依赖：hwclock (通常已在 ubuntu 中, 包含于 util-linux)
注意：SET 需要 root 权限。
"""

import socket
import threading
import subprocess
import datetime
import shlex
import os

HOST = "0.0.0.0"
PORT = 12345
LOG_PATH = "run_logs/rtc/rtc_log.log"

def log_line(line: str):
    ts = datetime.datetime.now().isoformat(sep=' ')
    """自动创建目录和文件 """
    log_dir = os.path.dirname(LOG_PATH)
    if log_dir and not os.path.exists(log_dir):
        os.makedirs(log_dir, exist_ok=True)

    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(f"{ts} | {line}\n")

def read_rtc():
    """使用 hwclock --show 读取 RTC，返回 ISO datetime 字符串或错误信息"""
    try:
        # hwclock --show 输出示例: "2025-10-20 11:23:45.123456+00:00"
        out = subprocess.check_output(["hwclock", "--show"], stderr=subprocess.STDOUT, text=True)
        # 保守解析：取第一行，去掉小数秒及时区（若存在），返回 YYYY-MM-DD HH:MM:SS
        line = out.strip().splitlines()[0]
        # 兼容 hwclock 格式：包含小数秒或时区等，提取前 19 个字符如果可能
        candidate = line.strip()
        # 查找类似 YYYY- 的起点
        # 尝试解析多种形式：
        for fmt in ("%Y-%m-%d %H:%M:%S", "%a %b %d %H:%M:%S %Y"):  # fallback
            try:
                dt = datetime.datetime.strptime(candidate[:19], "%Y-%m-%d %H:%M:%S")
                return dt.strftime("%Y-%m-%d %H:%M:%S")
            except Exception:
                continue
        # 最后直接返回 hwclock 的原始输出（经过 strip）
        return candidate
    except subprocess.CalledProcessError as e:
        return f"ERROR: hwclock failed: {e.output.strip()}"
    except FileNotFoundError:
        return "ERROR: hwclock not found. Install util-linux."

def set_rtc(datetime_str: str):
    """使用 hwclock --set --date "<datetime_str>" 写入 RTC。
    datetime_str 例如： "2025-10-20 11:23:45"
    返回 (True/False, message)
    """
    try:
        # hwclock 要求类似 "2025-10-20 11:23:45"
        cmd = ["hwclock", "--set", "--date", datetime_str]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            return False, proc.stderr.strip() or proc.stdout.strip()
        # 为确保硬件时钟更新（将系统时间写入硬件时钟或反之）通常会调用 --hctosys 或 --systohc
        # 这里仅设置 RTC；若需要同步系统时间，请另外运行 timedatectl 或 hwclock --hctosys / --systohc。
        return True, "OK"
    except FileNotFoundError:
        return False, "ERROR: hwclock not found. Install util-linux."
    except Exception as e:
        return False, f"Exception: {e}"

def handle_client(conn, addr):
    with conn:
        conn.sendall(b"RTC SERVER READY. Commands: READ | SET YYYY-MM-DD HH:MM:SS | QUIT\n")
        while True:
            data = conn.recv(1024)
            if not data:
                break
            text = data.decode('utf-8', errors='ignore').strip()
            if not text:
                continue
            if text.upper() == "READ":
                val = read_rtc()
                log_line(f"READ from {addr}: {val}")
                conn.sendall((str(val) + "\n").encode())
            elif text.upper().startswith("SET "):
                payload = text[4:].strip()
                # basic validation: expect 'YYYY-MM-DD HH:MM:SS'
                try:
                    # allow a couple common formats
                    dt = datetime.datetime.strptime(payload, "%Y-%m-%d %H:%M:%S")
                    ok, msg = set_rtc(payload)
                    if ok:
                        log_line(f"SET from {addr}: {payload}")
                        conn.sendall(b"SET OK\n")
                    else:
                        conn.sendall(("SET FAILED: " + msg + "\n").encode())
                except ValueError:
                    conn.sendall(b"BAD FORMAT. Use: SET YYYY-MM-DD HH:MM:SS\n")
            elif text.upper() == "QUIT":
                conn.sendall(b"BYE\n")
                break
            else:
                conn.sendall(b"UNKNOWN COMMAND\n")

def start_server(host=HOST, port=PORT):
    print(f"Starting RTC server on {host}:{port} . Log: {LOG_PATH}")
    log_line(f"SERVER START on {host}:{port}")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen(5)
        try:
            while True:
                conn, addr = s.accept()
                print("Conn from", addr)
                t = threading.Thread(target=handle_client, args=(conn, addr), daemon=True)
                t.start()
        except KeyboardInterrupt:
            print("Server shutting down.")
            log_line("SERVER STOPPED")

if __name__ == "__main__":
    start_server()