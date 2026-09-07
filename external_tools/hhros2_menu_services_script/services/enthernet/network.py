#!/usr/bin/env python3
"""
network_info.py
在终端显示并把网络接口信息写入 txt 文件（追加）
支持有线和无线（无线获取信号强度），并尝试获取网关和 DNS。
"""

import subprocess
import re
import datetime
import socket
import sys
from shutil import which
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
    LOG_DIR = PROJECT_ROOT / "ros2_ws" / "run_logs" / "ethernet"

# 创建日志目录
LOG_DIR.mkdir(parents=True, exist_ok=True)

# 日志文件路径（与 menu.py 中的 ETH_LOG 一致）
OUTFILE = str(LOG_DIR / "network_info.log")


def run(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.DEVNULL, shell=True, text=True)
    except subprocess.CalledProcessError:
        return ""


def list_interfaces():
    out = run("ip -o link")
    if not out:
        return []
    ifs = []
    for line in out.splitlines():
        m = re.match(r'^\d+:\s+([^:]+):\s+<([^>]+)>', line)
        if m:
            name = m.group(1)
            flags = m.group(2).split(",")
            ifs.append((name, flags))
    return ifs


def get_mac(ifname):
    out = run(f"cat /sys/class/net/{ifname}/address")
    return out.strip() or "N/A"


def get_ip_addresses(ifname):
    out = run(f"ip -o addr show dev {ifname}")
    ipv4 = []
    ipv6 = []
    for line in out.splitlines():
        if " inet " in line:
            m = re.search(r'inet (\d+\.\d+\.\d+\.\d+)/(\d+)', line)
            if m:
                ipv4.append(m.group(1))
        if " inet6 " in line:
            m = re.search(r'inet6 ([0-9a-fA-F:]+)/', line)
            if m:
                ipv6.append(m.group(1))
    return ipv4, ipv6


def is_wireless(ifname):
    try:
        with open("/proc/net/wireless") as f:
            body = f.read()
        return ifname in body
    except Exception:
        import os
        return os.path.isdir(f"/sys/class/net/{ifname}/wireless")


def get_wireless_signal(ifname):
    try:
        with open("/proc/net/wireless") as f:
            for line in f:
                if line.strip().startswith(ifname + ":"):
                    parts = line.split()
                    nums = re.findall(r"[-]?\d+\.\d+|[-]?\d+", line)
                    if len(nums) >= 3:
                        sig = nums[-2]
                        return f"{sig} dBm (from /proc/net/wireless)"
    except Exception:
        pass
    if which("iwconfig"):
        out = run(f"iwconfig {ifname}")
        m = re.search(r"Signal level[=:]\s*([-]?\d+)", out)
        if m:
            return f"{m.group(1)} dBm (from iwconfig)"
        m2 = re.search(r"Signal level=(-?\d+/\d+)", out)
        if m2:
            return m2.group(1)
    return "N/A"


def get_ethtool_info(ifname):
    if not which("ethtool"):
        return {"link": "N/A", "speed": "N/A", "duplex": "N/A"}
    out = run(f"ethtool {ifname}")
    info = {"link": "N/A", "speed": "N/A", "duplex": "N/A"}
    m = re.search(r"Link detected:\s*(yes|no)", out, re.I)
    if m:
        info["link"] = m.group(1).lower()
    m2 = re.search(r"Speed:\s*([0-9A-Za-z/]+)", out)
    if m2:
        info["speed"] = m2.group(1)
    m3 = re.search(r"Duplex:\s*(Full|Half)", out)
    if m3:
        info["duplex"] = m3.group(1)
    return info


def get_default_route_info():
    out = run("ip route show default")
    if not out:
        return {}
    lines = out.splitlines()
    routes = []
    for line in lines:
        m = re.search(r'default via ([0-9.]+) dev (\S+)', line)
        if m:
            routes.append({"gateway": m.group(1), "dev": m.group(2), "raw": line})
    return routes


def get_dns_servers():
    if which("resolvectl"):
        out = run("resolvectl status")
        m = re.findall(r'([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', out)
        return list(dict.fromkeys(m))
    if which("systemd-resolve"):
        out = run("systemd-resolve --status")
        m = re.findall(r'([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', out)
        return list(dict.fromkeys(m))
    try:
        with open("/etc/resolv.conf") as f:
            body = f.read()
        servers = re.findall(r'nameserver\s+([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', body)
        return servers
    except Exception:
        return []


def format_block(ifname, flags, mac, ipv4, ipv6, is_wl, signal, ethtool_info, is_default, gateway, dns_servers):
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = []
    lines.append(f"--- {ts} | Interface: {ifname} ---")
    lines.append(f"Flags: {','.join(flags)}")
    lines.append(f"MAC: {mac}")
    lines.append(f"IPv4: {', '.join(ipv4) if ipv4 else 'N/A'}")
    lines.append(f"IPv6: {', '.join(ipv6) if ipv6 else 'N/A'}")
    if is_wl:
        lines.append(f"Type: Wireless")
        lines.append(f"Signal: {signal}")
    else:
        lines.append(f"Type: Wired")
        lines.append(f"Link detected: {ethtool_info.get('link','N/A')}")
        lines.append(f"Speed: {ethtool_info.get('speed','N/A')}")
        lines.append(f"Duplex: {ethtool_info.get('duplex','N/A')}")
    lines.append(f"Default route via this interface: {'Yes' if is_default else 'No'}")
    if gateway:
        lines.append(f"Gateway: {gateway}")
    lines.append(f"DNS servers: {', '.join(dns_servers) if dns_servers else 'N/A'}")
    lines.append("")
    return "\n".join(lines)


def main():
    print(f"📁 日志文件: {OUTFILE}")
    print()
    
    interfaces = list_interfaces()
    if not interfaces:
        print("无法列出网络接口，确认系统有 `ip` 命令。")
        sys.exit(1)

    default_routes = get_default_route_info()
    dns_servers = get_dns_servers()

    all_blocks = []
    for ifname, flags in interfaces:
        if "UP" not in flags and "LOWER_UP" not in flags:
            continue
        mac = get_mac(ifname)
        ipv4, ipv6 = get_ip_addresses(ifname)
        wl = is_wireless(ifname)
        signal = get_wireless_signal(ifname) if wl else "N/A"
        ethtool_info = get_ethtool_info(ifname) if not wl else {}
        gw = None
        is_default = False
        for r in default_routes:
            if r.get("dev") == ifname:
                is_default = True
                gw = r.get("gateway")
                break
        block = format_block(ifname, flags, mac, ipv4, ipv6, wl, signal, ethtool_info, is_default, gw, dns_servers)
        all_blocks.append(block)
        print(block)

    if not all_blocks:
        print("没有找到 UP 状态的接口。")
        return

    try:
        with open(OUTFILE, "a", encoding="utf-8") as f:
            f.write("\n".join(all_blocks))
            f.write("\n")
        print(f"✅ 信息已追加写入：{OUTFILE}")
    except Exception as e:
        print("❌ 写入文件失败：", e)


if __name__ == "__main__":
    main()