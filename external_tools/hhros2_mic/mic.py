#!/usr/bin/env python3
import subprocess
import sys
import os
import argparse
from pathlib import Path
from datetime import datetime
import threading

# ---------- 路径配置 ----------
SCRIPT_DIR = Path(__file__).resolve().parent          # hhros2_mic 目录
SETUP_SCRIPT = SCRIPT_DIR / "setup_mic.sh"            # 环境配置脚本

def find_project_root() -> Path | None:
    """从当前脚本目录向上查找，直到找到同时包含 external_tools 和 ros2_ws 的目录（项目根）"""
    current = SCRIPT_DIR.resolve()
    for parent in [current] + list(current.parents):
        if (parent / "external_tools").exists() and (parent / "ros2_ws").exists():
            return parent
    return None

PROJECT_ROOT = find_project_root()
if PROJECT_ROOT is None:
    LOG_DIR = SCRIPT_DIR / "logs"
    print(f"警告：未找到项目根目录（包含 external_tools 和 ros2_ws），日志将保存在 {LOG_DIR}")
else:
    LOG_DIR = PROJECT_ROOT / "ros2_ws" / "run_logs"

LOG_PATH = LOG_DIR / "mic_use.log"
SDK_BIN_DIR = SCRIPT_DIR / "M2_SDK" / "offline_mic" / "bin"
RECORD_CMD = ["./record_sample"]

# ---------- 环境配置检测 ----------
def is_environment_ready():
    """检查环境是否已配置（参考 setup_mic.sh 的检测逻辑）"""
    if not (Path("/usr/local/include/cjson/cJSON.h").exists() and Path("/usr/local/lib/libcjson.so").exists()):
        return False
    if not (SDK_BIN_DIR / "record_sample").exists():
        return False
    rules = list(Path("/etc/udev/rules.d/").glob("*ch9102*"))
    if not rules:
        return False
    return True

def run_setup():
    """执行环境配置脚本（sudo）"""
    if not SETUP_SCRIPT.exists():
        print(f"错误：找不到环境配置脚本 {SETUP_SCRIPT}")
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
            print(f"配置失败，返回码 {proc.returncode}，请手动执行 sudo ./setup_mic.sh")
            return False
        print("环境配置完成。")
        return True
    except Exception as e:
        print(f"执行配置时出错：{e}")
        return False

# ---------- 日志功能 ----------
def log_mic_usage(status: str):
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{now}] record_sample {status}\n"
    try:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as f:
            f.write(line)
    except Exception as e:
        print("写入日志失败：", e)

# ---------- 录音会话 ----------
def record_session():
    print("\n=== 录音会话开始 ===")
    print(f"工作目录：{SDK_BIN_DIR}")
    print("输入 q 回车 停止录音并结束会话，Ctrl+C 也可终止。")
    
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
    print("=== 录音会话结束 ===\n")

# ---------- 日志查询 ----------
def show_usage_log():
    print("\n=== 麦克风使用记录 ===")
    if not LOG_PATH.exists():
        print("暂无记录。")
        return
    try:
        with LOG_PATH.open("r", encoding="utf-8") as f:
            lines = f.readlines()
    except Exception as e:
        print("读取日志失败：", e)
        return
    if not lines:
        print("日志为空。")
        return
    max_show = 30
    if len(lines) > max_show:
        print(f"共 {len(lines)} 条，显示最近 {max_show} 条：\n")
        lines_to_show = lines[-max_show:]
    else:
        print(f"共 {len(lines)} 条：\n")
        lines_to_show = lines
    for i, line in enumerate(lines_to_show, 1):
        print(f"{i:02d}. {line.rstrip()}")
    print()

# ---------- 菜单模式 ----------
def menu_mode():
    print("===== 麦克风管理脚本 =====")
    print(f"日志文件：{LOG_PATH}")
    print("功能：")
    print("  1. 开始录音")
    print("  2. 查看使用记录")
    print("  q. 退出")
    while True:
        choice = input("\n请选择 (1/2/q)：").strip().lower()
        if choice == "1":
            record_session()
            # 录音结束后，留在菜单界面，不退出终端（如果希望也退出，可取消注释下一行）
            # sys.exit(0)
        elif choice == "2":
            show_usage_log()
        elif choice == "q":
            print("退出。")
            sys.exit(0)   # 直接退出进程，关闭终端
        elif choice == "":
            continue
        else:
            print("无效输入，请输入 1, 2 或 q。")

# ---------- 主入口 ----------
def main():
    parser = argparse.ArgumentParser(description="麦克风控制脚本")
    parser.add_argument(
        "mode",
        nargs="?",
        choices=["start", "menu", "log"],
        default="menu",
        help="运行模式：start 直接录音，menu 交互菜单，log 显示日志"
    )
    args = parser.parse_args()

    # 检查环境
    if not is_environment_ready():
        if not run_setup():
            print("环境配置失败，请手动执行 sudo ./setup_mic.sh 后再试。")
            sys.exit(1)   # 失败也退出
    else:
        print("环境已就绪。")

    mode = args.mode
    if mode == "start":
        record_session()
        print("录音会话结束，退出。")
        sys.exit(0)   # 强制退出，关闭终端
    elif mode == "log":
        show_usage_log()
        # 查询完成后，回到主菜单或退出？这里设计为只显示日志，然后退出
        sys.exit(0)   # 关闭终端（如果需要保留，可注释掉）
    else:  # menu
        menu_mode()
    # 如果菜单正常结束（如用户没按q而退出循环），也会到这里，确保退出
    sys.exit(0)

if __name__ == "__main__":
    main()