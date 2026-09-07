"""
wifi_logger_full.py
----------------------------------
查询 Linux 下所有保存过的 Wi-Fi 网络信息，并记录：
SSID、MAC 地址、连接时间、状态、IP 地址。
仅在Wi-Fi连接发生变化时创建新的日志文件，并区分当前与历史连接。
"""
import os
import subprocess
import time
import datetime
import configparser
from pathlib import Path
import re

# ======== 可自定义参数 ========
NM_PATH = "/etc/NetworkManager/system-connections/"  # NetworkManager 配置路径
LOG_DIR = Path("./run_logs/wifi")  # 日志文件夹
# ==============================


def run_cmd(cmd):
    """运行 shell 命令并返回输出"""
    try:
        out = subprocess.check_output(cmd, shell=True, text=True, stderr=subprocess.DEVNULL)
        return out.strip()
    except subprocess.CalledProcessError:
        return ""


def get_ip_address():
    """获取当前设备 IP 地址"""
    ip = run_cmd("hostname -I | awk '{print $1}'")
    return ip or "N/A"


def get_connection_time(dev):
    """
    根据设备名获取WiFi连接的标准时间格式
    
    参数: 
        dev: WiFi设备名（如wlp3s0）
    
    返回:
        标准时间字符串（YYYY-MM-DD HH:MM:SS）或错误信息
    """
    try:
        # 1. 通过设备获取连接名称（SSID）
        ssid_cmd = f"iw dev {dev} link | grep 'SSID' | awk '{{print $2}}'"
        ssid = run_cmd(ssid_cmd)
        if not ssid:
            return "未获取到WiFi连接名称"
        
        # 2. 获取指定连接的ID和时间戳
        nmcli_cmd = f"nmcli -f connection.id,connection.timestamp connection show {ssid}"
        result = run_cmd(nmcli_cmd)
        
        # 3. 解析输出提取时间戳
        timestamp = None
        for line in result.splitlines():
            if line.strip().startswith("connection.timestamp:"):
                # 提取时间戳数字（处理可能的空格）
                timestamp = line.split(":")[-1].strip()
                break
        
        if not timestamp or not timestamp.isdigit():
            return "未找到有效的时间戳"
        
        # 4. 转换为标准时间格式
        dt = datetime.datetime.fromtimestamp(int(timestamp))
        return dt.strftime("%Y-%m-%d %H:%M:%S")
    
    except Exception as e:
        return f"处理错误: {str(e)}"


def get_bssid(dev):
    """通过 iw 命令获取当前连接的 Wi-Fi BSSID（MAC 地址）"""
    mac_pattern = r'([0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2})'
    out = run_cmd(f"iw dev {dev} link")
    for line in out.splitlines():
        if "Connected to" in line:
            match = re.search(mac_pattern, line)
            if match:
                return match.group(1)  # 返回匹配到的MAC地址
    return "N/A"


def get_active_wifi():
    """获取当前正在连接的 WiFi 名称、BSSID、状态"""
    out = run_cmd("nmcli -t -f ACTIVE,NAME,DEVICE,STATE connection show --active")
    active_wifi = []
    for line in out.splitlines():
        parts = line.split(":")
        if len(parts) >= 4:
            active, ssid, dev, state = parts[:4]
            if active == "yes":
                bssid = get_bssid(dev)
                connection_time = get_connection_time(dev)  # 获取连接时间
                ip = get_ip_address()  # 获取设备 IP 地址
                active_wifi.append({
                    "ssid": ssid, 
                    "bssid": bssid, 
                    "device": dev, 
                    "state": state, 
                    "ip": ip,
                    "connection_time": connection_time
                })
    return active_wifi


def get_saved_wifi():
    """读取 NetworkManager 保存的所有 Wi-Fi"""
    wifi_list = []
    if not os.path.exists(NM_PATH):
        return wifi_list

    for fname in os.listdir(NM_PATH):
        path = os.path.join(NM_PATH, fname)
        if not os.path.isfile(path) or not fname.endswith(".nmconnection"):
            continue

        try:
            # 使用sudo读取配置文件以解决权限问题
            config_content = run_cmd(f"sudo cat {path}")
            if not config_content:
                continue
                
            config = configparser.ConfigParser()
            config.read_string(config_content)
            
            ssid = config.get("wifi", "ssid", fallback=None)
            mac = config.get("wifi", "mac-address", fallback="N/A")
            if ssid:
                mtime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(os.path.getmtime(path)))
                wifi_list.append({"ssid": ssid, "config": fname, "mac": mac, "mtime": mtime})
        except Exception:
            continue

    return wifi_list


def get_latest_log():
    """获取最新的日志文件"""
    if not LOG_DIR.exists():
        return None
        
    log_files = list(LOG_DIR.glob("wifi_log_*.log"))
    if not log_files:
        return None
        
    # 按修改时间排序，返回最新的日志
    log_files.sort(key=os.path.getmtime, reverse=True)
    return log_files[0]


def extract_previous_ssid(latest_log):
    """从最新日志中提取上一次连接的SSID"""
    try:
        with open(latest_log, "r") as f:
            content = f.read()
            
        # 查找当前连接部分的SSID
        match = re.search(r"---- 当前连接 Wi-Fi ----\s*\nSSID: ([^\|]+)", content, re.DOTALL)
        if match:
            return match.group(1).strip()
        return None
    except Exception:
        return None


def extract_previous_connection(latest_log):
    """从最新日志中提取上一次连接的详细信息"""
    prev_conn = {
        "ssid": "无历史记录",
        "bssid": "N/A",
        "device": "N/A",
        "state": "已断开",
        "ip": "N/A",
        "connection_time": "N/A"
    }
    
    if not latest_log:
        return prev_conn
        
    try:
        with open(latest_log, "r") as f:
            lines = f.readlines()
            
        # 找到当前连接部分
        in_active_section = False
        for line in lines:
            if "---- 当前连接 Wi-Fi ----" in line:
                in_active_section = True
                continue
                
            if in_active_section and line.startswith("SSID:"):
                # 解析连接信息行
                parts = [p.strip() for p in line.split("|")]
                for part in parts:
                    if part.startswith("SSID:"):
                        prev_conn["ssid"] = part.split(":", 1)[1].strip()
                    elif part.startswith("MAC:"):
                        prev_conn["bssid"] = part.split(":", 1)[1].strip()
                    elif part.startswith("状态:"):
                        prev_conn["state"] = part.split(":", 1)[1].strip()
                    elif part.startswith("网卡:"):
                        prev_conn["device"] = part.split(":", 1)[1].strip()
                    elif part.startswith("IP:"):
                        prev_conn["ip"] = part.split(":", 1)[1].strip()
                    elif part.startswith("连接时间:"):
                        prev_conn["connection_time"] = part.split(":", 1)[1].strip()
                break
    except Exception:
        pass
        
    return prev_conn


def write_log(wifi_list, active_list, previous_conn=None):
    """写入日志文件，包含当前连接和历史连接信息"""
    os.makedirs(LOG_DIR, exist_ok=True)
    timestamp = time.strftime("%Y-%m-%d_%H-%M-%S", time.localtime())
    log_path = LOG_DIR / f"wifi_log_{timestamp}.log"

    with open(log_path, "w") as f:
        f.write(f"===== Wi-Fi 日志记录时间: {timestamp} =====\n")
        f.write("---- 当前连接 Wi-Fi ----\n")
        if active_list:
            for a in active_list:
                f.write(
                    f"SSID: {a['ssid']:<25} | MAC: {a['bssid']:<20} | 状态: {a['state']} | 网卡: {a['device']} | IP: {a['ip']} | 连接时间: {a['connection_time']}\n"
                )
        else:
            f.write("无活动连接。\n")

        # 如果有历史连接，单独记录上一次连接信息
        if previous_conn and previous_conn["ssid"] != "无历史记录" and (not active_list or previous_conn["ssid"] != active_list[0]["ssid"]):
            f.write("\n---- 上一次连接 Wi-Fi ----\n")
            f.write(
                f"SSID: {previous_conn['ssid']:<25} | MAC: {previous_conn['bssid']:<20} | 状态: {previous_conn['state']} | 网卡: {previous_conn['device']} | IP: {previous_conn['ip']} | 连接时间: {previous_conn['connection_time']}\n"
            )

        f.write("\n---- 历史保存 Wi-Fi ----\n")
        # 过滤掉当前连接和上一次连接的Wi-Fi，避免重复
        filtered_wifi = []
        current_ssids = set()
        if active_list:
            current_ssids = {a["ssid"] for a in active_list}
        if previous_conn and previous_conn["ssid"] != "无历史记录":
            current_ssids.add(previous_conn["ssid"])
            
        for w in wifi_list:
            if w["ssid"] not in current_ssids:
                filtered_wifi.append(w)
                
        if filtered_wifi:
            for w in filtered_wifi:
                f.write(
                    f"SSID: {w['ssid']:<25} | MAC: {w['mac']:<20} | 配置文件: {w['config']:<35} | 修改时间: {w['mtime']}\n"
                )
        else:
            f.write("无其他历史保存的Wi-Fi。\n")

    return log_path


def main():
    print("📡 正在收集 Wi-Fi 信息...\n")

    active_list = get_active_wifi()
    wifi_list = get_saved_wifi()
    latest_log = get_latest_log()
    
    # 获取当前连接的SSID
    current_ssid = active_list[0]["ssid"] if active_list else None
    # 获取上一次连接的SSID和详细信息
    previous_ssid = extract_previous_ssid(latest_log) if latest_log else None
    previous_conn = extract_previous_connection(latest_log) if latest_log else None

    # 检查是否需要创建新日志：首次运行、无当前连接或连接已变化
    if not latest_log or not current_ssid or current_ssid != previous_ssid:
        log_path = write_log(wifi_list, active_list, previous_conn)
        print(f"✅ 已生成新日志文件：{log_path}")
        print("完整日志路径：", log_path.resolve())
    else:
        print(f"ℹ️ Wi-Fi连接未变化 (当前: {current_ssid})，无需生成新日志")
        print(f"最新日志路径：{latest_log.resolve()}")


if __name__ == "__main__":
    main()
