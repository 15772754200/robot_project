#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RealSense 图像拍照/录像工具：带画面预览增强版。

增强内容：
1. 拍照成功后，终端打印 RGB / Depth 图片保存路径；
2. 拍照成功后，写入 realsense_start_*.log；
3. 拍照成功后，弹出图片预览窗口；
4. 录像开始时，终端打印 RGB / Depth 视频保存路径；
5. 录像开始与结束都写入日志；
6. 录像过程中实时显示 RGB / Depth 画面；
7. 录像达到时长限制自动停止时，也会打印并写入“录像保存完成”；
8. 如果当前是 SSH / 无图形界面环境，则自动关闭窗口预览，但仍正常保存文件和日志。

错误码沿用：
E001 ROS2 环境未加载；
E003 输入非法；
E005 图像订阅超时；
E006 视频写入初始化失败；
E007 日志目录异常；
E999 未知错误。
"""

import datetime
import os
import sys
import threading
import time
from pathlib import Path

from realsense_error_utils import (
    check_ros2_command,
    default_log_file,
    emit_error,
    emit_info,
    emit_warn,
    ensure_log_dir,
    find_latest_session_dir,
    latest_or_new_session_dir,
    startup_log_file,
    write_log,
)

LOG_FILE = default_log_file("realsense_record")

# ROS2 Python 环境检查：未 source 时，常见表现是 ros2 命令或 rclpy 找不到。
if not check_ros2_command(log_file=LOG_FILE):
    sys.exit(1)

try:
    import rclpy
    from rclpy.node import Node
    from sensor_msgs.msg import Image
    from cv_bridge import CvBridge
    import cv2
except Exception as e:
    emit_error("E001", detail=f"导入 ROS2/cv_bridge/OpenCV 模块失败: {e}", log_file=LOG_FILE)
    sys.exit(1)

CAMERA_TOPICS = {
    "rgb": "/camera/color/image_raw",
    "depth": "/camera/depth/image_rect_raw",
}
VALID_CAMERA_TYPES = {"rgb", "depth", "both"}
VALID_NOISE_LEVELS = {"none", "light", "medium"}
FRAME_TIMEOUT_SEC = 5.0


class RealSenseRecorder(Node):
    def __init__(self):
        super().__init__("realsense_recorder")
        self.bridge = CvBridge()

        self.rgb_frame = None
        self.depth_frame = None
        self.recording = False
        self.video_writer_rgb = None
        self.video_writer_depth = None
        self.record_thread = None
        self.record_session_id = 0
        self.record_done_event = threading.Event()
        self.record_done_event.set()

        self.rgb_size = None
        self.depth_size = None
        self.rgb_last_time = None
        self.depth_last_time = None
        self.rgb_fps_est = 30.0
        self.depth_fps_est = 30.0

        self.camera_type = "both"
        self.noise_level = "none"
        self.duration = None

        self.rgb_video_path = None
        self.depth_video_path = None
        self.record_start_time = None

        self.preview_enabled = self._detect_preview_available()
        self.preview_failed = False

        self.log_dir = self._prepare_log_dir()
        self.log_file = self._get_latest_log()
        write_log("RealSense 录制节点启动", self.log_file)

        self.create_subscription(Image, CAMERA_TOPICS["rgb"], self.rgb_callback, 10)
        self.create_subscription(Image, CAMERA_TOPICS["depth"], self.depth_callback, 10)
        emit_info(f"订阅图像话题成功: {CAMERA_TOPICS['rgb']} | {CAMERA_TOPICS['depth']}", log_file=self.log_file)

        if self.preview_enabled:
            emit_info("检测到图形环境，已启用 OpenCV 画面预览。", log_file=self.log_file)
        else:
            emit_warn("当前环境未检测到 DISPLAY/WAYLAND_DISPLAY，已关闭画面预览；文件仍会正常保存。", log_file=self.log_file)

    def _reset_record_state(self):
        self.recording = False
        self.record_thread = None
        self.video_writer_rgb = None
        self.video_writer_depth = None
        self.rgb_video_path = None
        self.depth_video_path = None
        self.record_start_time = None
        self.duration = None
        self.record_done_event.set()

    def _detect_preview_available(self) -> bool:
        """判断当前环境是否适合弹出 OpenCV 窗口。"""
        if os.name == "nt":
            return True
        return bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))

    def _prepare_log_dir(self) -> Path:
        d = latest_or_new_session_dir()
        if d is None:
            emit_error("E007", detail="无法创建或访问运行日志目录，当前拍照/录像动作无法继续。", log_file=LOG_FILE)
            raise RuntimeError("日志目录不可访问")
        return d

    def _get_latest_log(self) -> Path:
        latest_session = find_latest_session_dir()
        session_dir = latest_session if latest_session is not None else self.log_dir
        log_path = startup_log_file(session_dir=session_dir)
        if log_path is None:
            raise RuntimeError("启动日志路径不可用")
        log_path.touch(exist_ok=True)
        emit_info(f"复用运行目录: {session_dir}", log_file=log_path)
        return log_path

    def rgb_callback(self, msg):
        try:
            self.rgb_frame = self.bridge.imgmsg_to_cv2(msg, desired_encoding="bgr8")
            h, w, _ = self.rgb_frame.shape
            self.rgb_size = (w, h)
            now = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
            if self.rgb_last_time is not None:
                dt = now - self.rgb_last_time
                if 0.001 < dt < 1.0:
                    self.rgb_fps_est = 1.0 / dt
            self.rgb_last_time = now
        except Exception as e:
            emit_error("E999", detail=f"RGB 回调转换失败: {e}", log_file=self.log_file)

    def depth_callback(self, msg):
        try:
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
        except Exception as e:
            emit_error("E999", detail=f"Depth 回调转换失败: {e}", log_file=self.log_file)

    def _ready_for(self, camera_type: str) -> bool:
        if camera_type == "rgb":
            return self.rgb_frame is not None and self.rgb_size is not None
        if camera_type == "depth":
            return self.depth_frame is not None and self.depth_size is not None
        return (
            self.rgb_frame is not None and self.rgb_size is not None and
            self.depth_frame is not None and self.depth_size is not None
        )

    def _wait_for_frames(self, camera_type: str, timeout_sec: float = FRAME_TIMEOUT_SEC) -> bool:
        print("⏳ 等待图像数据中 ...")
        start = time.time()
        while time.time() - start <= timeout_sec:
            if self._ready_for(camera_type):
                return True
            time.sleep(0.05)
        missing = []
        if camera_type in ("rgb", "both") and self.rgb_frame is None:
            missing.append("RGB")
        if camera_type in ("depth", "both") and self.depth_frame is None:
            missing.append("Depth")
        emit_error("E005", detail=f"请求类型: {camera_type} | 未收到: {','.join(missing) if missing else '尺寸/FPS 未准备完成'}", log_file=self.log_file)
        return False

    def _safe_imshow(self, win_name: str, img, wait_ms: int = 1) -> int:
        """安全显示窗口。若环境不支持窗口，则自动关闭预览。"""
        if not self.preview_enabled or self.preview_failed:
            return -1
        try:
            cv2.imshow(win_name, img)
            return cv2.waitKey(wait_ms) & 0xFF
        except Exception as e:
            self.preview_failed = True
            emit_warn(f"OpenCV 预览窗口打开失败，已自动关闭预览: {e}", log_file=self.log_file)
            return -1

    def _safe_destroy_windows(self):
        if not self.preview_enabled or self.preview_failed:
            return
        try:
            cv2.destroyAllWindows()
            cv2.waitKey(1)
        except Exception:
            pass

    def _safe_destroy_window(self, win_name: str):
        if not self.preview_enabled or self.preview_failed:
            return
        try:
            cv2.destroyWindow(win_name)
            cv2.waitKey(1)
        except Exception:
            pass

    def _print_and_log_saved(self, message: str):
        print(message)
        write_log(message, self.log_file)

    def take_photo(self, camera_type="both", noise_level="none"):
        if not self._wait_for_frames(camera_type):
            return

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        saved_paths = []

        if camera_type in ["rgb", "both"] and self.rgb_frame is not None:
            img = self._apply_denoise(self.rgb_frame, noise_level)
            rgb_path = self.log_dir / f"rgb_photo_{timestamp}.jpg"
            ok = cv2.imwrite(str(rgb_path), img)
            if ok:
                saved_paths.append(("RGB", rgb_path))
                self._print_and_log_saved(f"📷 拍照保存完成 | 类型: RGB | 文件: {rgb_path}")
            else:
                emit_error("E999", detail=f"RGB 图片写入失败: {rgb_path}", log_file=self.log_file)

        if camera_type in ["depth", "both"] and self.depth_frame is not None:
            img = self._apply_denoise(self.depth_frame, noise_level)
            depth_path = self.log_dir / f"depth_photo_{timestamp}.png"
            ok = cv2.imwrite(str(depth_path), img)
            if ok:
                saved_paths.append(("Depth", depth_path))
                self._print_and_log_saved(f"📷 拍照保存完成 | 类型: Depth | 文件: {depth_path}")
            else:
                emit_error("E999", detail=f"Depth 图片写入失败: {depth_path}", log_file=self.log_file)

        if saved_paths:
            print("\n✅ 本次拍照结果：")
            write_log("本次拍照结果如下：", self.log_file)
            for name, path in saved_paths:
                line = f"   - {name}: {path}"
                print(line)
                write_log(line, self.log_file)

    def _open_writer(self, path: Path, fps: float, size, is_depth: bool = False):
        if size is None or not isinstance(size, tuple) or len(size) != 2:
            emit_error("E006", detail=f"尺寸未准备完成: path={path}, size={size}", log_file=self.log_file)
            return None
        if fps is None or fps <= 0:
            emit_error("E006", detail=f"FPS 未准备完成: path={path}, fps={fps}", log_file=self.log_file)
            return None

        fourcc = cv2.VideoWriter_fourcc(*"XVID")
        writer = cv2.VideoWriter(str(path), fourcc, float(fps), size)
        if not writer.isOpened():
            emit_error("E006", detail=f"VideoWriter 打开失败: path={path}, fps={fps}, size={size}, is_depth={is_depth}", log_file=self.log_file)
            return None
        return writer

    def start_record(self, camera_type="both", noise_level="none", duration=None):
        if self.recording:
            emit_warn("已经在录像中，请先停止。", log_file=self.log_file)
            return
        self._reset_record_state()
        if not self._wait_for_frames(camera_type):
            return

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.camera_type = camera_type
        self.noise_level = noise_level
        self.duration = duration
        self.rgb_video_path = None
        self.depth_video_path = None
        self.record_start_time = datetime.datetime.now()
        self.record_session_id += 1
        session_id = self.record_session_id
        self.record_done_event.clear()

        if camera_type in ["rgb", "both"]:
            rgb_path = self.log_dir / f"rgb_video_{timestamp}.avi"
            fps = max(1.0, min(float(self.rgb_fps_est), 60.0))
            self.video_writer_rgb = self._open_writer(rgb_path, fps, self.rgb_size)
            if self.video_writer_rgb is None:
                self._release_writers()
                return
            self.rgb_video_path = rgb_path
            msg = f"🎥 开始录像 | 类型: RGB | 尺寸:{self.rgb_size} | FPS:{fps:.1f} | 文件: {rgb_path}"
            self._print_and_log_saved(msg)

        if camera_type in ["depth", "both"]:
            depth_path = self.log_dir / f"depth_video_{timestamp}.avi"
            fps = max(1.0, min(float(self.depth_fps_est), 60.0))
            self.video_writer_depth = self._open_writer(depth_path, fps, self.depth_size, is_depth=True)
            if self.video_writer_depth is None:
                self._release_writers()
                return
            self.depth_video_path = depth_path
            msg = f"🎥 开始录像 | 类型: Depth | 尺寸:{self.depth_size} | FPS:{fps:.1f} | 文件: {depth_path}"
            self._print_and_log_saved(msg)

        self.recording = True
        self.record_thread = threading.Thread(target=self._record_loop, args=(session_id, duration), daemon=True)
        self.record_thread.start()

        emit_info("录像线程已启动。", log_file=self.log_file)
        print("🖼 录像过程中将实时显示画面；在预览窗口中按 q 可停止录像。")
        write_log("录像过程中启用实时预览；在预览窗口中按 q 可停止录像。", self.log_file)

        if duration is not None and self.record_thread is not None:
            deadline = time.monotonic() + duration
            last_remaining = None
            while self.recording and self.record_session_id == session_id:
                remaining = max(0, int(deadline - time.monotonic()))
                if remaining != last_remaining:
                    print(f"⏱ 剩余录制时间: {remaining} 秒")
                    write_log(f"录像倒计时 | 剩余: {remaining} 秒", self.log_file)
                    last_remaining = remaining
                if time.monotonic() >= deadline:
                    print("⏱ 达到时长限制，自动停止录像。")
                    write_log("达到时长限制，自动停止录像。", self.log_file)
                    self.stop_record(session_id)
                    break
                if self.record_done_event.wait(timeout=0.1):
                    break
            self.record_done_event.wait(timeout=2.0)

    def _release_writers(self, session_id=None):
        if session_id is not None and session_id != self.record_session_id:
            return
        if self.video_writer_rgb:
            self.video_writer_rgb.release()
            self.video_writer_rgb = None
        if self.video_writer_depth:
            self.video_writer_depth.release()
            self.video_writer_depth = None

    def stop_record(self, session_id=None):
        if session_id is not None and session_id != self.record_session_id:
            return
        if not self.recording:
            emit_warn("当前没有录像任务。", log_file=self.log_file)
            return

        self.recording = False
        self._release_writers(session_id)
        self.record_thread = None
        self._safe_destroy_window("RGB Recording Preview")
        self._safe_destroy_window("Depth Recording Preview")

        elapsed = None
        if self.record_start_time is not None:
            elapsed = (datetime.datetime.now() - self.record_start_time).total_seconds()

        write_log("录像停止", self.log_file)
        print("🟥 录像停止。")

        print("\n✅ 本次录像保存结果：")
        write_log("本次录像保存结果如下：", self.log_file)

        if elapsed is not None:
            line = f"   - 实际录制时长: {elapsed:.2f} 秒"
            print(line)
            write_log(line, self.log_file)

        if self.rgb_video_path is not None:
            line = f"   - RGB 视频: {self.rgb_video_path}"
            print(line)
            write_log(line, self.log_file)

        if self.depth_video_path is not None:
            line = f"   - Depth 视频: {self.depth_video_path}"
            print(line)
            write_log(line, self.log_file)

        if self.rgb_video_path is None and self.depth_video_path is None:
            line = "   - 未生成视频文件。"
            print(line)
            write_log(line, self.log_file, level="WARN")
        self._reset_record_state()

    def _record_loop(self, session_id, duration):
        start_time = datetime.datetime.now()
        last_print_time = start_time

        while self.recording and self.record_session_id == session_id:
            try:
                key = -1

                if self.camera_type in ["rgb", "both"] and self.rgb_frame is not None:
                    frame = self._apply_denoise(self.rgb_frame, self.noise_level)
                    if self.video_writer_rgb:
                        self.video_writer_rgb.write(frame)
                    key = self._safe_imshow("RGB Recording Preview", frame, wait_ms=1)

                if self.camera_type in ["depth", "both"] and self.depth_frame is not None:
                    frame = self._apply_denoise(self.depth_frame, self.noise_level)
                    frame_bgr = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
                    if self.video_writer_depth:
                        self.video_writer_depth.write(frame_bgr)
                    key2 = self._safe_imshow("Depth Recording Preview", frame, wait_ms=1)
                    if key2 != -1:
                        key = key2

                if key == ord("q"):
                    print("⌨️ 检测到 q，停止录像。")
                    write_log("用户在预览窗口按 q 停止录像。", self.log_file, level="WARN")
                    self.stop_record(session_id)
                    break

                time.sleep(0.001)
            except Exception as e:
                emit_error("E999", detail=f"录像循环异常: {e}", log_file=self.log_file)
                self.stop_record(session_id)
                break

        self._safe_destroy_window("RGB Recording Preview")
        self._safe_destroy_window("Depth Recording Preview")

    def _apply_denoise(self, img, level):
        if level not in VALID_NOISE_LEVELS:
            level = "none"
        if img.ndim == 3 and img.shape[2] == 3:
            if level == "light":
                return cv2.fastNlMeansDenoisingColored(img, None, 5, 5, 7, 15)
            if level == "medium":
                return cv2.GaussianBlur(img, (5, 5), 0)
            return img
        if level in ["light", "medium"]:
            return cv2.fastNlMeansDenoising(img, None, 10, 7, 21)
        return img


def ask_camera_type() -> str | None:
    camera_type = input("相机类型(rgb/depth/both): ").strip().lower()
    if camera_type not in VALID_CAMERA_TYPES:
        emit_error("E003", detail=f"相机类型非法: {camera_type}，只能输入 rgb/depth/both", log_file=LOG_FILE)
        return None
    return camera_type


def ask_noise_level(camera_type: str) -> str | None:
    if camera_type == "rgb":
        return "none"
    noise = input("降噪等级(no/light/medium): ").strip().lower()
    noise = "none" if noise in ["", "no"] else noise
    if noise not in VALID_NOISE_LEVELS:
        emit_error("E003", detail=f"降噪等级非法: {noise}，只能输入 no/light/medium", log_file=LOG_FILE)
        return None
    return noise


def ask_duration():
    duration_raw = input("录制时长限制(秒, 回车跳过): ").strip()
    if not duration_raw:
        return None
    try:
        duration = float(duration_raw)
        if duration <= 0:
            raise ValueError("录制时长必须大于 0")
        return duration
    except Exception as e:
        emit_error("E003", detail=f"录制时长非法: {duration_raw} | {e}", log_file=LOG_FILE)
        return "INVALID"


def main():
    write_log("record.py 启动", LOG_FILE)
    node = None
    spin_thread = None
    try:
        rclpy.init()
        node = RealSenseRecorder()
        spin_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
        spin_thread.start()

        while True:
            print("\n🎛 1) 拍照  2) 录像  3) 停止录像  4) 退出")
            op = input("选择操作: ").strip()

            if op == "1":
                camera_type = ask_camera_type()
                if camera_type is None:
                    continue
                noise = ask_noise_level(camera_type)
                if noise is None:
                    continue
                node.take_photo(camera_type, noise)

            elif op == "2":
                camera_type = ask_camera_type()
                if camera_type is None:
                    continue
                noise = ask_noise_level(camera_type)
                if noise is None:
                    continue
                duration = ask_duration()
                if duration == "INVALID":
                    continue
                node.start_record(camera_type, noise, duration)

            elif op == "3":
                node.stop_record()

            elif op == "4":
                print("👋 退出程序。")
                return 0

            else:
                emit_error("E003", detail=f"主菜单选项非法: {op}", log_file=LOG_FILE)

    except KeyboardInterrupt:
        print("\n🟥 用户中断。")
        write_log("用户中断 record.py", LOG_FILE, level="WARN")
        return 0
    except Exception as e:
        emit_error("E999", detail=f"record.py 主流程异常: {e}", log_file=LOG_FILE)
        return 1
    finally:
        if node is not None:
            try:
                if node.recording:
                    node.stop_record()
                write_log("录制控制节点关闭", node.log_file)
                node.destroy_node()
            except Exception as e:
                emit_error("E999", detail=f"关闭节点异常: {e}", log_file=LOG_FILE)
        try:
            if rclpy.ok():
                rclpy.shutdown()
        except Exception:
            pass
        print("[INFO] 程序结束。")
    return 0


if __name__ == "__main__":
    exit_code = main()
    try:
        sys.stdout.flush()
        sys.stderr.flush()
    except Exception:
        pass
    os._exit(exit_code)
