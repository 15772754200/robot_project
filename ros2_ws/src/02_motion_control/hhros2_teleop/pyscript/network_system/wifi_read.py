
#!/usr/bin/env python3
import os
from pathlib import Path
import re

# 日志目录，可修改
LOG_DIR = Path("./run_logs/wifi")
# 日志文件名格式匹配（确保只识别wifi_log开头的日志）
LOG_PATTERN = re.compile(r"wifi_log.*\.log$")


def find_latest_log_file():
    """查找最近修改的 Wi-Fi 日志文件"""
    # 先检查日志目录是否存在
    if not LOG_DIR.exists():
        print(f"❌ 日志目录不存在：{LOG_DIR.resolve()}")
        return None
    
    # 筛选符合格式的日志文件（wifi_log*.log）
    log_files = [f for f in LOG_DIR.glob("wifi_log*.log") if f.is_file() and LOG_PATTERN.match(f.name)]
    if not log_files:
        return None
    
    # 按文件修改时间排序，取最新的一个
    latest_log = max(log_files, key=lambda f: f.stat().st_mtime)
    return latest_log


def read_recent_wifi_logs(max_entries=5):
    """读取最近日志，序号分隔并区分当前/历史连接"""
    latest_log = find_latest_log_file()
    if not latest_log:
        print("❌ 未找到任何 Wi-Fi 日志文件。")
        return

    # 打印日志文件信息
    log_time = latest_log.name.replace("wifi_log_", "").replace(".log", "").replace("_", " ")
    print(f"📘 最新日志文件（记录时间：{log_time}）")
    print(f"   文件路径：{latest_log.resolve()}\n")

    # 读取日志内容并按模块分割（当前连接、上一次连接、历史保存）
    with open(latest_log, "r", encoding="utf-8") as f:
        content = f.read().strip()

    # 定义模块分割标识（对应日志中的三个核心部分）
    modules = []
    # 1. 提取“当前连接 Wi-Fi”模块
    current_match = re.search(r"---- 当前连接 Wi-Fi ----(.*?)(?=---- 上一次连接|---- 历史保存|$)", content, re.DOTALL)
    if current_match:
        current_content = current_match.group(1).strip()
        if current_content != "无活动连接。":
            modules.append(("当前连接", current_content))

    # 2. 提取“上一次连接 Wi-Fi”模块（如果存在）
    prev_match = re.search(r"---- 上一次连接 Wi-Fi ----(.*?)(?=---- 历史保存|$)", content, re.DOTALL)
    if prev_match:
        prev_content = prev_match.group(1).strip()
        modules.append(("上一次连接（历史）", prev_content))

    # 3. 提取“历史保存 Wi-Fi”模块（按每条记录分割）
    history_match = re.search(r"---- 历史保存 Wi-Fi ----(.*)", content, re.DOTALL)
    if history_match:
        history_content = history_match.group(1).strip()
        if history_content != "无其他历史保存的Wi-Fi。":
            # 按行分割历史保存的Wi-Fi（每行一条）
            history_lines = [line.strip() for line in history_content.split("\n") if line.strip()]
            for line in history_lines:
                modules.append(("历史保存", line))

    # 处理无有效记录的情况
    if not modules:
        print("⚠️ 日志文件中没有有效的 Wi-Fi 连接记录。")
        return

    # 按“当前→上一次→历史保存”顺序输出，用序号分隔
    print("📋 Wi-Fi 连接记录（共 {} 条）\n".format(len(modules)))
    for idx, (conn_type, content) in enumerate(modules, 1):
        # 连接类型标识（用特殊符号区分，更直观）
        type_mark = {
            "当前连接": "",
            "上一次连接（历史）": "",
            "历史保存": ""
        }[conn_type]

        # 打印序号和连接类型
        print(f"{idx:2d}. {type_mark} {conn_type}")
        print("   " + "-" * 60)
        
        # 格式化输出记录内容（确保字段对齐）
        if conn_type in ["当前连接", "上一次连接（历史）"]:
            # 拆分多字段记录（SSID、MAC、状态等）
            fields = [field.strip() for field in content.split("|") if field.strip()]
            for field in fields:
                print(f"   {field}")
        else:
            # 历史保存的记录直接输出（单条一行）
            print(f"   {content}")
        
        # 每条记录之间空一行（增强可读性）
        print()


if __name__ == "__main__":
    # 可调整 max_entries 控制最大显示条数（默认5条，0表示显示全部）
    read_recent_wifi_logs(max_entries=5)