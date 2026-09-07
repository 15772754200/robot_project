#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
运控主板外设管理系统
开发人员：丁培峰
版本：1.3
功能：查询、管理和记录外设设备信息（增加存储设备支持）
"""

import os
import sys
import time
import json
import subprocess
import re
from datetime import datetime
from pathlib import Path
import logging
from typing import Dict, List, Optional, Tuple
from enum import Enum
import serial
import serial.tools.list_ports
import psutil
import socket
import unicodedata

# 尝试导入可选模块
try:
    from tabulate import tabulate
    HAS_TABULATE = True
except ImportError:
    HAS_TABULATE = False

def _resolve_log_dir() -> Path:
    """Resolve log directory, preferring the invoking user's home when running via sudo."""
    override = os.environ.get('PERIPHERAL_MANAGER_LOG_DIR')
    if override:
        return Path(override)

    candidates = []

    sudo_user = os.environ.get('SUDO_USER')
    if sudo_user and sudo_user != 'root':
        candidates.append(sudo_user)

    try:
        login_user = os.getlogin()
        if login_user and login_user != 'root':
            candidates.append(login_user)
    except OSError:
        pass

    for env_var in ('LOGNAME', 'USER', 'USERNAME'):
        value = os.environ.get(env_var)
        if value and value != 'root':
            candidates.append(value)

    # Deduplicate while preserving order
    seen = set()
    unique_candidates = []
    for name in candidates:
        if name not in seen:
            unique_candidates.append(name)
            seen.add(name)

    for username in unique_candidates:
        try:
            import pwd  # type: ignore

            user_home = Path(pwd.getpwnam(username).pw_dir)
            return user_home / '.peripheral_manager' / 'logs'
        except Exception:
            continue

    return Path.home() / '.peripheral_manager' / 'logs'

# 配置日志目录
LOG_DIR = _resolve_log_dir()
LOG_DIR.mkdir(parents=True, exist_ok=True)

# 配置系统日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_DIR / 'peripheral_manager.log', encoding='utf-8'),
        logging.StreamHandler()
    ]
)

logger = logging.getLogger(__name__)

class ProtocolType(Enum):
    """通信协议类型"""
    USB = "USB"
    SERIAL = "串口"
    ETHERNET = "以太网"
    BLUETOOTH = "蓝牙"
    I2C = "I2C"
    SPI = "SPI"
    CAN = "CAN"
    WIFI = "WiFi"
    SATA = "SATA"
    NVME = "NVMe"

class DeviceStatus(Enum):
    """设备状态"""
    CONNECTED = "已连接"
    DISCONNECTED = "未连接"
    ENABLED = "已启用"
    DISABLED = "已禁用"
    MOUNTED = "已挂载"
    UNMOUNTED = "未挂载"
    ERROR = "错误"
    UNKNOWN = "未知"

class OperationType(Enum):
    """操作类型"""
    QUERY = "查询"
    ENABLE = "启用"
    DISABLE = "禁用"
    EXPORT = "导出"
    VIEW_LOG = "查看日志"
    SCAN = "扫描"
    MOUNT = "挂载"
    UNMOUNT = "卸载"

class Peripheral:
    """外设设备类"""
    
    def __init__(self, device_id: str, device_type: str, protocol: ProtocolType, 
                 port: str = None, status: DeviceStatus = DeviceStatus.UNKNOWN):
        self.device_id = device_id
        self.device_type = device_type
        self.protocol = protocol
        self.port = port
        self.status = status
        self.enabled = False
        self.vendor = ""
        self.product = ""
        self.serial_number = ""
        self.driver = ""
        self.description = ""
        self.parameters = {}
        self.last_seen = datetime.now()
    
    def to_dict(self) -> Dict:
        """转换为字典"""
        return {
            'device_id': self.device_id,
            'device_type': self.device_type,
            'protocol': self.protocol.value if isinstance(self.protocol, ProtocolType) else self.protocol,
            'port': self.port,
            'status': self.status.value if isinstance(self.status, DeviceStatus) else self.status,
            'enabled': self.enabled,
            'vendor': self.vendor,
            'product': self.product,
            'serial_number': self.serial_number,
            'driver': self.driver,
            'description': self.description,
            'parameters': self.parameters,
            'last_seen': self.last_seen.isoformat()
        }
    
    def get_display_info(self) -> Dict:
        """获取显示信息（根据设备类型返回不同的关键信息）"""
        info = {
            'ID': self.device_id,
            '类型': self.device_type,
            '协议': self.protocol.value if isinstance(self.protocol, ProtocolType) else self.protocol,
            '端口': self.port or 'N/A',
            '状态': self.status.value if isinstance(self.status, DeviceStatus) else self.status,
        }
        
        # 根据不同类型设备显示不同信息
        if self.device_type == "存储设备":
            # 存储设备显示容量和挂载信息
            info['容量'] = self.parameters.get('size', 'N/A')
            info['已用'] = self.parameters.get('used', 'N/A')
            info['挂载点'] = self.parameters.get('mountpoint', '未挂载')
            info['文件系统'] = self.parameters.get('fstype', 'N/A')
            info['设备名'] = self.parameters.get('device', 'N/A')
            
        elif self.protocol == ProtocolType.USB:
            info['厂商ID'] = self.vendor or 'N/A'
            info['产品ID'] = self.product or 'N/A'
            info['描述'] = self.description[:30] if self.description else 'N/A'
            
        elif self.protocol == ProtocolType.SERIAL:
            info['描述'] = self.parameters.get('description', 'N/A')[:30]
            info['硬件ID'] = self.parameters.get('hwid', 'N/A')[:20]
            
        elif self.protocol in [ProtocolType.ETHERNET, ProtocolType.WIFI]:
            info['MAC地址'] = self.parameters.get('mac_address', 'N/A')
            info['IP地址'] = self.parameters.get('ip_address', 'N/A')
            info['启用'] = '是' if self.enabled else '否'
            
        elif self.protocol == ProtocolType.BLUETOOTH:
            info['蓝牙地址'] = self.parameters.get('bd_address', 'N/A')
            info['启用'] = '是' if self.enabled else '否'
            
        else:
            # 显示设备
            if 'resolution' in self.parameters:
                info['分辨率'] = self.parameters.get('resolution', 'N/A')
            if 'primary' in self.parameters:
                info['主显示器'] = '是' if self.parameters.get('primary') else '否'
            # 音频设备
            if 'card_name' in self.parameters:
                info['卡名'] = self.parameters.get('card_name', 'N/A')
                info['描述'] = self.parameters.get('description', 'N/A')[:30]
        
        return info


class PeripheralScanner:
    """外设扫描器"""
    
    def __init__(self):
        self.devices = []
        self.usb_storage_map = {}  # USB设备到存储设备的映射
    
    def scan_all(self) -> List[Peripheral]:
        """扫描所有外设"""
        self.devices = []
        
        # 扫描USB设备
        self.scan_usb_devices()
        
        # 扫描存储设备（包括USB存储）
        self.scan_storage_devices()
        
        # 扫描串口设备
        self.scan_serial_devices()
        
        # 扫描网络接口
        self.scan_network_interfaces()
        
        # 扫描蓝牙设备
        self.scan_bluetooth_devices()
        
        # 扫描显示设备
        self.scan_display_devices()
        
        # 扫描音频设备
        self.scan_audio_devices()
        
        logger.info(f"扫描完成，发现 {len(self.devices)} 个设备")
        
        return self.devices
    
    def scan_usb_devices(self) -> List[Peripheral]:
        """扫描USB设备"""
        try:
            # 使用lsusb命令获取USB设备
            result = subprocess.run(['lsusb'], capture_output=True, text=True)
            
            for line in result.stdout.splitlines():
                # 解析lsusb输出: Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
                match = re.match(r'Bus (\d+) Device (\d+): ID ([0-9a-f]+):([0-9a-f]+) (.+)', line)
                if match:
                    bus, device, vendor_id, product_id, description = match.groups()
                    
                    peripheral = Peripheral(
                        device_id=f"USB_{bus}_{device}",
                        device_type="USB设备",
                        protocol=ProtocolType.USB,
                        port=f"Bus{bus}/Dev{device}",
                        status=DeviceStatus.CONNECTED
                    )
                    peripheral.vendor = vendor_id
                    peripheral.product = product_id
                    peripheral.description = description
                    peripheral.parameters = {
                        'description': description,
                        'bus': bus,
                        'device': device
                    }
                    
                    self.devices.append(peripheral)
                    
        except Exception as e:
            logger.error(f"扫描USB设备失败: {str(e)}")
        
        return self.devices
    
    def scan_storage_devices(self) -> List[Peripheral]:
        """扫描存储设备（包括USB存储、SATA、NVMe等）"""
        try:
            # 使用lsblk获取块设备信息
            result = subprocess.run(
                ['lsblk', '-J', '-o', 'NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE,MODEL,VENDOR,TRAN,RM'],
                capture_output=True,
                text=True
            )
            
            if result.returncode == 0:
                import json
                lsblk_data = json.loads(result.stdout)
                
                for device in lsblk_data.get('blockdevices', []):
                    self._process_block_device(device, None)
            
            # 备用方法：使用df和mount命令
            self._scan_mounted_filesystems()
            
        except Exception as e:
            logger.error(f"扫描存储设备失败: {str(e)}")
            # 使用备用方法
            self._scan_mounted_filesystems()
        
        return self.devices
    
    def _process_block_device(self, device: Dict, parent_device: Optional[str] = None):
        """处理块设备信息"""
        try:
            device_name = device.get('name', '')
            device_type = device.get('type', '')
            
            # 只处理磁盘和分区
            if device_type in ['disk', 'part']:
                # 判断协议类型
                tran = device.get('tran', '')
                removable = device.get('rm', False)
                
                if tran == 'usb' or removable:
                    protocol = ProtocolType.USB
                    device_type_str = "USB存储"
                elif tran == 'sata':
                    protocol = ProtocolType.SATA
                    device_type_str = "SATA硬盘"
                elif tran == 'nvme':
                    protocol = ProtocolType.NVME
                    device_type_str = "NVMe固态硬盘"
                else:
                    protocol = ProtocolType.SATA  # 默认
                    device_type_str = "存储设备"
                
                # 创建设备对象
                device_id = f"STORAGE_{device_name.upper()}"
                peripheral = Peripheral(
                    device_id=device_id,
                    device_type="存储设备",
                    protocol=protocol,
                    port=f"/dev/{device_name}",
                    status=DeviceStatus.MOUNTED if device.get('mountpoint') else DeviceStatus.UNMOUNTED
                )
                
                # 设置设备信息
                model_value = device.get('model') or 'Unknown'
                peripheral.description = f"{device_type_str} - {model_value}"

                vendor_value = device.get('vendor')
                if isinstance(vendor_value, str):
                    vendor_value = vendor_value.strip()
                elif vendor_value is not None:
                    vendor_value = str(vendor_value).strip()
                else:
                    vendor_value = ''

                peripheral.vendor = vendor_value or 'Unknown'
                
                # 获取容量信息
                size = device.get('size', 'N/A')
                mountpoint = device.get('mountpoint', '')
                
                # 如果有挂载点，获取使用情况
                used = 'N/A'
                free = 'N/A'
                use_percent = 'N/A'
                
                if mountpoint:
                    try:
                        usage = psutil.disk_usage(mountpoint)
                        used = self._format_bytes(usage.used)
                        free = self._format_bytes(usage.free)
                        use_percent = f"{usage.percent}%"
                        size = self._format_bytes(usage.total)  # 使用实际容量
                    except:
                        pass
                
                fstype_value = device.get('fstype') or 'N/A'

                peripheral.parameters = {
                    'device': f"/dev/{device_name}",
                    'size': size,
                    'used': used,
                    'free': free,
                    'use_percent': use_percent,
                    'mountpoint': mountpoint or '未挂载',
                    'fstype': fstype_value,
                    'model': model_value,
                    'removable': '是' if removable else '否'
                }
                
                self.devices.append(peripheral)
            
            # 递归处理子设备（分区）
            for child in device.get('children', []):
                self._process_block_device(child, device_name)
                
        except Exception as e:
            logger.error(f"处理块设备失败: {str(e)}")
    
    def _scan_mounted_filesystems(self):
        """扫描已挂载的文件系统（备用方法）"""
        try:
            # 获取所有磁盘分区
            partitions = psutil.disk_partitions(all=False)
            
            for partition in partitions:
                # 跳过特殊文件系统
                if partition.fstype in ['tmpfs', 'devtmpfs', 'squashfs']:
                    continue
                
                device_name = partition.device.replace('/dev/', '')
                
                # 检查是否已经添加过
                if any(d.device_id == f"STORAGE_{device_name.upper()}" for d in self.devices):
                    continue
                
                # 判断设备类型
                protocol = ProtocolType.SATA  # 默认
                if 'usb' in partition.opts or device_name.startswith('sd'):
                    if self._is_usb_device(partition.device):
                        protocol = ProtocolType.USB
                
                peripheral = Peripheral(
                    device_id=f"STORAGE_{device_name.upper()}",
                    device_type="存储设备",
                    protocol=protocol,
                    port=partition.device,
                    status=DeviceStatus.MOUNTED
                )
                
                # 获取磁盘使用情况
                try:
                    usage = psutil.disk_usage(partition.mountpoint)
                    
                    peripheral.parameters = {
                        'device': partition.device,
                        'size': self._format_bytes(usage.total),
                        'used': self._format_bytes(usage.used),
                        'free': self._format_bytes(usage.free),
                        'use_percent': f"{usage.percent}%",
                        'mountpoint': partition.mountpoint,
                        'fstype': partition.fstype,
                        'opts': partition.opts
                    }
                    
                    self.devices.append(peripheral)
                    
                except PermissionError:
                    continue
                    
        except Exception as e:
            logger.error(f"扫描文件系统失败: {str(e)}")
    
    def _is_usb_device(self, device_path: str) -> bool:
        """判断是否为USB设备"""
        try:
            device_name = os.path.basename(device_path)
            
            # 检查/sys/block/<device>/removable
            removable_path = f"/sys/block/{device_name}/removable"
            if os.path.exists(removable_path):
                with open(removable_path, 'r') as f:
                    if f.read().strip() == '1':
                        return True
            
            # 检查设备路径是否包含usb
            device_link = f"/sys/block/{device_name}/device"
            if os.path.exists(device_link):
                real_path = os.path.realpath(device_link)
                if 'usb' in real_path:
                    return True
                    
        except:
            pass
        
        return False
    
    def _format_bytes(self, bytes_value: int) -> str:
        """格式化字节大小"""
        for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
            if bytes_value < 1024.0:
                return f"{bytes_value:.2f} {unit}"
            bytes_value /= 1024.0
        return f"{bytes_value:.2f} PB"
    
    def scan_serial_devices(self) -> List[Peripheral]:
        """扫描串口设备"""
        try:
            # 使用pyserial扫描串口
            ports = serial.tools.list_ports.comports()
            
            for port in ports:
                peripheral = Peripheral(
                    device_id=f"SERIAL_{port.device.replace('/', '_')}",
                    device_type="串口设备",
                    protocol=ProtocolType.SERIAL,
                    port=port.device,
                    status=DeviceStatus.CONNECTED
                )
                
                peripheral.vendor = str(port.vid) if port.vid else ""
                peripheral.product = str(port.pid) if port.pid else ""
                peripheral.serial_number = port.serial_number or ""
                peripheral.description = port.description
                peripheral.parameters = {
                    'description': port.description,
                    'hwid': port.hwid,
                    'interface': port.interface or ""
                }
                
                self.devices.append(peripheral)
                
        except Exception as e:
            logger.error(f"扫描串口设备失败: {str(e)}")
        
        return self.devices
    
    def scan_network_interfaces(self) -> List[Peripheral]:
        """扫描网络接口"""
        try:
            interfaces = psutil.net_if_addrs()
            stats = psutil.net_if_stats()
            
            for interface_name, addrs in interfaces.items():
                # 跳过回环接口
                if interface_name == 'lo':
                    continue
                
                # 判断接口类型
                if interface_name.startswith('eth') or interface_name.startswith('enp') or interface_name.startswith('ens'):
                    protocol = ProtocolType.ETHERNET
                    device_type = "以太网"
                elif interface_name.startswith('wl'):
                    protocol = ProtocolType.WIFI
                    device_type = "WiFi"
                else:
                    protocol = ProtocolType.ETHERNET
                    device_type = "网络接口"
                
                # 获取接口状态
                is_up = stats[interface_name].isup if interface_name in stats else False
                
                peripheral = Peripheral(
                    device_id=f"NET_{interface_name}",
                    device_type=device_type,
                    protocol=protocol,
                    port=interface_name,
                    status=DeviceStatus.CONNECTED if is_up else DeviceStatus.DISCONNECTED
                )
                
                peripheral.enabled = is_up
                
                # 获取MAC地址和IP地址
                mac_addr = ""
                ip_addr = ""
                for addr in addrs:
                    if addr.family == socket.AF_INET:
                        ip_addr = addr.address
                    elif addr.family == psutil.AF_LINK:
                        mac_addr = addr.address
                
                peripheral.parameters = {
                    'mac_address': mac_addr,
                    'ip_address': ip_addr,
                    'is_up': is_up,
                    'speed': stats[interface_name].speed if interface_name in stats else 0
                }
                
                self.devices.append(peripheral)
                
        except Exception as e:
            logger.error(f"扫描网络接口失败: {str(e)}")
        
        return self.devices
    
    def scan_bluetooth_devices(self) -> List[Peripheral]:
        """扫描蓝牙设备"""
        try:
            # 检查蓝牙适配器
            result = subprocess.run(['hciconfig'], capture_output=True, text=True)
            
            for line in result.stdout.splitlines():
                if line.startswith('hci'):
                    # 解析蓝牙适配器信息
                    match = re.match(r'(hci\d+):\s+Type:\s+(\S+)', line)
                    if match:
                        device_name, device_type = match.groups()
                        
                        peripheral = Peripheral(
                            device_id=f"BT_{device_name}",
                            device_type="蓝牙",
                            protocol=ProtocolType.BLUETOOTH,
                            port=device_name,
                            status=DeviceStatus.CONNECTED
                        )
                        
                        # 获取蓝牙地址
                        addr_result = subprocess.run(
                            ['hciconfig', device_name], 
                            capture_output=True, 
                            text=True
                        )
                        
                        for addr_line in addr_result.stdout.splitlines():
                            if 'BD Address:' in addr_line:
                                bd_addr = addr_line.split('BD Address:')[1].split()[0]
                                peripheral.parameters['bd_address'] = bd_addr
                                break
                        
                        self.devices.append(peripheral)
                        
        except FileNotFoundError:
            logger.info("蓝牙工具未安装，跳过蓝牙设备扫描")
        except Exception as e:
            logger.error(f"扫描蓝牙设备失败: {str(e)}")
        
        return self.devices
    
    def scan_display_devices(self) -> List[Peripheral]:
        """扫描显示设备"""
        try:
            # 使用xrandr获取显示器信息
            result = subprocess.run(['xrandr'], capture_output=True, text=True)
            
            for line in result.stdout.splitlines():
                # 匹配输出端口
                if ' connected' in line:
                    parts = line.split()
                    port_name = parts[0]
                    
                    peripheral = Peripheral(
                        device_id=f"DISPLAY_{port_name}",
                        device_type="显示器",
                        protocol=ProtocolType.USB,
                        port=port_name,
                        status=DeviceStatus.CONNECTED
                    )
                    
                    # 解析分辨率
                    if 'primary' in line:
                        peripheral.parameters['primary'] = True
                    
                    # 查找分辨率
                    resolution_match = re.search(r'(\d+x\d+)', line)
                    if resolution_match:
                        peripheral.parameters['resolution'] = resolution_match.group(1)
                    
                    self.devices.append(peripheral)
                    
        except FileNotFoundError:
            logger.info("xrandr未安装，跳过显示设备扫描")
        except Exception as e:
            logger.error(f"扫描显示设备失败: {str(e)}")
        
        return self.devices
    
    def scan_audio_devices(self) -> List[Peripheral]:
        """扫描音频设备"""
        try:
            # 使用aplay和arecord获取音频设备
            playback_result = subprocess.run(
                ['aplay', '-l'], 
                capture_output=True, 
                text=True
            )
            
            for line in playback_result.stdout.splitlines():
                if line.startswith('card'):
                    # 解析音频设备信息
                    match = re.match(r'card (\d+): (\S+) ```math(.+?)```', line)
                    if match:
                        card_num, card_name, card_desc = match.groups()
                        
                        peripheral = Peripheral(
                            device_id=f"AUDIO_{card_num}",
                            device_type="音频设备",
                            protocol=ProtocolType.USB,
                            port=f"card{card_num}",
                            status=DeviceStatus.CONNECTED
                        )
                        
                        peripheral.parameters = {
                            'card_name': card_name,
                            'description': card_desc,
                            'type': 'playback'
                        }
                        
                        self.devices.append(peripheral)
                        
        except FileNotFoundError:
            logger.info("音频工具未安装，跳过音频设备扫描")
        except Exception as e:
            logger.error(f"扫描音频设备失败: {str(e)}")
        
        return self.devices
class OperationLogger:
    """操作日志记录器"""
    
    def __init__(self):
        self.operation_log_file = LOG_DIR / 'operation_log.json'
        self.ensure_log_file()
    
    def ensure_log_file(self):
        """确保日志文件存在"""
        if not self.operation_log_file.exists():
            self.operation_log_file.write_text('[]')
    
    def log_operation(self, operation_type: OperationType, details: Dict, 
                     success: bool = True, user: str = None):
        """记录操作日志"""
        try:
            logs = json.loads(self.operation_log_file.read_text())
            
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'operation': operation_type.value,
                'details': details,
                'success': success,
                'user': user or os.getenv('USER', 'unknown'),
                'pid': os.getpid()
            }
            
            logs.append(log_entry)
            
            # 只保留最近2000条记录
            if len(logs) > 2000:
                logs = logs[-2000:]
            
            self.operation_log_file.write_text(
                json.dumps(logs, indent=2, ensure_ascii=False)
            )
            
            # 同时记录到系统日志
            if success:
                logger.info(f"操作成功: {operation_type.value} - {details}")
            else:
                logger.warning(f"操作失败: {operation_type.value} - {details}")
                
        except Exception as e:
            logger.error(f"记录操作日志失败: {str(e)}")
    
    def get_operation_logs(self, operation_type: Optional[OperationType] = None,
                          start_time: Optional[str] = None,
                          limit: int = 50) -> List[Dict]:
        """获取操作日志"""
        try:
            logs = json.loads(self.operation_log_file.read_text())
            filtered_logs = []
            
            for log in reversed(logs):  # 最新的在前
                # 按操作类型过滤
                if operation_type and log.get('operation') != operation_type.value:
                    continue
                
                # 按时间过滤
                if start_time:
                    log_time = datetime.fromisoformat(log['timestamp'])
                    filter_time = datetime.fromisoformat(start_time)
                    if log_time < filter_time:
                        continue
                
                filtered_logs.append(log)
                
                if len(filtered_logs) >= limit:
                    break
            
            return filtered_logs
            
        except Exception as e:
            logger.error(f"获取操作日志失败: {str(e)}")
            return []

class PeripheralManager:
    """外设管理器"""
    
    def __init__(self):
        self.scanner = PeripheralScanner()
        self.devices = []
        self.connection_log_file = LOG_DIR / 'connection_log.json'
        self.enable_log_file = LOG_DIR / 'enable_log.json'
        self.operation_logger = OperationLogger()
        self.ensure_log_files()
    
    def ensure_log_files(self):
        """确保日志文件存在"""
        if not self.connection_log_file.exists():
            self.connection_log_file.write_text('[]')
        if not self.enable_log_file.exists():
            self.enable_log_file.write_text('[]')
    
    def query_peripherals(self, protocol_type: Optional[str] = None, 
                          port_type: Optional[str] = None) -> List[Peripheral]:
        """查询外设信息"""
        # 记录查询操作
        self.operation_logger.log_operation(
            OperationType.QUERY,
            {'protocol_type': protocol_type, 'port_type': port_type}
        )
        
        # 扫描所有设备
        self.devices = self.scanner.scan_all()
        
        # 应用过滤条件
        filtered_devices = self.devices
        
        if protocol_type:
            filtered_devices = [
                d for d in filtered_devices 
                if d.protocol.value.lower() == protocol_type.lower()
            ]
        
        if port_type:
            filtered_devices = [
                d for d in filtered_devices 
                if port_type.lower() in str(d.port).lower()
            ]
        
        # 记录连接日志
        self._log_connection(filtered_devices)
        
        return filtered_devices
    
    def enable_peripheral(self, device_id: str, enable: bool = True) -> Dict:
        """启用或禁用外设"""
        result = {
            'success': False,
            'message': '',
            'device': None
        }
        
        # 查找设备
        device = None
        for d in self.devices:
            if d.device_id == device_id:
                device = d
                break
        
        if not device:
            result['message'] = f'设备 {device_id} 未找到'
            self.operation_logger.log_operation(
                OperationType.ENABLE if enable else OperationType.DISABLE,
                {'device_id': device_id, 'error': '设备未找到'},
                success=False
            )
            return result
        
        try:
            # 根据设备类型执行不同的启用/禁用操作
            if device.protocol == ProtocolType.ETHERNET or device.protocol == ProtocolType.WIFI:
                # 网络接口的启用/禁用
                action = 'up' if enable else 'down'
                cmd = f"sudo ip link set {device.port} {action}"
                subprocess.run(cmd, shell=True, check=True)
                
            elif device.protocol == ProtocolType.BLUETOOTH:
                # 蓝牙设备的启用/禁用
                action = 'up' if enable else 'down'
                cmd = f"sudo hciconfig {device.port} {action}"
                subprocess.run(cmd, shell=True, check=True)
                
            # 更新设备状态
            device.enabled = enable
            device.status = DeviceStatus.ENABLED if enable else DeviceStatus.DISABLED
            
            # 记录启用日志
            self._log_enable(device, enable)
            
            # 记录操作日志
            self.operation_logger.log_operation(
                OperationType.ENABLE if enable else OperationType.DISABLE,
                {
                    'device_id': device_id,
                    'device_type': device.device_type,
                    'protocol': device.protocol.value
                },
                success=True
            )
            
            result['success'] = True
            result['message'] = f'设备 {device_id} {"已启用" if enable else "已禁用"}'
            result['device'] = device
            
        except subprocess.CalledProcessError as e:
            result['message'] = f'操作失败: 需要管理员权限'
            self.operation_logger.log_operation(
                OperationType.ENABLE if enable else OperationType.DISABLE,
                {'device_id': device_id, 'error': '权限不足'},
                success=False
            )
        except Exception as e:
            result['message'] = f'操作失败: {str(e)}'
            self.operation_logger.log_operation(
                OperationType.ENABLE if enable else OperationType.DISABLE,
                {'device_id': device_id, 'error': str(e)},
                success=False
            )
        
        return result
    
    def export_device_list(self, file_path: Optional[str] = None) -> str:
        """导出设备列表"""
        try:
            if not file_path:
                timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
                file_path = LOG_DIR / f'device_list_{timestamp}.json'
            
            export_data = {
                'export_time': datetime.now().isoformat(),
                'device_count': len(self.devices),
                'devices': [device.to_dict() for device in self.devices]
            }
            
            Path(file_path).write_text(
                json.dumps(export_data, indent=2, ensure_ascii=False)
            )
            
            # 记录操作日志
            self.operation_logger.log_operation(
                OperationType.EXPORT,
                {'file_path': str(file_path), 'device_count': len(self.devices)},
                success=True
            )
            
            return str(file_path)
            
        except Exception as e:
            logger.error(f"导出设备列表失败: {str(e)}")
            self.operation_logger.log_operation(
                OperationType.EXPORT,
                {'error': str(e)},
                success=False
            )
            return ""
    
    def get_connection_logs(self, device_id: Optional[str] = None, 
                           start_time: Optional[str] = None) -> List[Dict]:
        """获取连接日志"""
        self.operation_logger.log_operation(
            OperationType.VIEW_LOG,
            {'log_type': 'connection', 'device_id': device_id}
        )
        
        logs = json.loads(self.connection_log_file.read_text())
        filtered_logs = []
        
        for log in logs:
            # 按设备ID过滤
            if device_id and log.get('device_id') != device_id:
                continue
            
            # 按时间过滤
            if start_time:
                log_time = datetime.fromisoformat(log['timestamp'])
                filter_time = datetime.fromisoformat(start_time)
                if log_time < filter_time:
                    continue
            
            filtered_logs.append(log)
        
        return filtered_logs
    
    def get_enable_logs(self, device_id: Optional[str] = None, 
                       start_time: Optional[str] = None) -> List[Dict]:
        """获取启用日志"""
        self.operation_logger.log_operation(
            OperationType.VIEW_LOG,
            {'log_type': 'enable', 'device_id': device_id}
        )
        
        logs = json.loads(self.enable_log_file.read_text())
        filtered_logs = []
        
        for log in logs:
            # 按设备ID过滤
            if device_id and log.get('device_id') != device_id:
                continue
            
            # 按时间过滤
            if start_time:
                log_time = datetime.fromisoformat(log['timestamp'])
                filter_time = datetime.fromisoformat(start_time)
                if log_time < filter_time:
                    continue
            
            filtered_logs.append(log)
        
        return filtered_logs
    
    def _log_connection(self, devices: List[Peripheral]):
        """记录连接日志"""
        logs = json.loads(self.connection_log_file.read_text())
        
        for device in devices:
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'device_id': device.device_id,
                'device_type': device.device_type,
                'protocol': device.protocol.value if isinstance(device.protocol, ProtocolType) else device.protocol,
                'port': device.port,
                'status': device.status.value if isinstance(device.status, DeviceStatus) else device.status,
                'vendor': device.vendor,
                'product': device.product,
                'parameters': device.parameters,
                'file_path': str(self.connection_log_file),
                'file_name': self.connection_log_file.name
            }
            logs.append(log_entry)
        
        # 只保留最近1000条记录
        if len(logs) > 1000:
            logs = logs[-1000:]
        
        self.connection_log_file.write_text(json.dumps(logs, indent=2, ensure_ascii=False))
    
    def _log_enable(self, device: Peripheral, enabled: bool):
        """记录启用日志"""
        logs = json.loads(self.enable_log_file.read_text())
        
        log_entry = {
            'timestamp': datetime.now().isoformat(),
            'device_id': device.device_id,
            'device_type': device.device_type,
            'protocol': device.protocol.value if isinstance(device.protocol, ProtocolType) else device.protocol,
            'port': device.port,
            'enabled': enabled,
            'status': '已启用' if enabled else '已禁用',
            'file_path': str(self.enable_log_file),
            'file_name': self.enable_log_file.name
        }
        logs.append(log_entry)
        
        # 只保留最近1000条记录
        if len(logs) > 1000:
            logs = logs[-1000:]
        
        self.enable_log_file.write_text(json.dumps(logs, indent=2, ensure_ascii=False))

class PeripheralManagerUI:
    """外设管理界面"""
    
    def __init__(self):
        self.manager = PeripheralManager()
        self.running = True
    
    def clear_screen(self):
        """清屏"""
        os.system('clear')
    
    def print_header(self):
        """打印头部信息"""
        self.clear_screen()
        print("=" * 100)
        print(" " * 30 + "运控主板外设管理系统 v1.2")
        print(" " * 35 + "开发者: 丁培峰")
        print(" " * 30 + f"当前时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 100)
    
    def display_table(self, data: List[Dict], headers='keys'):
        """显示表格（优化中文显示）"""
        if not data:
            print("没有数据")
            return
        
        if HAS_TABULATE:
            # 使用tabulate，设置表格格式为simple以获得更好的显示
            print(tabulate(data, headers=headers, tablefmt='simple', stralign='left', numalign='left'))
        else:
            # 手动格式化表格
            if headers == 'keys' and data:
                headers = list(data[0].keys())
            
            # 计算每列的最大宽度
            col_widths = {}
            for header in headers:
                col_widths[header] = len(str(header))
                for row in data:
                    width = len(str(row.get(header, '')))
                    col_widths[header] = max(col_widths[header], width)
            
            # 打印表头
            header_parts = []
            for h in headers:
                header_parts.append(str(h).ljust(col_widths[h]))
            print(" | ".join(header_parts))
            print("-" * (sum(col_widths.values()) + len(headers) * 3 - 1))
            
            # 打印数据行
            for row in data:
                row_parts = []
                for h in headers:
                    value = str(row.get(h, ''))
                    row_parts.append(value.ljust(col_widths[h]))
                print(" | ".join(row_parts))
    
    def main_menu(self):
        """主菜单"""
        while self.running:
            self.print_header()
            print("\n主菜单:")
            print("1. 查询外设信息（详细）")
            print("2. 查询外设信息（简洁）")
            print("3. 启用/禁用外设")
            print("4. 查看连接日志")
            print("5. 查看启用日志")
            print("6. 查看操作日志")
            print("7. 外设统计信息")
            print("8. 导出设备列表")
            print("0. 退出")
            print("-" * 50)
            
            choice = input("请选择操作 [0-8]: ").strip()
            
            if choice == '1':
                self.query_peripherals_menu(detailed=True)
            elif choice == '2':
                self.query_peripherals_menu(detailed=False)
            elif choice == '3':
                self.enable_peripheral_menu()
            elif choice == '4':
                self.view_connection_logs()
            elif choice == '5':
                self.view_enable_logs()
            elif choice == '6':
                self.view_operation_logs()
            elif choice == '7':
                self.view_statistics()
            elif choice == '8':
                self.export_device_list()
            elif choice == '0':
                self.running = False
                print("\n感谢使用，再见！")
            else:
                print("\n无效选择，请重试")
                time.sleep(1)
    
    def query_peripherals_menu(self, detailed=True):
        """查询外设菜单"""
        self.print_header()
        print("\n查询外设信息" + ("（详细模式）" if detailed else "（简洁模式）") + ":")
        print("-" * 50)
        print("可用的协议类型:")
        for i, protocol in enumerate(ProtocolType, 1):
            print(f"  {i}. {protocol.value}")
        print("  0. 查询所有")
        print("-" * 50)
        
        choice = input("请选择协议类型 [0-8]: ").strip()
        
        protocol_filter = None
        if choice != '0' and choice.isdigit():
            protocols = list(ProtocolType)
            idx = int(choice) - 1
            if 0 <= idx < len(protocols):
                protocol_filter = protocols[idx].value
        
        print("\n正在扫描外设...")
        devices = self.manager.query_peripherals(protocol_type=protocol_filter)
        
        self.print_header()
        print(f"\n找到 {len(devices)} 个外设设备:")
        print("-" * 100)
        
        if detailed:
            # 详细模式：显示更多信息
            display_data = []
            for device in devices:
                display_data.append(device.get_display_info())
            
            if display_data:
                self.display_table(display_data)
        else:
            # 简洁模式：只显示基本信息
            display_data = []
            for device in devices:
                display_data.append({
                    'ID': device.device_id,
                    '类型': device.device_type,
                    '协议': device.protocol.value if isinstance(device.protocol, ProtocolType) else device.protocol,
                    '端口': device.port or 'N/A',
                    '状态': device.status.value if isinstance(device.status, DeviceStatus) else device.status,
                })
            
            if display_data:
                self.display_table(display_data)
        
        if not devices:
            print("没有找到外设设备")
        
        input("\n按回车键继续...")
    
    def enable_peripheral_menu(self):
        """启用/禁用外设菜单"""
        self.print_header()
        print("\n启用/禁用外设:")
        print("-" * 50)
        
        # 先显示当前设备列表
        print("正在扫描外设...")
        devices = self.manager.query_peripherals()
        
        print(f"\n当前外设列表 ({len(devices)} 个):")
        print("-" * 80)
        
        # 只显示可以启用/禁用的设备
        controllable_devices = []
        for device in devices:
            if device.protocol in [ProtocolType.ETHERNET, ProtocolType.WIFI, ProtocolType.BLUETOOTH]:
                info = device.get_display_info()
                controllable_devices.append({
                    'ID': info['ID'],
                    '类型': info['类型'],
                    '端口': info['端口'],
                    '状态': info['状态'],
                    '详情': info.get('IP地址', info.get('蓝牙地址', 'N/A'))
                })
        
        if controllable_devices:
            self.display_table(controllable_devices)
            
            print("\n" + "-" * 50)
            device_id = input("请输入要操作的设备ID: ").strip()
            
            if device_id:
                action = input("选择操作 (1=启用, 2=禁用): ").strip()
                
                if action == '1':
                    result = self.manager.enable_peripheral(device_id, True)
                elif action == '2':
                    result = self.manager.enable_peripheral(device_id, False)
                else:
                    print("无效的操作")
                    input("\n按回车键继续...")
                    return
                
                if result['success']:
                    print(f"\n✓ {result['message']}")
                else:
                    print(f"\n✗ {result['message']}")
        else:
            print("没有可控制的设备（仅支持网络接口和蓝牙设备）")
        
        input("\n按回车键继续...")
    
    def view_connection_logs(self):
        """查看连接日志"""
        self.print_header()
        print("\n连接日志:")
        print("-" * 100)
        
        # 获取日志
        logs = self.manager.get_connection_logs()
        
        if logs:
            # 只显示最近20条
            recent_logs = logs[-20:]
            
            display_data = []
            for log in recent_logs:
                display_data.append({
                    '时间': log['timestamp'][:19],
                    '设备ID': log['device_id'],
                    '类型': log['device_type'],
                    '协议': log['protocol'],
                    '状态': log['status']
                })
            
            self.display_table(display_data)
            print(f"\n显示最近 {len(recent_logs)} 条记录（共 {len(logs)} 条）")
        else:
            print("暂无连接日志")
        
        input("\n按回车键继续...")
    
    def view_enable_logs(self):
        """查看启用日志"""
        self.print_header()
        print("\n启用/禁用日志:")
        print("-" * 100)
        
        # 获取日志
        logs = self.manager.get_enable_logs()
        
        if logs:
            # 只显示最近20条
            recent_logs = logs[-20:]
            
            display_data = []
            for log in recent_logs:
                display_data.append({
                    '时间': log['timestamp'][:19],
                    '设备ID': log['device_id'],
                    '类型': log['device_type'],
                    '端口': log['port'],
                    '操作': log['status']
                })
            
            self.display_table(display_data)
            print(f"\n显示最近 {len(recent_logs)} 条记录（共 {len(logs)} 条）")
        else:
            print("暂无启用日志")
        
        input("\n按回车键继续...")
    
    def view_operation_logs(self):
        """查看操作日志"""
        self.print_header()
        print("\n操作日志:")
        print("-" * 100)
        
        # 获取日志
        logs = self.manager.operation_logger.get_operation_logs(limit=20)
        
        if logs:
            display_data = []
            for log in logs:
                display_data.append({
                    '时间': log['timestamp'][:19],
                    '操作': log['operation'],
                    '用户': log['user'],
                    '成功': '是' if log['success'] else '否',
                    '详情': str(log.get('details', {}))[:40]
                })
            
            self.display_table(display_data)
            print(f"\n显示最近 {len(logs)} 条记录")
        else:
            print("暂无操作日志")
        
        input("\n按回车键继续...")
    
    def view_statistics(self):
        """查看统计信息"""
        self.print_header()
        print("\n外设统计信息:")
        print("-" * 100)
        
        # 扫描设备
        devices = self.manager.query_peripherals()
        
        # 统计各类设备数量
        stats = {}
        for device in devices:
            protocol = device.protocol.value if isinstance(device.protocol, ProtocolType) else device.protocol
            if protocol not in stats:
                stats[protocol] = {
                    'total': 0,
                    'connected': 0,
                    'enabled': 0
                }
            
            stats[protocol]['total'] += 1
            if device.status == DeviceStatus.CONNECTED:
                stats[protocol]['connected'] += 1
            if device.enabled:
                stats[protocol]['enabled'] += 1
        
        # 显示统计
        print(f"总设备数: {len(devices)}")
        print("\n按协议类型统计:")
        print("-" * 50)
        
        display_data = []
        for protocol, stat in stats.items():
            display_data.append({
                '协议类型': protocol,
                '总数': stat['total'],
                '已连接': stat['connected'],
                '已启用': stat['enabled']
            })
        
        if display_data:
            self.display_table(display_data)
        
        # 显示USB设备分类统计
        print("\n\nUSB设备详细统计:")
        print("-" * 50)
        usb_stats = {}
        for device in devices:
            if device.protocol == ProtocolType.USB:
                desc = device.description[:30] if device.description else "未知设备"
                vendor = device.vendor or "未知"
                key = f"{vendor} - {desc}"
                usb_stats[key] = usb_stats.get(key, 0) + 1
        
        if usb_stats:
            for key, count in sorted(usb_stats.items()):
                print(f"  {key}: {count} 个")
        else:
            print("  无USB设备")
        
        input("\n按回车键继续...")
    
    def export_device_list(self):
        """导出设备列表"""
        self.print_header()
        print("\n导出设备列表:")
        print("-" * 50)
        
        # 扫描设备
        print("正在扫描设备...")
        devices = self.manager.query_peripherals()
        
        # 导出
        export_path = self.manager.export_device_list()
        
        if export_path:
            print(f"✓ 设备列表已导出到:")
            print(f"  {export_path}")
            print(f"\n共导出 {len(devices)} 个设备信息")
            
            # 显示导出的设备概要
            print("\n导出设备概要:")
            print("-" * 30)
            protocol_count = {}
            for device in devices:
                protocol = device.protocol.value if isinstance(device.protocol, ProtocolType) else device.protocol
                protocol_count[protocol] = protocol_count.get(protocol, 0) + 1
            
            for protocol, count in protocol_count.items():
                print(f"  {protocol}: {count} 个")
        else:
            print("✗ 导出失败")
        
        input("\n按回车键继续...")

def main():
    """主函数"""
    try:
        # 检查权限
        if os.geteuid() != 0:
            print("提示：某些功能（如启用/禁用网络接口）需要管理员权限")
            print("建议使用 'sudo python3 peripheral_manager.py' 运行")
            print()
            input("按回车键继续以普通用户权限运行...")
        
        # 启动UI
        ui = PeripheralManagerUI()
        ui.main_menu()
        
    except KeyboardInterrupt:
        print("\n\n程序被用户中断")
    except Exception as e:
        logger.error(f"程序异常: {str(e)}")
        print(f"\n程序异常: {str(e)}")

if __name__ == "__main__":
    main()
