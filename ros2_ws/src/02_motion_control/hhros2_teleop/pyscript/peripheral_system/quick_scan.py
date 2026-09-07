
### 5. 快速测试脚本 `quick_scan.py`


#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快速扫描外设设备
"""

import subprocess
import os

def quick_scan():
    """快速扫描并显示外设摘要"""
    print("=" * 60)
    print(" " * 20 + "外设快速扫描")
    print("=" * 60)
    
    # USB设备
    print("\n[USB设备]")
    try:
        result = subprocess.run(['lsusb'], capture_output=True, text=True)
        usb_count = len(result.stdout.splitlines())
        print(f"  发现 {usb_count} 个USB设备")
        for line in result.stdout.splitlines()[:3]:  # 显示前3个
            print(f"  - {line[23:]}")  # 跳过Bus和Device信息
    except:
        print("  无法获取USB信息")
    
    # 串口设备
    print("\n[串口设备]")
    serial_ports = []
    for port in os.listdir('/dev'):
        if port.startswith('ttyUSB') or port.startswith('ttyACM'):
            serial_ports.append(f"/dev/{port}")
    
    if serial_ports:
        print(f"  发现 {len(serial_ports)} 个串口设备")
        for port in serial_ports:
            print(f"  - {port}")
    else:
        print("  未发现串口设备")
    
    # 网络接口
    print("\n[网络接口]")
    try:
        result = subprocess.run(['ip', 'link', 'show'], capture_output=True, text=True)
        interfaces = []
        for line in result.stdout.splitlines():
            if ':' in line and not line.startswith(' '):
                parts = line.split(':')
                if len(parts) >= 2:
                    iface = parts[1].strip()
                    if iface != 'lo':  # 排除回环接口
                        interfaces.append(iface)
        
        print(f"  发现 {len(interfaces)} 个网络接口")
        for iface in interfaces:
            print(f"  - {iface}")
    except:
        print("  无法获取网络接口信息")
    
    # 显示设备
    print("\n[显示设备]")
    try:
        result = subprocess.run(['xrandr'], capture_output=True, text=True)
        displays = []
        for line in result.stdout.splitlines():
            if ' connected' in line:
                display = line.split()[0]
                displays.append(display)
        
        if displays:
            print(f"  发现 {len(displays)} 个显示设备")
            for display in displays:
                print(f"  - {display}")
        else:
            print("  未发现显示设备")
    except:
        print("  无法获取显示设备信息")
    
    print("\n" + "=" * 60)
    print("扫描完成！运行 'python3 peripheral_manager.py' 查看详细信息")

if __name__ == "__main__":
    quick_scan()