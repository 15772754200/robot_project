#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
外设管理系统测试工具
"""

import json
from pathlib import Path

def test_query():
    """测试查询功能"""
    print("测试查询外设...")
    try:
        from peripheral_manager import PeripheralManager
        
        manager = PeripheralManager()
        devices = manager.query_peripherals()
        
        print(f"找到 {len(devices)} 个设备")
        for device in devices[:5]:  # 只显示前5个
            print(f"  - {device.device_id}: {device.device_type} ({device.protocol.value})")
    except Exception as e:
        print(f"查询测试失败: {str(e)}")

def test_logs():
    """测试日志功能"""
    print("\n测试日志功能...")
    log_dir = Path.home() / '.peripheral_manager' / 'logs'
    
    if log_dir.exists():
        for log_file in log_dir.glob('*.json'):
            print(f"  日志文件: {log_file.name}")
            
            try:
                logs = json.loads(log_file.read_text())
                print(f"    记录数: {len(logs)}")
                
                if logs:
                    latest = logs[-1]
                    print(f"    最新记录: {latest.get('timestamp', 'N/A')}")
            except Exception as e:
                print(f"    读取失败: {str(e)}")
    else:
        print("  日志目录不存在")

if __name__ == "__main__":
    print("=" * 50)
    print("外设管理系统测试")
    print("=" * 50)
    
    test_query()
    test_logs()
    
    print("\n测试完成！")