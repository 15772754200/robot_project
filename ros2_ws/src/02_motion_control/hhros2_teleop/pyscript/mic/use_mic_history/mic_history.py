#!/usr/bin/env python3
"""
麦克风使用记录查看脚本（由 lmich 调用）
功能：交互式查看日志，按 q 退出
"""
import sys
from pathlib import Path

# ---------- 自动定位日志路径 ----------
SCRIPT_DIR = Path(__file__).resolve().parent

def find_project_root() -> Path | None:
    """从当前目录向上查找，直到找到同时包含 external_tools 和 ros2_ws 的目录"""
    current = SCRIPT_DIR.resolve()
    for parent in [current] + list(current.parents):
        if (parent / "external_tools").exists() and (parent / "ros2_ws").exists():
            return parent
    return None

PROJECT_ROOT = find_project_root()
if PROJECT_ROOT is None:
    LOG_PATH = SCRIPT_DIR / "mic_use.log"
    print(f"警告：未找到项目根目录，日志将尝试读取 {LOG_PATH}")
else:
    LOG_PATH = PROJECT_ROOT / "ros2_ws" / "run_logs" / "mic" / "mic_use.log"

# ---------- 日志显示函数 ----------
def show_usage_log():
    """查询并打印麦克风使用记录"""
    print("\n=== 麦克风使用记录查询 ===")
    if not LOG_PATH.exists():
        print(f"日志文件不存在：{LOG_PATH}")
        print("当前还没有任何使用记录。")
        return

    try:
        with LOG_PATH.open("r", encoding="utf-8") as f:
            lines = f.readlines()
    except Exception as e:
        print("读取日志失败：", e)
        return

    if not lines:
        print("日志文件为空，没有记录。")
        return

    # 只显示最近 30 条，避免太长
    max_show = 30
    if len(lines) > max_show:
        print(f"日志共 {len(lines)} 条，显示最近 {max_show} 条：\n")
        lines_to_show = lines[-max_show:]
    else:
        print(f"日志共 {len(lines)} 条，全部显示：\n")
        lines_to_show = lines

    for i, line in enumerate(lines_to_show, 1):
        print(f"{i:02d}. {line.rstrip()}")

    print("\n=== 记录显示完毕，返回主菜单 ===\n")

# ---------- 主菜单 ----------
def main():
    print("===== 麦克风管理脚本 =====")
    print(f"日志文件路径：{LOG_PATH}")
    print("功能说明：")
    print("  1. 查询麦克风使用记录（查看 mic_use.log）")
    print("  q. 退出程序")

    while True:
        choice = input("\n请选择功能(1/q):").strip().lower()
        if choice == "1":
            show_usage_log()
        elif choice == "q":
            print("收到 q，退出程序。")
            break
        elif choice == "":
            continue
        else:
            print("无效的选择，请输入 1 或 q。")

    # 退出进程，关闭终端
    sys.exit(0)

if __name__ == "__main__":
    main()