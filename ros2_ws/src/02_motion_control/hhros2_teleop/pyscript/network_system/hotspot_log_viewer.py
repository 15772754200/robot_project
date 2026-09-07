#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import glob
import argparse

# 日志目录和前缀
LOG_DIR = "run_logs/hotspot/"
LOG_PREFIX = "hotspot_log"   # 会匹配 hotspot_log.txt 和 hotspot_log_*.txt


def find_latest_log():
    """
    查找最新的热点日志文件：
    匹配 /var/log/hotspot_log*，然后按修改时间取最新的一个
    """
    pattern = os.path.join(LOG_DIR, f"{LOG_PREFIX}*")
    files = glob.glob(pattern)
    if not files:
        return None
    # 取修改时间最大的那个文件
    latest = max(files, key=os.path.getmtime)
    return latest


def print_log(log_path, keyword=None):
    """
    打印日志文件：
    - 如果 keyword 为 None，则打印全部内容
    - 否则只打印包含关键字的行
    """
    try:
        with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.rstrip("\n")
                if keyword and keyword not in line:
                    continue
                print(line)
    except FileNotFoundError:
        print(f"❌ 未找到日志文件: {log_path}")
    except PermissionError:
        print(f"⚠️ 没有权限读取日志文件: {log_path}，请尝试使用 sudo 运行。")


def main():
    parser = argparse.ArgumentParser(
        description="热点日志查询工具（默认打印最新的热点日志）"
    )
    parser.add_argument(
        "-f", "--file",
        help="指定要读取的日志文件路径（默认自动选择最新的 hotspot 日志）"
    )
    parser.add_argument(
        "-k", "--keyword",
        help="按关键字过滤日志行（可选）"
    )
    args = parser.parse_args()

    # 1）如果用户指定了文件，就用指定的
    if args.file:
        log_path = args.file
    else:
        # 2）否则自动找最新的
        log_path = find_latest_log()

    if not log_path:
        print(f"⚠️ 未在 {LOG_DIR} 下找到任何 {LOG_PREFIX}* 日志文件")
        return

    print(f"🔍 正在读取日志文件: {log_path}")
    if args.keyword:
        print(f"   仅显示包含关键字 “{args.keyword}” 的行\n")
    else:
        print()

    print_log(log_path, args.keyword)


if __name__ == "__main__":
    main()
