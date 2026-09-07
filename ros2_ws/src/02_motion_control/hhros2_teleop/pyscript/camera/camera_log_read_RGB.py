import argparse
import os
import glob
import re
from datetime import datetime
from pathlib import Path

def find_latest_log_file(ros_log_dir=None):
    """找到最新的 realsense_start_RGB_YYYY-MM-DD_HH-MM-SS.log 日志文件"""
    ros_log_dir = ros_log_dir or Path("run_logs/camera/")
    if not os.path.exists(ros_log_dir):
        print(f"❌ ROS日志目录不存在: {ros_log_dir}")
        return None

    # 支持递归，因为有时候 ~/.ros/log/ 会有子目录
    pattern = os.path.join(ros_log_dir, "**", "realsense_start_RGB_*.log")
    log_files = glob.glob(pattern, recursive=True)
    log_files = [f for f in log_files if os.path.isfile(f)]

    if not log_files:
        print("❌ 未找到符合格式的日志文件（realsense_start_YYYY-MM-DD_HH-MM-SS.log）")
        return None

    def parse_log_datetime(path):
        m = re.search(r"realsense_start_RGB_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})\.log", path)
        if not m:
            return datetime.min
        return datetime.strptime(m.group(1), "%Y-%m-%d_%H-%M-%S")

    # 按文件名里的时间排序，最新的在前
    log_files.sort(key=lambda p: parse_log_datetime(p), reverse=True)
    latest = log_files[0]

    print(f"✅ 找到最新的日志文件: {os.path.basename(latest)}")
    print(f"📂 路径: {latest}\n")
    return latest


def main():
    parser = argparse.ArgumentParser(description="打印最新的 RealSense 启动日志")
    parser.add_argument(
        "-l", "--log-dir",
        help="ROS 日志根目录（默认 程序根目录/run_logs/camera/）"
    )
    parser.add_argument(
        "-f", "--file",
        help="指定要打印的日志文件，指定后不再自动搜索最新日志"
    )
    args = parser.parse_args()

    if args.file:
        log_file = args.file if os.path.exists(args.file) else None
        if not log_file:
            print(f"❌ 指定的日志文件不存在: {args.file}")
            return
    else:
        log_file = find_latest_log_file(args.log_dir)
        if not log_file:
            return

    # 直接打印原始内容
    try:
        with open(log_file, "r", encoding="utf-8") as f:
            content = f.read()
        print("📄 日志内容")
        print("=" * 70)
        print(content)
    except Exception as e:
        print(f"❌ 读取日志失败: {e}")


if __name__ == "__main__":
    main()

