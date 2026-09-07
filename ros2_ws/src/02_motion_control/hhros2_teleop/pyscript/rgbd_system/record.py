#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2
import numpy as np
import threading
import datetime
import os
import time
from pathlib import Path
# 配置日志
LOG_DIR = Path("run_logs/camera/")
LOG_DIR.mkdir(parents=True, exist_ok=True)
LOG_DIR.chmod(0o777)
# ========= 日志函数 =========
def write_log(msg, log_file=None):
    """统一日志写入"""
    if log_file is None:
        LOG_DIR = Path("run_logs/camera/")
        os.makedirs(LOG_DIR, exist_ok=True)
        log_file = os.path.join(LOG_DIR, "realsense_default.log")
    with open(log_file, "a") as f:
        f.write(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {msg}\n")
    print(f"📝 日志写入: {msg}")


# ========= 主类 =========
class RealSenseRecorder(Node):
    def __init__(self):
        super().__init__('realsense_recorder')
        self.bridge = CvBridge()

        # 缓存与状态
        self.rgb_frame = None
        self.depth_frame = None
        self.recording = False
        self.video_writer_rgb = None
        self.video_writer_depth = None
        self.record_thread = None

        # 相机参数
        self.rgb_size = None
        self.depth_size = None
        self.rgb_last_time = None
        self.depth_last_time = None
        self.rgb_fps_est = 30.0
        self.depth_fps_est = 30.0

        # ========= 改进后的日志逻辑 =========
        self.log_dir = LOG_DIR
        os.makedirs(LOG_DIR, exist_ok=True)
        self.log_file = self._get_latest_log()
        write_log("RealSense 录制节点启动", self.log_file)
        # ==================================

        # 订阅相机话题
        self.create_subscription(Image, "/camera/camera/color/image_raw", self.rgb_callback, 10)
        self.create_subscription(Image, "/camera/camera/depth/image_rect_raw", self.depth_callback, 10)
        print("✅ 订阅 RealSense 图像话题成功")

    # ========= 改进：查找最新日志文件 =========
    def _get_latest_log(self):
        """
        查找 ./run_logs/camera/ 目录中最新的 realsense_start_*.log 文件。
        若存在则复用，否则创建新文件。
        """

        prefix = "realsense_start_"
        logs = [f for f in os.listdir(LOG_DIR) if f.startswith(prefix) and f.endswith(".log")]

        if not logs:
            # 没有日志文件，新建
            new_path = os.path.join(
                LOG_DIR, f"{prefix}{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.log"
            )
            print(f"🆕 创建新日志文件: {new_path}")
            return new_path

        # 提取时间排序
        def extract_time(name):
            try:
                t = name[len(prefix):-4]
                return datetime.datetime.strptime(t, "%Y-%m-%d_%H-%M-%S")
            except Exception:
                return datetime.datetime.min

        logs.sort(key=extract_time, reverse=True)
        latest = os.path.join(LOG_DIR, logs[0])
        print(f"🧭 已找到最新日志文件: {latest}")
        return latest

    # ========= 回调函数 =========
    def rgb_callback(self, msg):
        self.rgb_frame = self.bridge.imgmsg_to_cv2(msg, desired_encoding="bgr8")
        h, w, _ = self.rgb_frame.shape
        self.rgb_size = (w, h)
        now = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
        if self.rgb_last_time is not None:
            dt = now - self.rgb_last_time
            if 0.001 < dt < 1.0:
                self.rgb_fps_est = 1.0 / dt
        self.rgb_last_time = now

    def depth_callback(self, msg):
        depth = self.bridge.imgmsg_to_cv2(msg, desired_encoding="passthrough")
        depth_norm = cv2.normalize(depth, None, 0, 255, cv2.NORM_MINMAX)
        self.depth_frame = cv2.convertScaleAbs(depth_norm)
        h, w = self.depth_frame.shape
        self.depth_size = (w, h)
        now = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
        if self.depth_last_time is not None:
            dt = now - self.depth_last_time
            if 0.001 < dt < 1.0:
                self.depth_fps_est = 1.0 / dt
        self.depth_last_time = now

    # ========= 拍照 =========
    def take_photo(self, camera_type="both", noise_level="none"):
        print("📸 等待图像数据中 ...")
        start = time.time()
        while True:
            if camera_type in ["rgb", "both"] and self.rgb_frame is not None:
                break
            if camera_type in ["depth", "both"] and self.depth_frame is not None:
                break
            if time.time() - start > 5:
                print("⚠️ 超时未接收到图像，拍照失败。")
                return
            rclpy.spin_once(self, timeout_sec=0.1)

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

        if camera_type in ["rgb", "both"] and self.rgb_frame is not None:
            img = self._apply_denoise(self.rgb_frame, noise_level)
            rgb_path = os.path.join(self.log_dir, f"rgb_photo_{timestamp}.jpg")
            cv2.imwrite(rgb_path, img)
            write_log(f"拍照完成 | 类型: RGB | 文件: {rgb_path}", self.log_file)

        if camera_type in ["depth", "both"] and self.depth_frame is not None:
            img = self._apply_denoise(self.depth_frame, noise_level)
            depth_path = os.path.join(self.log_dir, f"depth_photo_{timestamp}.png")
            cv2.imwrite(depth_path, img)
            write_log(f"拍照完成 | 类型: Depth | 文件: {depth_path}", self.log_file)

    # ========= 开始录像 =========
    def start_record(self, camera_type="both", noise_level="none", duration=None):
        if self.recording:
            print("⚠️ 已经在录像中，请先停止。")
            return

        print("⏳ 等待相机帧信息 ...")
        start_wait = time.time()
        while True:
            ready = (
                (camera_type == "rgb" and self.rgb_size) or
                (camera_type == "depth" and self.depth_size) or
                (camera_type == "both" and self.rgb_size and self.depth_size)
            )
            if ready:
                break
            if time.time() - start_wait > 5:
                print("⚠️ 未检测到相机图像，no法启动录像。")
                return
            rclpy.spin_once(self, timeout_sec=0.1)

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.recording = True
        self.camera_type = camera_type
        self.noise_level = noise_level
        self.duration = duration

        fourcc = cv2.VideoWriter_fourcc(*'XVID')

        if camera_type in ["rgb", "both"]:
            size = self.rgb_size
            fps = max(1, min(self.rgb_fps_est, 60))
            rgb_path = os.path.join(self.log_dir, f"rgb_video_{timestamp}.avi")
            self.video_writer_rgb = cv2.VideoWriter(rgb_path, fourcc, fps, size)
            write_log(f"开始录像 | 类型: RGB | 尺寸:{size} | FPS:{fps:.1f} | 文件: {rgb_path}", self.log_file)

        if camera_type in ["depth", "both"]:
            size = self.depth_size
            fps = max(1, min(self.depth_fps_est, 60))
            depth_path = os.path.join(self.log_dir, f"depth_video_{timestamp}.avi")
            self.video_writer_depth = cv2.VideoWriter(depth_path, fourcc, fps, size)
            write_log(f"开始录像 | 类型: Depth | 尺寸:{size} | FPS:{fps:.1f} | 文件: {depth_path}", self.log_file)

        self.record_thread = threading.Thread(target=self._record_loop, daemon=True)
        self.record_thread.start()

    # ========= 停止录像 =========
    def stop_record(self):
        if not self.recording:
            print("⚠️ 当前没有录像任务。")
            return
        self.recording = False
        if self.video_writer_rgb:
            self.video_writer_rgb.release()
        if self.video_writer_depth:
            self.video_writer_depth.release()
        write_log("录像停止", self.log_file)
        print("🟥 录像停止。")

    # ========= 内部录像循环 =========
    def _record_loop(self):
        start_time = datetime.datetime.now()
        last_print_time = start_time
        while self.recording:
            if self.camera_type in ["rgb", "both"] and self.rgb_frame is not None:
                frame = self._apply_denoise(self.rgb_frame, self.noise_level)
                self.video_writer_rgb.write(frame)

            if self.camera_type in ["depth", "both"] and self.depth_frame is not None:
                frame = self._apply_denoise(self.depth_frame, self.noise_level)
                frame_bgr = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
                self.video_writer_depth.write(frame_bgr)

            if self.duration:
                now = datetime.datetime.now()
                elapsed = (now - start_time).total_seconds()
                if (now - last_print_time).total_seconds() >= 1:
                    remaining = max(0, int(self.duration - elapsed))
                    print(f"⏱ 剩余录制时间: {remaining} 秒")
                    last_print_time = now
                if elapsed >= self.duration:
                    print("⏱ 达到时长限制，自动停止录像。")
                    self.stop_record()
                    break
            cv2.waitKey(1)

    # ========= 降噪函数 =========
    def _apply_denoise(self, img, level):
        if img.ndim == 3 and img.shape[2] == 3:
            if level == "light":
                return cv2.fastNlMeansDenoisingColored(img, None, 5, 5, 7, 15)
            elif level == "medium":
                return cv2.GaussianBlur(img, (5, 5), 0)
            else:
                return img
        else:
            if level in ["light", "medium"]:
                return cv2.fastNlMeansDenoising(img, None, 10, 7, 21)
            else:
                return img


# ========= 主函数 =========
def main():
    rclpy.init()
    node = RealSenseRecorder()
    threading.Thread(target=rclpy.spin, args=(node,), daemon=True).start()

    try:
        while True:
            print("\n🎛 1) 拍照  2) 录像  3) 停止录像  4) 退出")
            op = input("选择操作: ").strip()

            if op == "1":
                camera_type = input("相机类型(rgb/depth/both): ").strip().lower()
                if camera_type == "rgb":
                    noise = "none"
                else:
                    noise = input("降噪等级(no/light/medium): ").strip().lower()
                    noise = "none" if noise in ["", "no"] else noise
                node.take_photo(camera_type, noise)

            elif op == "2":
                camera_type = input("相机类型(rgb/depth/both): ").strip().lower()
                if camera_type == "rgb":
                    noise = "none"
                else:
                    noise = input("降噪等级(no/light/medium): ").strip().lower()
                    noise = "none" if noise in ["", "no"] else noise
                duration = input("录制时长限制(秒, 回车跳过): ").strip()
                duration = float(duration) if duration else None
                node.start_record(camera_type, noise, duration)

            elif op == "3":
                node.stop_record()

            elif op == "4":
                print("👋 退出程序。")
                break

            else:
                print("⚠️ 无效输入，请重新选择。")

    except KeyboardInterrupt:
        print("\n🟥 用户中断。")
    finally:
        node.stop_record()
        node.destroy_node()
        rclpy.shutdown()
        write_log("录制控制节点关闭", node.log_file)
        print("[INFO] 程序结束。")


if __name__ == "__main__":
    main()
