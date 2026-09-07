#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32
import os
from datetime import datetime
import time

class ControlModeLogger(Node):
    def __init__(self):
        super().__init__('control_mode_logger')
        
        # 控制模式映射字典
        self.control_mode_map = {
            -1: "还未启动",
            0: "准备姿态",
            1: "RL行走",
            2: "RL站立",
            3: "RL跑步",
            4: "RL跳跃",
            5: "RL模仿",
            9: "软状态"
        }
        
        # 记录上一次的控制模式值，初始化为None表示没有历史记录
        self.last_control_mode = None
        self.message_count = 0
        self.change_count = 0
        
        # 获取当前日期和时间，用于创建log文件名
        current_time = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # 创建log目录（如果不存在）
        log_dir = os.path.join(os.path.expanduser("~"), "control_mode_logs")
        os.makedirs(log_dir, exist_ok=True)
        
        # 设置log文件路径
        self.log_file = os.path.join(log_dir, f"control_mode_{current_time}.log")
        
        # 创建订阅者
        self.subscription = self.create_subscription(
            Int32,
            '/control_mode_publisher',
            self.control_mode_callback,
            10
        )
        
        # 初始化log文件
        with open(self.log_file, 'w') as f:
            f.write(f"Control Mode Change Log - Started at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("=" * 100 + "\n")
            f.write("Timestamp, Mode Code, Mode Description, Duration (s), Message Count\n")
            f.write("-" * 100 + "\n")
        
        # 记录模式开始时间
        self.mode_start_time = time.time()
        self.start_time = time.time()
        
        self.get_logger().info(f"Control mode change logger started. Log file: {self.log_file}")
        self.get_logger().info("Only logging when control mode changes.")
        self.get_logger().info("Waiting for messages on /control_mode_publisher...")
        
        # 打印控制模式映射表
        self.get_logger().info("Control mode mapping:")
        for code, description in sorted(self.control_mode_map.items()):
            self.get_logger().info(f"  {code}: {description}")
    
    def get_mode_description(self, mode_code):
        """根据模式代码获取描述"""
        if mode_code in self.control_mode_map:
            return self.control_mode_map[mode_code]
        else:
            return f"未知模式({mode_code})"
    
    def control_mode_callback(self, msg):
        """处理接收到的控制模式消息，只在模式变化时记录"""
        self.message_count += 1
        current_time = time.time()
        mode_code = msg.data
        mode_description = self.get_mode_description(mode_code)
        
        # 如果这是第一条消息或者控制模式发生了变化
        if self.last_control_mode is None or mode_code != self.last_control_mode:
            # 计算当前模式持续时间
            mode_duration = current_time - self.mode_start_time
            
            # 获取当前时间戳
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            
            # 如果是第一次记录（不是模式切换），不记录持续时间
            if self.last_control_mode is None:
                log_entry = f"{timestamp}, {mode_code}, {mode_description}, First Mode, {self.message_count}\n"
                self.get_logger().info(f"初始模式: {mode_code}({mode_description}) 时间: {timestamp}")
            else:
                # 获取上一次模式的描述
                last_mode_description = self.get_mode_description(self.last_control_mode)
                # 记录模式切换
                log_entry = f"{timestamp}, {mode_code}, {mode_description}, {mode_duration:.3f}, {self.message_count}\n"
                self.get_logger().info(f"模式切换: {self.last_control_mode}({last_mode_description}) -> "
                                     f"{mode_code}({mode_description}) "
                                     f"(持续时间: {mode_duration:.2f}秒, 消息计数: {self.message_count})")
            
            # 写入log文件
            with open(self.log_file, 'a') as f:
                f.write(log_entry)
            
            # 更新状态
            self.last_control_mode = mode_code
            self.mode_start_time = current_time
            self.change_count += 1
            
            # 每10次变化打印一次统计信息
            if self.change_count % 10 == 0:
                elapsed_time = current_time - self.start_time
                avg_time_per_msg = elapsed_time / self.message_count if self.message_count > 0 else 0
                self.get_logger().info(f"统计信息: 变化次数={self.change_count}, "
                                      f"消息总数={self.message_count}, "
                                      f"运行时间={elapsed_time:.1f}秒, "
                                      f"平均消息间隔={avg_time_per_msg:.3f}秒")
    
    def __del__(self):
        """析构函数，记录结束时间和统计信息"""
        if hasattr(self, 'log_file'):
            end_time = time.time()
            total_duration = end_time - self.start_time
            
            with open(self.log_file, 'a') as f:
                f.write("=" * 100 + "\n")
                f.write("SUMMARY:\n")
                f.write(f"  Log ended at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"  Total duration: {total_duration:.2f} seconds\n")
                f.write(f"  Total messages received: {self.message_count}\n")
                f.write(f"  Total mode changes: {self.change_count}\n")
                f.write(f"  Average time per message: {total_duration/self.message_count:.3f}s\n" 
                       if self.message_count > 0 else "  No messages received\n")
                
                if self.change_count > 0:
                    f.write(f"  Average messages per change: {self.message_count/self.change_count:.1f}\n")
                    f.write(f"  Average time per mode: {total_duration/self.change_count:.2f}s\n")
                
                # 添加模式统计
                f.write("\n  Mode mapping reference:\n")
                for code, description in sorted(self.control_mode_map.items()):
                    f.write(f"    {code}: {description}\n")
                
                f.write("=" * 100 + "\n")

def main(args=None):
    rclpy.init(args=args)
    
    try:
        node = ControlModeLogger()
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info("正在关闭控制模式日志记录器...")
    except Exception as e:
        node.get_logger().error(f"错误: {e}")
    finally:
        if 'node' in locals():
            node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()