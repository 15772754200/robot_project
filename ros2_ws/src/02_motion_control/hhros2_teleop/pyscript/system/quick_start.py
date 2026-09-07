#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
系统管理工具 - 快速启动版本
"""

import os
import psutil
from datetime import datetime

def quick_system_info():
    """快速显示系统信息"""
    print("=" * 60)
    print(" " * 20 + "系统快速检查")
    print("=" * 60)
    
    # CPU信息
    print(f"\nCPU使用率: {psutil.cpu_percent(interval=1)}%")
    print(f"CPU核心数: {psutil.cpu_count()}")
    
    # 内存信息
    mem = psutil.virtual_memory()
    print(f"\n内存使用: {mem.percent}%")
    print(f"总内存: {mem.total / (1024**3):.2f} GB")
    print(f"可用内存: {mem.available / (1024**3):.2f} GB")
    
    # 磁盘信息
    disk = psutil.disk_usage('/')
    print(f"\n磁盘使用: {disk.percent}%")
    print(f"总空间: {disk.total / (1024**3):.2f} GB")
    print(f"可用空间: {disk.free / (1024**3):.2f} GB")
    
    # 前5个CPU占用最高的进程
    print("\n" + "=" * 60)
    print("CPU占用最高的5个进程:")
    print("-" * 60)
    
    processes = []
    for proc in psutil.process_iter(['pid', 'name', 'cpu_percent']):
        try:
            processes.append(proc.info)
        except:
            pass
    
    processes.sort(key=lambda x: x['cpu_percent'], reverse=True)
    
    for i, proc in enumerate(processes[:5], 1):
        print(f"{i}. PID: {proc['pid']:6} | CPU: {proc['cpu_percent']:5.1f}% | {proc['name']}")
    
    print("=" * 60)

if __name__ == "__main__":
    try:
        quick_system_info()
    except KeyboardInterrupt:
        print("\n程序已退出")