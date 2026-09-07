#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
系统管理工具 - 进程和存储管理
开发人员：丁培峰
版本：1.0
"""

import os
import sys
import time
import json
import psutil
import subprocess
from datetime import datetime
from pathlib import Path
import logging
from tabulate import tabulate
import getpass
from typing import Dict, List, Tuple, Optional

def _resolve_log_dir() -> Path:
    """Resolve log directory, preferring the invoking user's home when running via sudo."""
    override = os.environ.get('SYSTEM_MANAGER_LOG_DIR')
    if override:
        return Path(override)

    sudo_user = os.environ.get('SUDO_USER')
    if sudo_user and sudo_user != 'root':
        try:
            import pwd  # type: ignore

            user_home = Path(pwd.getpwnam(sudo_user).pw_dir)
            return user_home / '.system_manager' / 'logs'
        except Exception:
            pass

    return Path.home() / '.system_manager' / 'logs'


# 配置日志
LOG_DIR = _resolve_log_dir()
LOG_DIR.mkdir(parents=True, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_DIR / 'system_manager.log', encoding='utf-8'),
        logging.StreamHandler()
    ]
)

logger = logging.getLogger(__name__)

class ProcessManager:
    """进程管理类"""
    
    def __init__(self):
        self.log_file = LOG_DIR / 'process_management.json'
        self.ensure_log_file()
    
    def ensure_log_file(self):
        """确保日志文件存在"""
        if not self.log_file.exists():
            self.log_file.write_text('[]')
    
    def validate_process_id(self, pid: int) -> bool:
        """验证进程ID是否存在"""
        return psutil.pid_exists(pid)
    
    def adjust_priority(self, pid: int, priority: int) -> Dict:
        """
        调整进程优先级
        priority: -20 (最高) 到 19 (最低)
        """
        result = {
            'success': False,
            'message': '',
            'data': {}
        }
        
        try:
            # 参数验证
            if not self.validate_process_id(pid):
                result['message'] = f'进程 {pid} 不存在'
                return result
            
            if priority < -20 or priority > 19:
                result['message'] = f'优先级必须在 -20 到 19 之间'
                return result
            
            # 获取进程信息
            process = psutil.Process(pid)
            old_nice = process.nice()
            
            # 调整优先级（需要sudo权限）
            if os.geteuid() != 0:
                # 尝试使用sudo
                cmd = f"sudo renice {priority} -p {pid}"
                subprocess.run(cmd, shell=True, check=True)
            else:
                process.nice(priority)
            
            # 记录到日志
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'pid': pid,
                'process_name': process.name(),
                'old_priority': old_nice,
                'new_priority': priority,
                'operation': 'adjust_priority',
                'operator': getpass.getuser()
            }
            
            self._save_log(log_entry)
            
            result['success'] = True
            result['message'] = f'进程 {pid} 优先级已从 {old_nice} 调整为 {priority}'
            result['data'] = {
                'pid': pid,
                'name': process.name(),
                'status': process.status(),
                'create_time': datetime.fromtimestamp(process.create_time()).isoformat(),
                'cpu_percent': process.cpu_percent(),
                'memory_percent': process.memory_percent(),
                'nice': priority
            }
            
        except subprocess.CalledProcessError:
            result['message'] = '权限不足，需要管理员权限'
        except Exception as e:
            result['message'] = f'操作失败: {str(e)}'
        
        return result
    
    def list_processes(self, filter_name: Optional[str] = None) -> List[Dict]:
        """查看进程列表"""
        processes = []
        
        for proc in psutil.process_iter(['pid', 'name', 'status', 'create_time', 
                                         'cpu_percent', 'memory_percent', 'nice']):
            try:
                pinfo = proc.info
                
                # 应用过滤条件
                if filter_name and filter_name.lower() not in pinfo['name'].lower():
                    continue
                
                processes.append({
                    'PID': pinfo['pid'],
                    '进程名': pinfo['name'],
                    '状态': pinfo['status'],
                    '优先级': pinfo['nice'],
                    'CPU(%)': round(pinfo['cpu_percent'], 2),
                    '内存(%)': round(pinfo['memory_percent'], 2),
                    '启动时间': datetime.fromtimestamp(pinfo['create_time']).strftime('%Y-%m-%d %H:%M:%S')
                })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        
        return processes
    
    def view_logs(self, process_name: Optional[str] = None, 
                  date_from: Optional[str] = None) -> List[Dict]:
        """查看进程管理日志"""
        logs = json.loads(self.log_file.read_text())
        filtered_logs = []
        
        for log in logs:
            # 按进程名过滤
            if process_name and process_name.lower() not in log.get('process_name', '').lower():
                continue
            
            # 按日期过滤
            if date_from:
                log_date = datetime.fromisoformat(log['timestamp'])
                filter_date = datetime.fromisoformat(date_from)
                if log_date < filter_date:
                    continue
            
            filtered_logs.append({
                '时间': log['timestamp'],
                'PID': log['pid'],
                '进程名': log['process_name'],
                '操作': log['operation'],
                '旧优先级': log.get('old_priority', 'N/A'),
                '新优先级': log.get('new_priority', 'N/A'),
                '操作员': log['operator']
            })
        
        return filtered_logs
    
    def kill_process(self, pid: int, signal: int = 15) -> Dict:
        """
        终止进程
        signal: 15(SIGTERM, 优雅终止) 或 9(SIGKILL, 强制终止)
        """
        result = {
            'success': False,
            'message': '',
            'data': {}
        }

        try:
            # 参数验证
            if not self.validate_process_id(pid):
                result['message'] = f'进程 {pid} 不存在'
                return result

            if signal not in [9, 15]:
                result['message'] = f'信号参数无效，必须为 9(SIGKILL) 或 15(SIGTERM)'
                return result

            # 获取进程信息（在终止前）
            process = psutil.Process(pid)
            process_info = {
                'pid': pid,
                'name': process.name(),
                'status': process.status(),
                'create_time': datetime.fromtimestamp(process.create_time()).isoformat(),
                'cpu_percent': process.cpu_percent(),
                'memory_percent': process.memory_percent()
            }

            # 检查是否为系统关键进程
            critical_processes = ['systemd', 'init', 'kthreadd']
            if process.name() in critical_processes:
                result['message'] = f'警告: {process.name()} 是系统关键进程，不允许终止'
                return result

            # 终止进程
            if signal == 15:
                process.terminate()
                try:
                    process.wait(timeout=5)
                    result['message'] = f'进程 {pid} ({process.name()}) 已优雅终止'
                except psutil.TimeoutExpired:
                    # 优雅终止超时，询问是否强制终止
                    result['message'] = f'进程 {pid} 未响应SIGTERM信号，可能需要使用SIGKILL'
                    result['data'] = process_info
                    result['data']['needs_force'] = True
                    return result
            else:  # signal == 9
                process.kill()
                result['message'] = f'进程 {pid} ({process.name()}) 已强制终止'

            # 记录到日志
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'pid': pid,
                'process_name': process_info['name'],
                'signal': signal,
                'signal_name': 'SIGTERM' if signal == 15 else 'SIGKILL',
                'operation': 'kill_process',
                'operator': getpass.getuser()
            }

            self._save_log(log_entry)

            result['success'] = True
            result['data'] = process_info

        except psutil.AccessDenied:
            result['message'] = f'权限不足，无法终止进程 {pid}。可能需要管理员权限'
        except psutil.NoSuchProcess:
            result['message'] = f'进程 {pid} 已不存在'
        except Exception as e:
            result['message'] = f'操作失败: {str(e)}'

        return result

    def _save_log(self, entry: Dict):
        """保存日志条目"""
        logs = json.loads(self.log_file.read_text())
        logs.append(entry)
        self.log_file.write_text(json.dumps(logs, indent=2, ensure_ascii=False))

class StorageManager:
    """存储管理类"""
    
    def __init__(self):
        self.cleanup_log = LOG_DIR / 'storage_cleanup.json'
        self.ensure_log_file()
    
    def ensure_log_file(self):
        """确保日志文件存在"""
        if not self.cleanup_log.exists():
            self.cleanup_log.write_text('[]')
    
    def query_storage(self, partition: Optional[str] = None) -> List[Dict]:
        """查询存储空间"""
        storage_info = []
        
        if partition:
            # 查询指定分区
            if os.path.exists(partition):
                usage = psutil.disk_usage(partition)
                storage_info.append(self._format_storage_info(partition, usage))
        else:
            # 查询所有分区
            partitions = psutil.disk_partitions()
            for part in partitions:
                try:
                    usage = psutil.disk_usage(part.mountpoint)
                    storage_info.append(self._format_storage_info(part.mountpoint, usage, part.fstype))
                except PermissionError:
                    continue
        
        return storage_info
    
    def _format_storage_info(self, mountpoint: str, usage, fstype: str = None) -> Dict:
        """格式化存储信息"""
        return {
            '挂载点': mountpoint,
            '文件系统': fstype or 'N/A',
            '总容量': self._format_bytes(usage.total),
            '已使用': self._format_bytes(usage.used),
            '可用': self._format_bytes(usage.free),
            '使用率': f"{usage.percent}%"
        }
    
    def cleanup_storage(self, target_path: str, operation: str = 'analyze') -> Dict:
        """
        存储清理
        operation: analyze(分析), delete_cache(删除缓存), compress(压缩)
        """
        result = {
            'success': False,
            'message': '',
            'data': {}
        }
        
        try:
            if not os.path.exists(target_path):
                result['message'] = f'路径 {target_path} 不存在'
                return result
            
            if operation == 'analyze':
                # 分析目录大小
                total_size = 0
                file_count = 0
                
                for dirpath, dirnames, filenames in os.walk(target_path):
                    for filename in filenames:
                        filepath = os.path.join(dirpath, filename)
                        try:
                            total_size += os.path.getsize(filepath)
                            file_count += 1
                        except:
                            pass
                
                result['success'] = True
                result['message'] = '分析完成'
                result['data'] = {
                    '路径': target_path,
                    '文件数量': file_count,
                    '总大小': self._format_bytes(total_size)
                }
                
            elif operation == 'delete_cache':
                # 清理缓存文件（仅清理安全的缓存目录）
                cache_dirs = [
                    os.path.expanduser('~/.cache/thumbnails'),
                    '/tmp',
                    '/var/tmp'
                ]
                
                cleaned_size = 0
                for cache_dir in cache_dirs:
                    if cache_dir.startswith(target_path) or target_path.startswith(cache_dir):
                        # 安全清理
                        cmd = f"find {cache_dir} -type f -atime +7 -delete 2>/dev/null"
                        subprocess.run(cmd, shell=True)
                        cleaned_size += 1  # 简化计算
                
                result['success'] = True
                result['message'] = '缓存清理完成'
                result['data'] = {'清理路径': target_path, '状态': '完成'}
                
            elif operation == 'compress':
                # 压缩备份
                timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
                archive_name = f"backup_{timestamp}.tar.gz"
                archive_path = f"/tmp/{archive_name}"
                
                cmd = f"tar -czf {archive_path} -C {os.path.dirname(target_path)} {os.path.basename(target_path)}"
                subprocess.run(cmd, shell=True, check=True)
                
                result['success'] = True
                result['message'] = f'压缩完成: {archive_path}'
                result['data'] = {
                    '源路径': target_path,
                    '压缩文件': archive_path,
                    '文件大小': self._format_bytes(os.path.getsize(archive_path))
                }
            
            # 记录操作日志
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'operation': operation,
                'target_path': target_path,
                'result': result['data'],
                'operator': getpass.getuser()
            }
            self._save_log(log_entry)
            
        except Exception as e:
            result['message'] = f'操作失败: {str(e)}'
        
        return result
    
    def _format_bytes(self, bytes_value: int) -> str:
        """格式化字节大小"""
        for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
            if bytes_value < 1024.0:
                return f"{bytes_value:.2f} {unit}"
            bytes_value /= 1024.0
        return f"{bytes_value:.2f} PB"
    
    def _save_log(self, entry: Dict):
        """保存清理日志"""
        logs = json.loads(self.cleanup_log.read_text())
        logs.append(entry)
        self.cleanup_log.write_text(json.dumps(logs, indent=2, ensure_ascii=False))

class SystemManagerUI:
    """系统管理界面"""
    
    def __init__(self):
        self.process_mgr = ProcessManager()
        self.storage_mgr = StorageManager()
        self.running = True
    
    def clear_screen(self):
        """清屏"""
        os.system('clear')
    
    def print_header(self):
        
        """打印头部信息"""
        self.clear_screen()
        print("=" * 80)
        print(" " * 20 + "Ubuntu 系统管理工具 v1.0")
        print(" " * 25 + f"开发者: 丁培峰")
        print(" " * 20 + f"当前用户: {getpass.getuser()}")
        print(" " * 20 + f"当前时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 80)
    
    def main_menu(self):
        """主菜单"""
        while self.running:
            self.print_header()
            print("\n主菜单:")
            print("1. 进程管理")
            print("2. 存储管理")
            print("3. 查看系统日志")
            print("0. 退出")
            print("-" * 40)
            
            choice = input("请选择操作 [0-3]: ").strip()
            
            if choice == '1':
                self.process_menu()
            elif choice == '2':
                self.storage_menu()
            elif choice == '3':
                self.log_menu()
            elif choice == '0':
                self.running = False
                print("\n感谢使用，再见！")
            else:
                print("\n无效选择，请重试")
                time.sleep(1)
    
    def process_menu(self):
        """进程管理菜单"""
        while True:
            self.print_header()
            print("\n进程管理:")
            print("1. 查看进程列表")
            print("2. 调整进程优先级")
            print("3. 搜索进程")
            print("4. 终止进程")
            print("0. 返回主菜单")
            print("-" * 40)

            choice = input("请选择操作 [0-4]: ").strip()

            if choice == '1':
                self.view_processes()
            elif choice == '2':
                self.adjust_process_priority()
            elif choice == '3':
                self.search_process()
            elif choice == '4':
                self.kill_process()
            elif choice == '0':
                break
            else:
                print("\n无效选择，请重试")
                time.sleep(1)
    
    def view_processes(self):
        """查看进程列表"""
        self.print_header()
        print("\n进程列表 (按CPU使用率排序):")
        print("-" * 80)
        
        processes = self.process_mgr.list_processes()
        # 按CPU使用率排序
        processes.sort(key=lambda x: x['CPU(%)'], reverse=True)
        
        # 只显示前20个进程
        if len(processes) > 20:
            processes = processes[:20]
            
        if processes:
            print(tabulate(processes, headers='keys', tablefmt='grid'))
            print(f"\n显示前 20 个进程，共 {len(self.process_mgr.list_processes())} 个进程")
        else:
            print("没有找到进程")
        
        input("\n按回车键继续...")
    
    def adjust_process_priority(self):
        """调整进程优先级"""
        self.print_header()
        print("\n调整进程优先级:")
        print("-" * 40)
        
        try:
            pid = int(input("请输入进程ID: ").strip())
            print("\n优先级范围: -20 (最高) 到 19 (最低)")
            print("普通用户只能降低优先级，提高优先级需要root权限")
            priority = int(input("请输入新的优先级 [-20 到 19]: ").strip())
            
            # 确认操作
            confirm = input(f"\n确认调整进程 {pid} 的优先级为 {priority}? (y/n): ").strip().lower()
            
            if confirm == 'y':
                result = self.process_mgr.adjust_priority(pid, priority)
                
                if result['success']:
                    print(f"\n✓ {result['message']}")
                    if result['data']:
                        print("\n进程信息:")
                        for key, value in result['data'].items():
                            print(f"  {key}: {value}")
                else:
                    print(f"\n✗ {result['message']}")
            else:
                print("\n操作已取消")
                
        except ValueError:
            print("\n✗ 输入无效，请输入数字")
        except Exception as e:
            print(f"\n✗ 操作失败: {str(e)}")
        
        input("\n按回车键继续...")
    
    def search_process(self):
        """搜索进程"""
        self.print_header()
        print("\n搜索进程:")
        print("-" * 40)

        name = input("请输入进程名称（部分匹配）: ").strip()

        if name:
            processes = self.process_mgr.list_processes(filter_name=name)

            if processes:
                print(f"\n找到 {len(processes)} 个匹配的进程:")
                print(tabulate(processes, headers='keys', tablefmt='grid'))
            else:
                print(f"\n没有找到包含 '{name}' 的进程")

        input("\n按回车键继续...")

    def kill_process(self):
        """终止进程"""
        self.print_header()
        print("\n终止进程:")
        print("-" * 40)
        print("信号说明:")
        print("  15 (SIGTERM) - 优雅终止，允许进程清理资源")
        print("  9  (SIGKILL) - 强制立即终止")
        print()

        try:
            pid = int(input("请输入要终止的进程ID: ").strip())
            signal_input = input("请选择信号 [15=优雅终止, 9=强制终止，默认15]: ").strip()

            # 默认使用SIGTERM
            signal = 15 if not signal_input else int(signal_input)

            if signal not in [9, 15]:
                print("\n✗ 无效的信号值，请输入 9 或 15")
                input("\n按回车键继续...")
                return

            # 确认操作
            signal_name = "优雅终止(SIGTERM)" if signal == 15 else "强制终止(SIGKILL)"
            confirm = input(f"\n确认使用 {signal_name} 终止进程 {pid}? (y/n): ").strip().lower()

            if confirm == 'y':
                result = self.process_mgr.kill_process(pid, signal)

                if result['success']:
                    print(f"\n✓ {result['message']}")
                    if result['data'] and 'needs_force' in result['data']:
                        # 进程未响应SIGTERM，询问是否使用SIGKILL
                        force = input("\n是否使用 SIGKILL 强制终止? (y/n): ").strip().lower()
                        if force == 'y':
                            result = self.process_mgr.kill_process(pid, 9)
                            if result['success']:
                                print(f"\n✓ {result['message']}")
                            else:
                                print(f"\n✗ {result['message']}")
                else:
                    print(f"\n✗ {result['message']}")
            else:
                print("\n操作已取消")

        except ValueError:
            print("\n✗ 输入无效，请输入数字")
        except Exception as e:
            print(f"\n✗ 操作失败: {str(e)}")

        input("\n按回车键继续...")
    
    def storage_menu(self):
        """存储管理菜单"""
        while True:
            self.print_header()
            print("\n存储管理:")
            print("1. 查看存储空间")
            print("2. 分析目录大小")
            print("3. 清理缓存")
            print("4. 压缩备份")
            print("0. 返回主菜单")
            print("-" * 40)
            
            choice = input("请选择操作 [0-4]: ").strip()
            
            if choice == '1':
                self.view_storage()
            elif choice == '2':
                self.analyze_directory()
            elif choice == '3':
                self.cleanup_cache()
            elif choice == '4':
                self.compress_backup()
            elif choice == '0':
                break
            else:
                print("\n无效选择，请重试")
                time.sleep(1)
    
    def view_storage(self):
        """查看存储空间"""
        self.print_header()
        print("\n存储空间信息:")
        print("-" * 80)
        
        storage_info = self.storage_mgr.query_storage()
        
        if storage_info:
            print(tabulate(storage_info, headers='keys', tablefmt='grid'))
            
            # 计算总使用率
            total_used = sum(float(s['使用率'].rstrip('%')) for s in storage_info) / len(storage_info)
            print(f"\n平均使用率: {total_used:.1f}%")
            
            if total_used > 80:
                print("⚠ 警告：存储空间使用率较高，建议清理！")
        else:
            print("无法获取存储信息")
        
        input("\n按回车键继续...")
    
    def analyze_directory(self):
        """分析目录大小"""
        self.print_header()
        print("\n分析目录大小:")
        print("-" * 40)
        
        path = input("请输入要分析的目录路径 [默认: 当前用户主目录]: ").strip()
        if not path:
            path = os.path.expanduser('~')
        
        print(f"\n正在分析 {path} ...")
        result = self.storage_mgr.cleanup_storage(path, 'analyze')
        
        if result['success']:
            print(f"\n✓ {result['message']}")
            print("\n分析结果:")
            for key, value in result['data'].items():
                print(f"  {key}: {value}")
        else:
            print(f"\n✗ {result['message']}")
        
        input("\n按回车键继续...")
    
    def cleanup_cache(self):
        """清理缓存"""
        self.print_header()
        print("\n清理缓存:")
        print("-" * 40)
        print("将清理以下目录的缓存文件：")
        print("  - ~/.cache/thumbnails")
        print("  - /tmp (7天未访问的文件)")
        print("  - /var/tmp (7天未访问的文件)")
        
        confirm = input("\n确认清理缓存? (y/n): ").strip().lower()
        
        if confirm == 'y':
            result = self.storage_mgr.cleanup_storage('/tmp', 'delete_cache')
            
            if result['success']:
                print(f"\n✓ {result['message']}")
            else:
                print(f"\n✗ {result['message']}")
        else:
            print("\n操作已取消")
        
        input("\n按回车键继续...")
    
    def compress_backup(self):
        """压缩备份"""
        self.print_header()
        print("\n压缩备份:")
        print("-" * 40)
        
        path = input("请输入要备份的目录路径: ").strip()
        
        if path and os.path.exists(path):
            confirm = input(f"\n确认压缩备份 {path}? (y/n): ").strip().lower()
            
            if confirm == 'y':
                print(f"\n正在压缩 {path} ...")
                result = self.storage_mgr.cleanup_storage(path, 'compress')
                
                if result['success']:
                    print(f"\n✓ {result['message']}")
                    print("\n备份信息:")
                    for key, value in result['data'].items():
                        print(f"  {key}: {value}")
                else:
                    print(f"\n✗ {result['message']}")
            else:
                print("\n操作已取消")
        else:
            print("\n✗ 路径无效或不存在")
        
        input("\n按回车键继续...")
    
    def log_menu(self):
        """日志查看菜单"""
        while True:
            self.print_header()
            print("\n系统日志:")
            print("1. 查看进程管理日志")
            print("2. 查看存储清理日志")
            print("0. 返回主菜单")
            print("-" * 40)
            
            choice = input("请选择操作 [0-2]: ").strip()
            
            if choice == '1':
                self.view_process_logs()
            elif choice == '2':
                self.view_storage_logs()
            elif choice == '0':
                break
            else:
                print("\n无效选择，请重试")
                time.sleep(1)
    
    def view_process_logs(self):
        """查看进程管理日志"""
        self.print_header()
        print("\n进程管理日志:")
        print("-" * 80)
        
        logs = self.process_mgr.view_logs()
        
        if logs:
            # 只显示最近20条
            if len(logs) > 20:
                logs = logs[-20:]
            
            print(tabulate(logs, headers='keys', tablefmt='grid'))
            print(f"\n显示最近 {len(logs)} 条记录")
        else:
            print("暂无日志记录")
        
        input("\n按回车键继续...")
    
    def view_storage_logs(self):
        """查看存储清理日志"""
        self.print_header()
        print("\n存储清理日志:")
        print("-" * 80)
        
        try:
            logs = json.loads(self.storage_mgr.cleanup_log.read_text())
            
            if logs:
                # 格式化显示
                formatted_logs = []
                for log in logs[-20:]:  # 最近20条
                    formatted_logs.append({
                        '时间': log['timestamp'],
                        '操作': log['operation'],
                        '目标路径': log['target_path'],
                        '操作员': log['operator']
                    })
                
                print(tabulate(formatted_logs, headers='keys', tablefmt='grid'))
                print(f"\n显示最近 {len(formatted_logs)} 条记录")
            else:
                print("暂无日志记录")
        except:
            print("暂无日志记录")
        
        input("\n按回车键继续...")

def main():
    """主函数"""
    try:
        # 检查是否需要提升权限
        if os.geteuid() != 0:
            print("提示：某些功能需要管理员权限才能执行")
            print("建议使用 'sudo python3 system_manager.py' 运行以获得完整功能")
            print()
            input("按回车键继续以普通用户权限运行...")
        
        # 启动UI
        ui = SystemManagerUI()
        ui.main_menu()
        
    except KeyboardInterrupt:
        print("\n\n程序被用户中断")
    except Exception as e:
        logger.error(f"程序异常: {str(e)}")
        print(f"\n程序异常: {str(e)}")

if __name__ == "__main__":
    main()
