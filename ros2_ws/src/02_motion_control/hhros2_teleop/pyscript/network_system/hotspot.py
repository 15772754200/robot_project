#!/usr/bin/env python3
import subprocess
import time
import os
from datetime import datetime

# 配置参数（根据实际情况修改）
HOTSPOT_IFACE = "wlan0"       # 热点专用网卡
HOTSPOT_NAME = "MyHotspot"    # 热点名称
HOTSPOT_PASS = "12345678"     # 热点密码
LOG_FILE = "run_logs/hotspot/hotspot_log.txt"
CHECK_INTERVAL = 2  # 设备检测间隔（秒）

def run_cmd(cmd, check=False):
    """执行系统命令并返回结果"""
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            check=check
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"❌ 命令执行失败: {e.stderr.strip()}")
        return None

def get_hotspot_ip_range():
    """获取热点的IP网段（用于过滤非热点设备）"""
    # 从热点网卡获取IP（通常是192.168.x.1）
    ip_info = run_cmd(f"ip addr show {HOTSPOT_IFACE} | grep 'inet '")
    if ip_info:
        # 提取网段（如192.168.3.0/24）
        ip_prefix = ip_info.split()[1].split('/')[0].rsplit('.', 1)[0]
        return f"{ip_prefix}."
    return "192.168."  # 默认网段

def get_connected_devices():
    """获取当前连接到热点的设备（MAC+IP）"""
    ip_prefix = get_hotspot_ip_range()
    arp_table = run_cmd("arp -n")
    devices = {}
    for line in arp_table.splitlines():
        if ip_prefix in line and "(incomplete)" not in line:
            parts = line.split()
            if len(parts) >= 3:
                ip = parts[0]
                mac = parts[2].upper()  # 统一MAC格式为大写
                devices[mac] = ip
    return devices

def monitor_connections():
    """实时监控设备连接状态（连接/断开）"""
    print(f"📡 开始监控热点设备（间隔{CHECK_INTERVAL}秒，Ctrl+C退出）...")
    previous_devices = {}  # 上一次检测到的设备 {MAC: IP}
    try:
        while True:
            current_devices = get_connected_devices()
            # 检测新连接的设备
            for mac, ip in current_devices.items():
                if mac not in previous_devices:
                    now = datetime.now()
                    print(f"🔗 [新连接] {mac} （IP: {ip}） @ {now}")
                    with open(LOG_FILE, "a") as f:
                        f.write(f"[连接] {now} | MAC: {mac} | IP: {ip}\n")
            # 检测断开的设备
            for mac, ip in previous_devices.items():
                if mac not in current_devices:
                    now = datetime.now()
                    print(f"🔌 [已断开] {mac} （IP: {ip}） @ {now}")
                    with open(LOG_FILE, "a") as f:
                        f.write(f"[断开] {now} | MAC: {mac} | IP: {ip}\n")
            # 更新设备列表
            previous_devices = current_devices.copy()
            time.sleep(CHECK_INTERVAL)
    except KeyboardInterrupt:
        print("\n🟡 已停止设备监控")

def is_hotspot_running():
    """检查热点是否正在运行"""
    output = run_cmd(f"iw dev {HOTSPOT_IFACE} info")
    return "type AP" in output

def start_hotspot():
    """启动热点并开始监控设备"""
    print(f"🔹 正在启动热点（{HOTSPOT_IFACE}）...")
    # 断开热点网卡的现有连接
    run_cmd(f"nmcli device disconnect {HOTSPOT_IFACE}")
    
    # 检查并创建热点配置
    existing = run_cmd(f"nmcli connection show {HOTSPOT_NAME}")
    if not existing:
        run_cmd(
            f"nmcli connection add type wifi ifname {HOTSPOT_IFACE} "
            f"con-name {HOTSPOT_NAME} autoconnect no ssid {HOTSPOT_NAME}",
            check=True
        )
        run_cmd(
            f"nmcli connection modify {HOTSPOT_NAME} "
            f"802-11-wireless.mode ap 802-11-wireless.band bg "
            f"ipv4.method shared wifi-sec.key-mgmt wpa-psk wifi-sec.psk {HOTSPOT_PASS}",
            check=True
        )
    else:
        run_cmd(f"nmcli connection modify {HOTSPOT_NAME} ifname {HOTSPOT_IFACE}")
    
    # 启动热点
    run_cmd(f"nmcli connection up {HOTSPOT_NAME}", check=True)
    start_time = datetime.now()
    print(f"✅ 热点已开启: {HOTSPOT_NAME}")
    print(f"   密码: {HOTSPOT_PASS}")
    print(f"   网卡: {HOTSPOT_IFACE}")
    print(f"   时间: {start_time}")
    
    # 写入日志
    with open(LOG_FILE, "a") as f:
        f.write(f"\n===== 热点启动 =====\n时间: {start_time}\n名称: {HOTSPOT_NAME}\n")
        f.write(f"密码: {HOTSPOT_PASS}\n网卡: {HOTSPOT_IFACE}\n")
    
    # 开始监控设备连接
    monitor_connections()

def stop_hotspot():
    """关闭热点"""
    print(f"🛑 正在关闭热点（{HOTSPOT_IFACE}）...")
    stop_time = datetime.now()
    run_cmd(f"nmcli connection down {HOTSPOT_NAME}")
    run_cmd(f"nmcli device disconnect {HOTSPOT_IFACE}")
    print(f"✅ 热点已关闭 @ {stop_time}")
    with open(LOG_FILE, "a") as f:
        f.write(f"===== 热点关闭 =====\n时间: {stop_time}\n\n")

def main():
    if os.geteuid() != 0:
        print("⚠️ 请使用sudo运行：sudo python3 redian.py")
        return
    # 检查热点网卡是否存在
    if HOTSPOT_IFACE not in run_cmd("iw dev | grep Interface | awk '{print $2}'"):
        print(f"❌ 未找到网卡 {HOTSPOT_IFACE}，请检查配置")
        return
    # 启动或关闭热点
    if is_hotspot_running():
        stop_hotspot()
    else:
        start_hotspot()

if __name__ == "__main__":
    main()

