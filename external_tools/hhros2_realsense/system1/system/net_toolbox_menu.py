#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
net_toolbox_menu.py
===================

一个“菜单启动器”，把你现有的脚本统一到一个入口里：启动后选功能 -> 自动调用对应脚本。

已集成（同目录下）：
- hotspot.py                  启动/关闭热点并监控设备（需要 sudo，会写 /var/log/hotspot_log_*.txt）
- hotspot_log_viewer.py       查看热点日志（默认读最新）
- wifi_log.py                 生成 Wi-Fi 日志（写到 ~/wifi_logs/）
- wifi_read.py                读取 Wi-Fi 最新日志（从 ~/wifi_logs/ 读）
- wifi_connect.sh             Wi-Fi 开关 + 自动连接（bash 脚本）
- tongxin_test.py             网络链路自检（ping/dns/简易测速 + json 日志）

建议：用普通用户运行本脚本；涉及“热点”操作时才会弹出 sudo。
（如果你用 sudo 运行本脚本，wifi_log.py / wifi_read.py 的 Path.home() 可能变成 /root，日志会写到 /root/wifi_logs）
"""

from __future__ import annotations

import os
import sys
import subprocess
from pathlib import Path
from typing import List, Optional


def _here() -> Path:
    return Path(__file__).resolve().parent


def _py() -> str:
    return sys.executable or "python3"


def _exists(p: Path) -> bool:
    return p.exists() and p.is_file()


def run_cmd(cmd: List[str], *, check: bool = False) -> int:
    """运行子进程命令，返回退出码。"""
    print("\n" + "=" * 72)
    print("→ 执行命令：", " ".join(cmd))
    print("=" * 72)
    try:
        r = subprocess.run(cmd, check=check)
        return int(r.returncode)
    except subprocess.CalledProcessError as e:
        print(f"❌ 命令失败，返回码={e.returncode}")
        return int(e.returncode)
    except FileNotFoundError:
        print("❌ 找不到可执行文件/脚本：", cmd[0])
        return 127


def _sudo_prefix() -> List[str]:
    return ["sudo"] if os.geteuid() != 0 else []


def warn_if_root():
    if os.geteuid() == 0:
        sudo_user = os.environ.get("SUDO_USER")
        if sudo_user:
            print("⚠️ 你正在以 root 身份运行（可能是 sudo 运行的）。")
            print("   注意：wifi_log.py / wifi_read.py 使用 Path.home()，可能会把日志写到 /root/wifi_logs。")
            print("   建议：用普通用户运行本菜单脚本，仅在“热点”选项时再 sudo。\n")


# ------------------------- Wi-Fi 日志快捷查看（可选增强） -------------------------

def wifi_show_latest(max_entries: int = 10) -> None:
    """
    直接在本脚本里读取 ~/wifi_logs 下最新的 wifi_log*.log，并按条目打印前 max_entries 条。
    这相当于 wifi_read.py 的“更可控”版本（它当前不会真正限制 max_entries）。
    """
    import re

    log_dir = Path.home() / "wifi_logs"
    if not log_dir.exists():
        print(f"❌ 日志目录不存在：{log_dir}")
        return

    # 只匹配 wifi_log*.log
    cand = [p for p in log_dir.glob("wifi_log*.log") if p.is_file()]
    if not cand:
        print(f"❌ 在 {log_dir} 下未找到任何 wifi_log*.log")
        return

    latest = max(cand, key=lambda p: p.stat().st_mtime)
    print(f"📘 最新 Wi-Fi 日志：{latest}")

    content = latest.read_text(encoding="utf-8", errors="ignore").strip()

    modules = []

    # 当前连接
    m = re.search(r"---- 当前连接 Wi-Fi ----(.*?)(?=---- 上一次连接|---- 历史保存|$)", content, re.DOTALL)
    if m:
        cur = m.group(1).strip()
        if cur and cur != "无活动连接。":
            modules.append(("当前连接", cur))

    # 上一次连接
    m = re.search(r"---- 上一次连接 Wi-Fi ----(.*?)(?=---- 历史保存|$)", content, re.DOTALL)
    if m:
        prev = m.group(1).strip()
        if prev:
            modules.append(("上一次连接（历史）", prev))

    # 历史保存
    m = re.search(r"---- 历史保存 Wi-Fi ----(.*)", content, re.DOTALL)
    if m:
        hist = m.group(1).strip()
        if hist and hist != "无其他历史保存的Wi-Fi。":
            for line in [x.strip() for x in hist.splitlines() if x.strip()]:
                modules.append(("历史保存", line))

    if not modules:
        print("⚠️ 日志文件中没有可展示的内容。")
        return

    if max_entries <= 0:
        max_entries = len(modules)

    print(f"\n📋 Wi-Fi 连接记录（展示 {min(max_entries, len(modules))}/{len(modules)} 条）\n")
    for i, (typ, body) in enumerate(modules[:max_entries], 1):
        print(f"{i:2d}. {typ}")
        print("   " + "-" * 60)
        if typ in ("当前连接", "上一次连接（历史）"):
            fields = [f.strip() for f in body.split("|") if f.strip()]
            for f in fields:
                print("   " + f)
        else:
            print("   " + body)
        print()


# ------------------------- 菜单动作 -------------------------

def action_hotspot_toggle(base: Path) -> None:
    p = base / "hotspot.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    # hotspot.py 内部会检查 root；这里直接 sudo 调它
    cmd = _sudo_prefix() + [_py(), str(p)]
    run_cmd(cmd)


def action_hotspot_view_logs(base: Path) -> None:
    p = base / "hotspot_log_viewer.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return

    file_path = input("输入日志文件路径（回车=自动读取最新）：").strip()
    keyword = input("输入关键字过滤（回车=不过滤）：").strip()

    cmd = [_py(), str(p)]
    if file_path:
        cmd += ["-f", file_path]
    if keyword:
        cmd += ["-k", keyword]

    # 不强制 sudo：没权限时脚本会提示你用 sudo
    run_cmd(cmd)


def action_wifi_generate_log(base: Path) -> None:
    p = base / "wifi_log.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    # 重要：不要 sudo 跑，否则日志写到 /root/wifi_logs；脚本内部已用 sudo cat 读取系统配置
    cmd = [_py(), str(p)]
    run_cmd(cmd)


def action_wifi_read_latest(base: Path) -> None:
    # 优先用本脚本内置的“可限制条数”的查看器；也可以改为直接跑 wifi_read.py
    s = input("想显示最近几条记录？(默认 10，0=全部)：").strip()
    try:
        n = int(s) if s else 10
    except ValueError:
        n = 10
    wifi_show_latest(max_entries=n)

    # 如果你更想跑原脚本（无条数限制），取消注释：
    # p = base / "wifi_read.py"
    # if _exists(p):
    #     run_cmd([_py(), str(p)])


def action_wifi_connect_toggle(base: Path) -> None:
    p = base / "wifi_connect.sh"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    # 确保可执行
    try:
        p.chmod(p.stat().st_mode | 0o111)
    except PermissionError:
        pass

    # bash 脚本通常无需 sudo，但如果你的系统有 polkit 限制，失败再自己 sudo 执行即可
    cmd = ["bash", str(p)]
    run_cmd(cmd)


def action_network_link_check(base: Path) -> None:
    p = base / "tongxin_test.py"
    if not _exists(p):
        print(f"❌ 未找到 {p}")
        return
    cmd = [_py(), str(p)]
    run_cmd(cmd)


def print_menu() -> None:
    print("\n" + "=" * 72)
    print("网络工具箱 - 统一入口菜单")
    print("=" * 72)
    print(" 1) 热点：启动/关闭（hotspot.py，自动 sudo）")
    print(" 2) 热点：查看日志（hotspot_log_viewer.py）")
    print(" 3) Wi-Fi：生成日志（wifi_log.py，写到 ~/wifi_logs）")
    print(" 4) Wi-Fi：读取最新日志（内置查看器，可限制条数）")
    print(" 5) Wi-Fi：开关/自动连接（wifi_connect.sh）")
    print(" 6) 网络链路自检（tongxin_test.py）")
    print(" q) 退出")
    print("=" * 72)


def main() -> None:
    warn_if_root()
    base = _here()

    actions = {
        "1": lambda: action_hotspot_toggle(base),
        "2": lambda: action_hotspot_view_logs(base),
        "3": lambda: action_wifi_generate_log(base),
        "4": lambda: action_wifi_read_latest(base),
        "5": lambda: action_wifi_connect_toggle(base),
        "6": lambda: action_network_link_check(base),
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
