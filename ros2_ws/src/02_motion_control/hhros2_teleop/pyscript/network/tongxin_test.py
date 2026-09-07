import subprocess
import platform
import re
import time
import json
import os
from dataclasses import dataclass
from typing import Optional, Dict, List
from datetime import datetime
from pathlib import Path

@dataclass
class NetworkInfo:
    """网络连接信息"""
    interface_name: str  # 网卡名称
    connection_type: str  # 连接类型：WiFi/Ethernet
    ssid: str  # WiFi名称（仅WiFi）
    signal_strength: int  # 信号强度
    ip_address: str
    mac_address: str
    gateway: str
    dns_servers: List[str]
    link_speed: str
    status: str

class NetworkLinkChecker:
    """网络链路检测系统（支持WiFi和有线网络）"""
    
    def __init__(self):
        self.system = platform.system()
        self.network_info: Optional[NetworkInfo] = None
        self.check_results = []
        
        # ---------- 自动定位日志目录 ----------
        self.log_dir = self._get_log_dir()
        self.log_dir.mkdir(parents=True, exist_ok=True)
        print(f"📁 日志目录: {self.log_dir}")
    
    def _get_log_dir(self) -> Path:
        """获取日志目录"""
        # 从当前文件位置开始查找项目根目录
        current = Path(__file__).resolve().parent
        
        for parent in [current] + list(current.parents):
            if (parent / "external_tools").exists() and (parent / "ros2_ws").exists():
                return parent / "ros2_ws" / "run_logs" / "network"
        
        # 如果找不到，使用当前目录下的 logs
        return current / "logs" / "network"
    
    def _get_log_file_path(self) -> Path:
        """生成日志文件路径"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return self.log_dir / f"network_check_{timestamp}.json"
    
    def get_active_network_linux(self) -> Optional[NetworkInfo]:
        """获取Linux系统的活动网络连接（WiFi或有线）"""
        print("正在检测网络连接...")
        
        # 方法1: 使用ip命令检测所有网络接口
        try:
            # 获取所有网络接口
            result = subprocess.run(
                ['ip', 'addr', 'show'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            if result.returncode != 0:
                print("获取网络接口失败")
                return None
            
            output = result.stdout
            
            # 查找活动的网络接口（有IP地址的）
            interfaces = re.findall(r'\d+:\s+(\w+):', output)
            
            for interface in interfaces:
                # 跳过回环接口
                if interface == 'lo':
                    continue
                
                # 获取该接口的详细信息
                result2 = subprocess.run(
                    ['ip', 'addr', 'show', interface],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                
                interface_info = result2.stdout
                
                # 检查是否有IP地址（说明接口是活动的）
                ip_match = re.search(r'inet\s+([\d.]+)/\d+', interface_info)
                mac_match = re.search(r'link/ether\s+([0-9a-f:]+)', interface_info)
                
                if ip_match:
                    ip_address = ip_match.group(1)
                    mac_address = mac_match.group(1) if mac_match else "未知"
                    
                    # 判断是WiFi还是以太网
                    is_wifi = interface.startswith('wl') or interface.startswith('wlan')
                    connection_type = "WiFi" if is_wifi else "以太网"
                    
                    # 获取网关
                    gateway = self._get_gateway_linux()
                    
                    # 获取DNS
                    dns_servers = self._get_dns_linux()
                    
                    # 获取连接速度（仅以太网）
                    link_speed = self._get_link_speed_linux(interface)
                    
                    # 如果是WiFi，尝试获取SSID和信号强度
                    ssid = "N/A"
                    signal_strength = 100  # 有线网络默认100%
                    
                    if is_wifi:
                        ssid, signal_strength = self._get_wifi_details_linux(interface)
                    
                    network_info = NetworkInfo(
                        interface_name=interface,
                        connection_type=connection_type,
                        ssid=ssid,
                        signal_strength=signal_strength,
                        ip_address=ip_address,
                        mac_address=mac_address,
                        gateway=gateway,
                        dns_servers=dns_servers,
                        link_speed=link_speed,
                        status="已连接"
                    )
                    
                    print(f"✓ 找到活动网络接口: {interface} ({connection_type})")
                    return network_info
            
            print("未找到活动的网络连接")
            return None
            
        except Exception as e:
            print(f"获取网络信息失败: {e}")
            return None
    
    def _get_gateway_linux(self) -> str:
        """获取默认网关"""
        try:
            result = subprocess.run(
                ['ip', 'route', 'show', 'default'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            match = re.search(r'default via ([\d.]+)', result.stdout)
            if match:
                return match.group(1)
        except:
            pass
        return "未知"
    
    def _get_dns_linux(self) -> List[str]:
        """获取DNS服务器"""
        dns_servers = []
        try:
            with open('/etc/resolv.conf', 'r') as f:
                content = f.read()
                dns_matches = re.findall(r'nameserver\s+([\d.]+)', content)
                dns_servers = dns_matches[:2]  # 只取前两个
        except:
            pass
        return dns_servers if dns_servers else ["未知"]
    
    def _get_link_speed_linux(self, interface: str) -> str:
        """获取网络连接速度"""
        try:
            # 尝试使用ethtool
            result = subprocess.run(
                ['ethtool', interface],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            speed_match = re.search(r'Speed:\s+(\d+[MG]b/s)', result.stdout)
            if speed_match:
                return speed_match.group(1)
        except:
            pass
        
        # 尝试从sys文件系统读取
        try:
            with open(f'/sys/class/net/{interface}/speed', 'r') as f:
                speed = f.read().strip()
                if speed and speed != '-1':
                    return f"{speed}Mbps"
        except:
            pass
        
        return "自动"
    
    def _get_wifi_details_linux(self, interface: str) -> tuple:
        """获取WiFi详细信息（SSID和信号强度）"""
        ssid = "未知"
        signal_strength = 0
        
        try:
            # 尝试使用iw获取WiFi信息
            result = subprocess.run(
                ['iw', interface, 'link'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            ssid_match = re.search(r'SSID:\s*(.+)', result.stdout)
            signal_match = re.search(r'signal:\s*(-?\d+)', result.stdout)
            
            if ssid_match:
                ssid = ssid_match.group(1).strip()
            
            if signal_match:
                signal_dbm = int(signal_match.group(1))
                # 将dBm转换为百分比
                signal_strength = min(100, max(0, (signal_dbm + 100) * 100 // 60))
        except:
            pass
        
        return ssid, signal_strength
    
    def get_active_network_windows(self) -> Optional[NetworkInfo]:
        """获取Windows系统的活动网络连接"""
        try:
            # 先尝试WiFi
            cmd = 'netsh wlan show interfaces'
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, encoding='gbk')
            
            if "未运行无线" not in result.stdout and "There is no wireless" not in result.stdout:
                # 有WiFi连接
                output = result.stdout
                
                ssid_match = re.search(r'SSID\s+:\s+(.+)', output)
                signal_match = re.search(r'信号\s+:\s+(\d+)%', output)
                mac_match = re.search(r'物理地址\s+:\s+(.+)', output)
                
                # 获取IP地址
                ip_result = subprocess.run('ipconfig', shell=True, capture_output=True, text=True, encoding='gbk')
                ip_match = re.search(r'IPv4.*:\s+([\d.]+)', ip_result.stdout)
                gateway_match = re.search(r'默认网关.*:\s+([\d.]+)', ip_result.stdout)
                
                return NetworkInfo(
                    interface_name="WiFi",
                    connection_type="WiFi",
                    ssid=ssid_match.group(1).strip() if ssid_match else "未知",
                    signal_strength=int(signal_match.group(1)) if signal_match else 0,
                    ip_address=ip_match.group(1) if ip_match else "未知",
                    mac_address=mac_match.group(1).strip() if mac_match else "未知",
                    gateway=gateway_match.group(1) if gateway_match else "未知",
                    dns_servers=["未知"],
                    link_speed="自动",
                    status="已连接"
                )
            
            # 没有WiFi，尝试以太网
            ip_result = subprocess.run('ipconfig', shell=True, capture_output=True, text=True, encoding='gbk')
            ip_match = re.search(r'IPv4.*:\s+([\d.]+)', ip_result.stdout)
            gateway_match = re.search(r'默认网关.*:\s+([\d.]+)', ip_result.stdout)
            
            if ip_match:
                return NetworkInfo(
                    interface_name="以太网",
                    connection_type="以太网",
                    ssid="N/A",
                    signal_strength=100,
                    ip_address=ip_match.group(1),
                    mac_address="未知",
                    gateway=gateway_match.group(1) if gateway_match else "未知",
                    dns_servers=["未知"],
                    link_speed="自动",
                    status="已连接"
                )
            
        except Exception as e:
            print(f"获取网络信息失败: {e}")
        
        return None
    
    def get_current_network(self) -> Optional[NetworkInfo]:
        """根据操作系统获取当前网络连接信息"""
        print(f"检测操作系统: {self.system}\n")
        
        if self.system == "Windows":
            return self.get_active_network_windows()
        elif self.system == "Linux":
            return self.get_active_network_linux()
        else:
            print(f"✗ 不支持的操作系统: {self.system}")
            return None
    
    def test_ping(self, host: str = "8.8.8.8", count: int = 4) -> Dict:
        """测试网络连接（Ping测试）"""
        print(f"\n正在测试网络连接 (ping {host})...")
        
        try:
            param = '-n' if self.system == "Windows" else '-c'
            cmd = ['ping', param, str(count), host]
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            output = result.stdout
            
            # 解析丢包率和延迟
            if self.system == "Windows":
                loss_match = re.search(r'丢失 = (\d+)|Lost = (\d+)', output)
                time_match = re.search(r'平均 = (\d+)ms|Average = (\d+)ms', output)
            else:
                loss_match = re.search(r'(\d+)% packet loss', output)
                time_match = re.search(r'avg.*?=\s*([\d.]+)', output)
            
            packet_loss = 0
            if loss_match:
                for group in loss_match.groups():
                    if group is not None:
                        packet_loss = int(float(group))
                        break
            
            avg_time = 0
            if time_match:
                for group in time_match.groups():
                    if group is not None:
                        avg_time = float(group)
                        break
            
            packet_loss_rate = packet_loss / count if count > 0 else 0
            
            print(f"  丢包数: {packet_loss}/{count}")
            print(f"  平均延迟: {avg_time:.1f}ms")
            
            return {
                "success": True,
                "packet_loss_rate": packet_loss_rate,
                "average_time": avg_time,
                "host": host
            }
            
        except Exception as e:
            print(f"✗ Ping测试失败: {e}")
            return {
                "success": False,
                "packet_loss_rate": 1.0,
                "average_time": 0,
                "host": host
            }
    
    def test_dns(self) -> Dict:
        """测试DNS解析"""
        print("\n正在测试DNS解析...")
        
        try:
            import socket
            
            test_domains = ["www.baidu.com", "www.google.com", "www.github.com"]
            success_count = 0
            
            for domain in test_domains:
                try:
                    ip = socket.gethostbyname(domain)
                    print(f"  ✓ {domain} → {ip}")
                    success_count += 1
                except:
                    print(f"  ✗ {domain} 解析失败")
            
            return {
                "success": success_count > 0,
                "success_rate": success_count / len(test_domains)
            }
            
        except Exception as e:
            print(f"✗ DNS测试失败: {e}")
            return {"success": False, "success_rate": 0}
    
    def test_bandwidth(self) -> Dict:
        """测试网络带宽（简单的下载测试）"""
        print("\n正在测试网络速度...")
        
        try:
            import urllib.request
            import time
            
            # 下载一个小文件测试速度
            test_url = "http://www.baidu.com"
            
            start_time = time.time()
            with urllib.request.urlopen(test_url, timeout=5) as response:
                data = response.read()
            end_time = time.time()
            
            size_kb = len(data) / 1024
            duration = end_time - start_time
            speed_kbps = size_kb / duration if duration > 0 else 0
            
            print(f"  下载大小: {size_kb:.2f} KB")
            print(f"  耗时: {duration:.2f} 秒")
            print(f"  速度: {speed_kbps:.2f} KB/s")
            
            return {
                "success": True,
                "speed_kbps": speed_kbps
            }
            
        except Exception as e:
            print(f"  速度测试失败: {e}")
            return {
                "success": False,
                "speed_kbps": 0
            }
    
    def check_link_validity(self) -> bool:
        """检查链路合法性"""
        print("=" * 70)
        print("[步骤1] 检查网络链路状态")
        print("=" * 70)
        
        self.network_info = self.get_current_network()
        
        if not self.network_info:
            print("\n✗ 未检测到网络连接")
            print("\n可能的原因:")
            print("1. 网络未连接")
            print("2. 虚拟机网络适配器未启用")
            print("3. 需要配置虚拟机网络设置")
            return False
        
        print(f"\n✓ 检测到网络连接!")
        print(f"\n当前网络信息:")
        print(f"  连接类型: {self.network_info.connection_type}")
        print(f"  网卡名称: {self.network_info.interface_name}")
        if self.network_info.connection_type == "WiFi":
            print(f"  WiFi名称: {self.network_info.ssid}")
            print(f"  信号强度: {self.network_info.signal_strength}%")
        print(f"  IP地址: {self.network_info.ip_address}")
        print(f"  MAC地址: {self.network_info.mac_address}")
        print(f"  网关: {self.network_info.gateway}")
        print(f"  DNS服务器: {', '.join(self.network_info.dns_servers)}")
        print(f"  连接速度: {self.network_info.link_speed}")
        print(f"  连接状态: {self.network_info.status}")
        
        return True
    
    def send_test_params(self):
        """发送检测参数"""
        print("\n" + "=" * 70)
        print("[步骤2] 配置检测参数")
        print("=" * 70)
        
        params = {
            "device_id": "LOCAL_PC",
            "interface": self.network_info.interface_name,
            "connection_type": self.network_info.connection_type,
            "packet_loss_threshold": 0.10,  # 丢包率阈值10%
            "ping_host": "8.8.8.8",
            "ping_count": 4,
            "min_speed_kbps": 50  # 最低速度50KB/s
        }
        
        print("检测参数:")
        for key, value in params.items():
            print(f"  {key}: {value}")
        
        return params
    
    def execute_link_test(self, params: Dict) -> Dict:
        """执行链路检测"""
        print("\n" + "=" * 70)
        print("[步骤3] 执行网络链路检测")
        print("=" * 70)
        
        # 执行Ping测试
        ping_result = self.test_ping(params["ping_host"], params["ping_count"])
        
        # 执行DNS测试
        dns_result = self.test_dns()
        
        # 执行速度测试
        speed_result = self.test_bandwidth()
        
        return {
            "packet_loss_rate": ping_result["packet_loss_rate"],
            "average_ping": ping_result["average_time"],
            "dns_success": dns_result["success"],
            "dns_success_rate": dns_result["success_rate"],
            "speed_kbps": speed_result["speed_kbps"]
        }
    
    def analyze_results(self, test_results: Dict, params: Dict) -> str:
        """分析检测结果"""
        print("\n" + "=" * 70)
        print("[步骤4] 分析检测结果")
        print("=" * 70)
        
        print(f"\n检测结果汇总:")
        print(f"  丢包率: {test_results['packet_loss_rate']:.1%}")
        print(f"  平均延迟: {test_results['average_ping']:.0f}ms")
        print(f"  DNS解析成功率: {test_results['dns_success_rate']:.1%}")
        print(f"  网络速度: {test_results['speed_kbps']:.2f} KB/s")
        
        # 判断是否合格
        packet_loss_ok = test_results['packet_loss_rate'] <= params['packet_loss_threshold']
        dns_ok = test_results['dns_success']
        speed_ok = test_results['speed_kbps'] >= params['min_speed_kbps']
        
        print(f"\n合格性判断:")
        print(f"  丢包率: {'✓ 合格' if packet_loss_ok else '✗ 不合格'} (阈值: {params['packet_loss_threshold']:.1%})")
        print(f"  DNS解析: {'✓ 正常' if dns_ok else '✗ 异常'}")
        print(f"  网络速度: {'✓ 合格' if speed_ok else '✗ 较慢'} (阈值: {params['min_speed_kbps']} KB/s)")
        
        if packet_loss_ok and dns_ok and speed_ok:
            result = "合格"
            print(f"\n✓ 综合评估: 网络连接良好")
        else:
            result = "不合格"
            print(f"\n✗ 综合评估: 网络连接存在问题")
            
            if not packet_loss_ok:
                print(f"  - 丢包率过高，网络可能不稳定")
            if not dns_ok:
                print(f"  - DNS解析异常，请检查DNS设置")
            if not speed_ok:
                print(f"  - 网络速度较慢")
        
        return result
    
    def save_log(self, test_results: Dict, result: str):
        """保存检测日志到 ros2_ws/run_logs/network/"""
        print("\n" + "=" * 70)
        print("[步骤5] 保存检测日志")
        print("=" * 70)
        
        log_file_path = self._get_log_file_path()
        
        log_entry = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "log_dir": str(self.log_dir),
            "log_file": log_file_path.name,
            "network_info": {
                "interface": self.network_info.interface_name,
                "connection_type": self.network_info.connection_type,
                "ssid": self.network_info.ssid,
                "ip_address": self.network_info.ip_address,
                "mac_address": self.network_info.mac_address,
                "gateway": self.network_info.gateway,
                "dns_servers": self.network_info.dns_servers,
                "link_speed": self.network_info.link_speed
            },
            "test_results": test_results,
            "check_result": result
        }
        
        print(f"日志信息:")
        print(f"  时间戳: {log_entry['timestamp']}")
        print(f"  连接类型: {self.network_info.connection_type}")
        print(f"  网卡接口: {self.network_info.interface_name}")
        print(f"  检测结果: {result}")
        print(f"  日志文件: {log_file_path}")
        
        # 保存到JSON文件
        try:
            with open(log_file_path, 'w', encoding='utf-8') as f:
                json.dump(log_entry, f, ensure_ascii=False, indent=2)
            print(f"\n✓ 日志已保存到: {log_file_path}")
        except Exception as e:
            print(f"\n⚠ 日志保存失败: {e}")
        
        self.check_results.append(log_entry)
        
        return log_entry
    
    def run_complete_check(self):
        """运行完整的网络链路检测流程"""
        print("\n" + "=" * 70)
        print("网络链路自检系统（支持WiFi和有线网络）")
        print("=" * 70 + "\n")
        print(f"📁 日志目录: {self.log_dir}")
        print()
        
        # 步骤1: 检查链路合法性
        if not self.check_link_validity():
            print("\n❌ 网络链路检测终止")
            return
        
        # 步骤2: 发送检测参数
        params = self.send_test_params()
        
        # 步骤3: 执行链路检测
        test_results = self.execute_link_test(params)
        
        # 步骤4: 分析结果
        result = self.analyze_results(test_results, params)
        
        # 步骤5: 保存日志
        log = self.save_log(test_results, result)
        
        print("\n" + "=" * 70)
        print(f"✓ 网络链路检测完成 - 结果: {result}")
        print("=" * 70)


# 主程序
if __name__ == "__main__":
    # 创建网络检测器
    checker = NetworkLinkChecker()
    
    # 运行完整检测
    checker.run_complete_check()
    
    # 查看历史记录
    print("\n\n" + "=" * 70)
    print("检测历史记录")
    print("=" * 70)
    
    if checker.check_results:
        for i, record in enumerate(checker.check_results, 1):
            print(f"\n记录 {i}:")
            print(f"  时间: {record['timestamp']}")
            print(f"  连接类型: {record['network_info']['connection_type']}")
            print(f"  IP地址: {record['network_info']['ip_address']}")
            print(f"  结果: {record['check_result']}")
    else:
        print("暂无检测记录")