#!/usr/bin/env python3
"""
麦克风录音脚本（由 enmic 调用）
功能：自动配置环境、启动录音，录音结束后自动退出
"""
import sys
import os
import subprocess
import threading
from pathlib import Path
from datetime import datetime

# ---------- 参数处理 ----------
# 如果传入了 UNIQUE_ID，可用于标识终端（可忽略或用于日志）
UNIQUE_ID = sys.argv[1] if len(sys.argv) > 1 else "unknown"

# ---------- 路径配置 ----------
SCRIPT_DIR = Path(__file__).resolve().parent          # use_mic 目录
PROJECT_ROOT = SCRIPT_DIR.parent.parent.parent.parent.parent  # 向上5级到项目根（根据实际调整）

# 或者更可靠的方式：向上查找 external_tools
def find_project_root() -> Path | None:
    current = SCRIPT_DIR.resolve()
    for parent in [current] + list(current.parents):
        if (parent / "external_tools").exists() and (parent / "ros2_ws").exists():
            return parent
    return None

PROJECT_ROOT = find_project_root()
if PROJECT_ROOT is None:
    print("错误：未找到项目根目录（包含 external_tools 和 ros2_ws）")
    sys.exit(1)

# 外部工具目录
EXTERNAL_TOOLS = PROJECT_ROOT / "external_tools"
HHROS2_MIC = EXTERNAL_TOOLS / "hhros2_mic"
SETUP_SCRIPT = HHROS2_MIC / "setup_mic.sh"
SDK_BIN_DIR = HHROS2_MIC / "M2_SDK" / "offline_mic" / "bin"
RECORD_CMD = ["./record_sample"]

# 日志路径（统一到 ros2_ws/run_logs/）
LOG_DIR = PROJECT_ROOT / "ros2_ws" / "run_logs"  / "mic"
LOG_PATH = LOG_DIR / "mic_use.log"

# ---------- 环境配置检测 ----------
def is_environment_ready() -> bool:
    """检查环境是否已配置"""
    if not (Path("/usr/local/include/cjson/cJSON.h").exists() and Path("/usr/local/lib/libcjson.so").exists()):
        return False
    if not (SDK_BIN_DIR / "record_sample").exists():
        return False
    rules = list(Path("/etc/udev/rules.d/").glob("*ch9102*"))
    if not rules:
        return False
    return True

def run_setup() -> bool:
    """执行环境配置脚本"""
    if not SETUP_SCRIPT.exists():
        print(f"错误：找不到 {SETUP_SCRIPT}")
        return False
    print("\n===== 环境未配置，执行自动配置（可能需要 sudo 密码）=====")
    try:
        os.chmod(SETUP_SCRIPT, 0o755)
        proc = subprocess.Popen(
            ["sudo", str(SETUP_SCRIPT)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        for line in proc.stdout:
            print(line, end="")
        proc.wait()
        if proc.returncode != 0:
            print(f"配置失败，返回码 {proc.returncode}")
            return False
        print("环境配置完成。")
        return True
    except Exception as e:
        print(f"执行配置时出错：{e}")
        return False

# ---------- 日志功能 ----------
def log_mic_usage(status: str):
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{now}] record_sample {status} (ID: {UNIQUE_ID})\n"
    try:
        LOG_DIR.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as f:
            f.write(line)
    except Exception as e:
        print("写入日志失败：", e)

# ---------- 录音会话 ----------
def record_session():
    print(f"\n=== 录音会话开始 (ID: {UNIQUE_ID}) ===")
    print(f"工作目录：{SDK_BIN_DIR}")
    print("输入 q 回车 停止录音，Ctrl+C 也可终止。")
    
    if not SDK_BIN_DIR.exists():
        print(f"错误：SDK 目录不存在 {SDK_BIN_DIR}")
        return
    
    try:
        log_mic_usage("START")
        proc = subprocess.Popen(
            RECORD_CMD,
            cwd=str(SDK_BIN_DIR),
        )
    except FileNotFoundError:
        print(f"错误：找不到 {SDK_BIN_DIR / 'record_sample'}")
        log_mic_usage("FAILED: FileNotFound")
        return
    except Exception as e:
        print(f"启动录音失败：{e}")
        log_mic_usage(f"FAILED: {type(e).__name__}")
        return

    stop_flag = {"stopped": False}

    def keyboard_watch():
        while True:
            try:
                s = input()
            except EOFError:
                break
            if s.strip().lower() == "q":
                if not stop_flag["stopped"]:
                    print("收到 q，停止录音...")
                    stop_flag["stopped"] = True
                    log_mic_usage("STOP")
                    try:
                        proc.terminate()
                    except Exception:
                        pass
                break

    t = threading.Thread(target=keyboard_watch, daemon=True)
    t.start()

    try:
        ret = proc.wait()
        if not stop_flag["stopped"]:
            log_mic_usage(f"EXIT_CODE={ret}")
            print(f"录音进程退出，返回码：{ret}")
    except KeyboardInterrupt:
        if not stop_flag["stopped"]:
            print("\n收到 Ctrl+C，停止录音...")
            stop_flag["stopped"] = True
            log_mic_usage("STOP")
        try:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        except Exception:
            pass
    
    print("=== 录音会话结束 ===")

# ---------- 主入口 ----------
def main():
    # 检查环境
    if not is_environment_ready():
        if not run_setup():
            print("环境配置失败，请手动执行 sudo ./setup_mic.sh 后再试。")
            sys.exit(1)
    else:
        print("环境已就绪。")

    # 开始录音
    record_session()
    
    # 录音结束，退出进程
    print("录音会话结束，退出。")
    sys.exit(0)

if __name__ == "__main__":
    main()