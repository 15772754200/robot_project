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
import os

OUTFILE = "run_logs/network/network_info.log"

def run(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.DEVNULL, shell=True, text=True)
    except subprocess.CalledProcessError:
        return ""

def list_interfaces():
    # 使用 `ip -o link` 列出接口及状态
    out = run("ip -o link")
    if not out:
        return []
    ifs = []
    for line in out.splitlines():
        # 格式: "1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 ..."
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
    # 使用 ip addr
    out = run(f"ip -o addr show dev {ifname}")
    ipv4 = []
    ipv6 = []
    for line in out.splitlines():
        # sample: "2: eth0    inet 192.168.1.10/24 brd ... "
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
    """
    更可靠地检测无线接口
    """
    # 方法1: 检查 /sys/class/net/{ifname}/wireless 目录
    wireless_path = f"/sys/class/net/{ifname}/wireless"
    if os.path.exists(wireless_path):
        return True
    
    # 方法2: 检查 /proc/net/wireless 文件
    try:
        with open("/proc/net/wireless", "r") as f:
            content = f.read()
            # 更精确的匹配，避免部分匹配
            for line in content.splitlines():
                if line.startswith(ifname + ":"):
                    return True
    except Exception:
        pass
    
    # 方法3: 使用 iwconfig 检测
    if which("iwconfig"):
        try:
            result = run(f"iwconfig {ifname} 2>/dev/null")
            if "no wireless extensions" not in result.lower() and "essid" in result.lower():
                return True
        except Exception:
            pass
    
    # 方法4: 检查 /sys/class/net/{ifname}/type
    try:
        with open(f"/sys/class/net/{ifname}/type", "r") as f:
            dev_type = f.read().strip()
            # ARPHRD_IEEE80211 (无线) 通常是 801
            if dev_type == "801":
                return True
    except Exception:
        pass
    
    # 方法5: 使用 ip link show 信息检测
    result = run(f"ip link show {ifname}")
    if "link/ieee802.11" in result:
        return True
    
    return False

def get_interface_type(ifname):
    """获取更详细的
    类型信息"""
    if is_wireless(ifname):
        return "Wireless"
    else:
        return "Wired"

def get_wireless_signal(ifname):
    """更可靠的无线信号检测"""
    # 方法1: 使用 iw 命令（更现代）
    if which("iw"):
        out = run(f"iw dev {ifname} link")
        if "signal:" in out:
            m = re.search(r"signal:\s*([-]?\d+)", out)
            if m:
                return f"{m.group(1)} dBm (from iw)"
    
    # 方法2: 使用 iwconfig（传统方法）
    if which("iwconfig"):
        out = run(f"iwconfig {ifname}")
        m = re.search(r"Signal level[=:]\s*([-]?\d+\s*dBm)", out, re.IGNORECASE)
        if m:
            return m.group(1)
        m = re.search(r"Signal level[=:]\s*([-]?\d+)", out)
        if m:
            return f"{m.group(1)} dBm (from iwconfig)"
    
    # 方法3: 检查 /proc/net/wireless
    try:
        with open("/proc/net/wireless") as f:
            for line in f.readlines()[2:]:  # 跳过前两行标题
                if line.startswith(ifname + ":"):
                    parts = line.split()
                    if len(parts) >= 4:
                        return f"{parts[3]} dBm (from /proc/net/wireless)"
    except Exception:
        pass
    
    return "N/A"

def get_ethtool_info(ifname):
    # 获取 link detected, speed, duplex（如果 ethtool 可用）
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
    # example: default via 192.168.1.1 dev eth0 proto dhcp metric 100 
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
    # Try resolvectl (systemd-resolve) then /etc/resolv.conf
    if which("resolvectl"):
        out = run("resolvectl status")
        servers = re.findall(r"DNS Servers:\s*([0-9. \n:-]+)", out)
        # fallback simpler parse
        m = re.findall(r'([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', out)
        return list(dict.fromkeys(m))
    if which("systemd-resolve"):
        out = run("systemd-resolve --status")
        m = re.findall(r'([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', out)
        return list(dict.fromkeys(m))
    # fallback parse /etc/resolv.conf
    try:
        with open("/etc/resolv.conf") as f:
            body = f.read()
        servers = re.findall(r'nameserver\s+([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', body)
        return servers
    except Exception:
        return []

def format_block(ifname, flags, mac, ipv4, ipv6, if_type, signal, ethtool_info, is_default, gateway, dns_servers):
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = []
    lines.append(f"--- {ts} | Interface: {ifname} ---")
    lines.append(f"Flags: {','.join(flags)}")
    lines.append(f"MAC: {mac}")
    lines.append(f"IPv4: {', '.join(ipv4) if ipv4 else 'N/A'}")
    lines.append(f"IPv6: {', '.join(ipv6) if ipv6 else 'N/A'}")
    lines.append(f"Type: {if_type}")
    
    if if_type == "Wireless":
        lines.append(f"Signal: {signal}")
    else:
        lines.append(f"Link detected: {ethtool_info.get('link','N/A')}")
        lines.append(f"Speed: {ethtool_info.get('speed','N/A')}")
        lines.append(f"Duplex: {ethtool_info.get('duplex','N/A')}")
    
    lines.append(f"Default route via this interface: {'Yes' if is_default else 'No'}")
    if gateway:
        lines.append(f"Gateway: {gateway}")
    lines.append(f"DNS servers: {', '.join(dns_servers) if dns_servers else 'N/A'}")
    lines.append("")  # 空行
    return "\n".join(lines)

def main():
    interfaces = list_interfaces()
    if not interfaces:
        print("无法列出网络接口，确认系统有 `ip` 命令。")
        sys.exit(1)

    default_routes = get_default_route_info()
    dns_servers = get_dns_servers()


    all_blocks = []
    for ifname, flags in interfaces:  
        # 只关注 UP 的接口（可按需改为全部）
        if "UP" not in flags and "LOWER_UP" not in flags:
            continue
        
        mac = get_mac(ifname)
        ipv4, ipv6 = get_ip_addresses(ifname)
        if_type = get_interface_type(ifname)
        signal = get_wireless_signal(ifname) if if_type == "Wireless" else "N/A"
        ethtool_info = get_ethtool_info(ifname) if if_type == "Wired" else {}
        
        # 是否为默认路由接口
        gw = None
        is_default = False
        for r in default_routes:
            if r.get("dev") == ifname:
                is_default = True
                gw = r.get("gateway")
                break
        
        block = format_block(ifname, flags, mac, ipv4, ipv6, if_type, signal, ethtool_info, is_default, gw, dns_servers)
        all_blocks.append(block)
        # 终端打印
        print(block)

    if not all_blocks:
        print("没有找到 UP 状态的接口。")
        return

    # 写入文件（追加）
    try:
        # 获取文件所在目录路径
        dir_path = os.path.dirname(OUTFILE)
        # 如果目录不存在则创建（包括所有父目录）
        if not os.path.exists(dir_path):
            os.makedirs(dir_path, exist_ok=True, mode=0o777)

        with open(OUTFILE, "a", encoding="utf-8") as f:
            f.write("\n".join(all_blocks))
            f.write("\n")
        print(f"信息已追加写入：{OUTFILE}")
    except Exception as e:
        print("写入文件失败：", e)

if __name__ == "__main__":
    main()