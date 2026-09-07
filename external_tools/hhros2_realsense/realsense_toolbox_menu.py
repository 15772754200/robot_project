#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
realsense_toolbox_menu.py
========================

把你现有的 RealSense 相关脚本统一到一个入口菜单里：
启动本脚本 -> 选择功能 -> 自动调用对应脚本。

同目录下需要存在：
- start.sh             : 启动 realsense2_camera (ros2 launch...)，并写 run_logs/realsense/realsense_start_*/realsense_start_*.log
- camera_menu.py       : 运行时调参 + topic 选择/echo
- record.py            : 订阅图像话题，拍照/录像，写入对应启动目录
- camera_log_read.py   : 打印最新（或指定）的 realsense_start_*.log
- camera_test.py       : pyrealsense2 直连测试（不依赖 ROS2）
"""

from __future__ import annotations

import os
import sys
import subprocess
from pathlib import Path
from typing import List
import stat as _stat
from realsense_error_utils import run_log_root


def _here() -> Path:
    return Path(__file__).resolve().parent


def _py() -> str:
    return sys.executable or "python3"


def _exists(p: Path) -> bool:
    return p.exists() and p.is_file()


def run_cmd(cmd: List[str], *, check: bool = False) -> int:
    """运行子进程命令，返回退出码。"""
    print("\n" + "=" * 78)
    print("→ 执行：", " ".join(cmd))
    print("=" * 78)
    try:
        r = subprocess.run(cmd, check=check)
        return int(r.returncode)
    except subprocess.CalledProcessError as e:
        print(f"❌ 命令失败，返回码={e.returncode}")
        return int(e.returncode)
    except FileNotFoundError:
        print("❌ 找不到可执行文件/命令：", cmd[0])
        return 127


def warn_if_root() -> None:
    if os.geteuid() == 0:
        sudo_user = os.environ.get("SUDO_USER")
        if sudo_user:
            print("⚠️ 你正在以 root 身份运行（可能是 sudo 运行的）。")
        else:
            print("⚠️ 你正在以 root 身份运行。")
        print(f"   注意：请确认当前用户对统一日志目录有写权限：{run_log_root()}\n")


def ensure_executable(p: Path) -> None:
    """尽量给脚本加可执行位（失败也无妨）。"""
    try:
        st = p.stat()
        p.chmod(st.st_mode | _stat.S_IXUSR | _stat.S_IXGRP | _stat.S_IXOTH)
    except Exception:
        pass


# -------------------------- 动作：调用你的已有脚本 --------------------------

def action_start_camera(base: Path) -> None:
    """运行 start.sh（它本身会交互选择分辨率/FPS/点云等）。"""
    p = base / "start.sh"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    ensure_executable(p)
    run_cmd(["bash", str(p)])


def action_runtime_param_and_topic(base: Path) -> None:
    """运行 camera_menu.py（运行时调参 + topic echo）。"""
    p = base / "camera_menu.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    run_cmd([_py(), str(p)])


def action_record_photo_video(base: Path) -> None:
    """运行 record.py（拍照/录像）。"""
    p = base / "record.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    run_cmd([_py(), str(p)])


def action_read_startup_log(base: Path) -> None:
    """运行 camera_log_read.py（默认打印最新日志，也可输入路径）。"""
    p = base / "camera_log_read.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return

    file_path = input("输入日志文件路径（回车=自动读取最新）：").strip()
    log_dir = ""
    if not file_path:
        log_dir = input(f"指定日志根目录（回车=默认 {run_log_root()}）：").strip()

    cmd = [_py(), str(p)]
    if file_path:
        cmd += ["-f", file_path]
    elif log_dir:
        cmd += ["-l", log_dir]

    run_cmd(cmd)


def action_device_test_pyrealsense(base: Path) -> None:
    """运行 camera_test.py（pyrealsense2 直连测试，适合在 ROS2 启动前做硬件检查）。"""
    p = base / "camera_test.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    run_cmd([_py(), str(p)])


def action_quick_check(base: Path) -> None:
    """
    快速检查：
    - ros2 是否可用
    - 是否已启动相机节点（/camera/camera）
    - 列出是否存在最新 realsense_start_*.log
    """
    print("\n🔎 Quick Check")

    rc = run_cmd(["bash", "-lc", "command -v ros2 >/dev/null 2>&1"])
    if rc == 0:
        print("✅ ros2 命令可用（请确保已 source ROS2 环境）。")
    else:
        print("⚠️ ros2 命令不可用：如果你要用 ROS2 功能，请先 source /opt/ros/<distro>/setup.bash")

    try:
        out = subprocess.check_output(["bash", "-lc", "ros2 node list 2>/dev/null"], text=True)
        nodes = [x.strip() for x in out.splitlines() if x.strip()]
        if "/camera/camera" in nodes:
            print("✅ 检测到相机节点：/camera/camera")
        else:
            print("⚠️ 未检测到 /camera/camera（若相机未启动，这是正常的）。")
    except Exception:
        print("⚠️ 无法执行 'ros2 node list'（可能未 source ROS2 环境）。")

    log_root = run_log_root()
    if log_root.exists():
        logs = sorted(log_root.rglob("realsense_start_*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
        if logs:
            print(f"✅ 找到最新启动日志：{logs[0]}")
        else:
            print(f"⚠️ 在 {log_root} 下未找到 realsense_start_*.log")
    else:
        print(f"⚠️ ROS 日志目录不存在：{log_root}")


# -------------------------- 菜单 --------------------------

def print_menu() -> None:
    print("\n" + "=" * 78)
    print("RealSense 工具箱 - 统一入口菜单")
    print("=" * 78)
    print(" 1) 启动相机（start.sh：选择分辨率/FPS/点云，写启动日志）")
    print(" 2) 运行时调参 + Topic 选择/echo（camera_menu.py）")
    print(" 3) 拍照/录像（record.py）")
    print(" 4) 查看最新/指定启动日志（camera_log_read.py）")
    print(" 5) 直连硬件测试（camera_test.py，不依赖 ROS2）")
    print(" 6) Quick Check（检查 ros2/相机节点/最新日志）")
    print(" q) 退出")
    print("=" * 78)


def main() -> None:
    warn_if_root()
    base = _here()

    actions = {
        "1": lambda: action_start_camera(base),
        "2": lambda: action_runtime_param_and_topic(base),
        "3": lambda: action_record_photo_video(base),
        "4": lambda: action_read_startup_log(base),
        "5": lambda: action_device_test_pyrealsense(base),
        "6": lambda: action_quick_check(base),
    }

    while True:
        print_menu()
        choice = input("请选择功能 (1-6/q)：").strip().lower()

        if choice in ("q", "quit", "exit"):
            print("👋 已退出")
            return

        fn = actions.get(choice)
        if not fn:
            print("❌ 无效选项，请重新输入。")
            continue

        fn()
        input("\n按回车返回菜单...")


if __name__ == "__main__":
    main()
