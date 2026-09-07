#!/usr/bin/python3
"""
rtc_local.py
本地 RTC 控制程序
支持命令：
  READ
  SET YYYY-MM-DD HH:MM:SS
  QUIT

依赖：hwclock（util-linux）
SET 需要 root 权限
修改：指定 RTC 设备为 /dev/rtc1
"""

import subprocess
import datetime
import os
from pathlib import Path

# ---------- 自动定位项目根目录 ----------
def find_project_root() -> Path | None:
    """从当前目录向上查找，直到找到同时包含 external_tools 和 ros2_ws 的目录"""
    current = Path(__file__).resolve().parent
    for parent in [current] + list(current.parents):
        if (parent / "external_tools").exists() and (parent / "ros2_ws").exists():
            return parent
    return None

PROJECT_ROOT = find_project_root()

if PROJECT_ROOT is None:
    # 如果找不到，使用当前目录
    LOG_DIR = Path(__file__).resolve().parent / "logs"
    print(f"⚠️ 警告：未找到项目根目录，日志将保存在 {LOG_DIR}")
else:
    LOG_DIR = PROJECT_ROOT / "ros2_ws" / "run_logs" / "rtc"

# 创建日志目录
LOG_DIR.mkdir(parents=True, exist_ok=True)

# 日志文件路径（与 menu.py 中的 RTC_LOG 一致）
LOG_PATH = str(LOG_DIR / "rtc_log.log")

# 指定 RTC 设备文件路径
RTC_DEVICE = "/dev/rtc1"


def log_line(line: str):
    ts = datetime.datetime.now().isoformat(sep=' ')
    try:
        # 确保目录存在
        log_dir = os.path.dirname(LOG_PATH)
        if log_dir and not os.path.exists(log_dir):
            os.makedirs(log_dir, exist_ok=True)
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(f"{ts} | {line}\n")
    except Exception as e:
        print(f"⚠️ 写入日志失败: {e}")


def read_rtc():
    try:
        out = subprocess.check_output(
            ["hwclock", "--show", "-f", RTC_DEVICE],
            stderr=subprocess.STDOUT,
            text=True
        )
        line = out.strip().splitlines()[0]
        candidate = line.strip()
        try:
            dt = datetime.datetime.strptime(candidate[:19], "%Y-%m-%d %H:%M:%S")
            return dt.strftime("%Y-%m-%d %H:%M:%S")
        except Exception:
            return candidate
    except subprocess.CalledProcessError as e:
        return f"ERROR: hwclock failed: {e.output.strip()}"
    except FileNotFoundError:
        return "ERROR: hwclock not found. Install util-linux."


def set_rtc(datetime_str: str):
    try:
        proc = subprocess.run(
            ["hwclock", "--set", "--date", datetime_str, "-f", RTC_DEVICE],
            capture_output=True,
            text=True
        )
        if proc.returncode != 0:
            return False, proc.stderr.strip() or proc.stdout.strip()
        return True, "OK"
    except FileNotFoundError:
        return False, "ERROR: hwclock not found"
    except Exception as e:
        return False, str(e)


def main():
    print(f"📁 日志文件: {LOG_PATH}")
    print(f"🔧 RTC 设备: {RTC_DEVICE}")
    print()
    print("RTC LOCAL MODE. Commands: READ | SET YYYY-MM-DD HH:MM:SS | QUIT")
    print("=" * 60)

    while True:
        try:
            cmd = input("> ").strip()
        except EOFError:
            break

        if not cmd:
            continue

        if cmd.upper() == "READ":
            val = read_rtc()
            print(val)
            log_line(f"READ: {val}")

        elif cmd.upper().startswith("SET "):
            payload = cmd[4:].strip()
            try:
                datetime.datetime.strptime(payload, "%Y-%m-%d %H:%M:%S")
            except ValueError:
                print("❌ BAD FORMAT. Use: SET YYYY-MM-DD HH:MM:SS")
                continue

            ok, msg = set_rtc(payload)
            if ok:
                print("✅ SET OK")
                log_line(f"SET: {payload}")
            else:
                print("❌ SET FAILED:", msg)

        elif cmd.upper() == "QUIT":
            print("👋 BYE")
            break

        else:
            print("❌ UNKNOWN COMMAND")


if __name__ == "__main__":
    main()