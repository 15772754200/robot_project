#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RealSense 工具箱通用错误与日志模块。
所有脚本统一使用该模块输出“控制台提示 + 日志写入”。
"""

from __future__ import annotations

import datetime
import os
import shutil
from pathlib import Path
from typing import Optional

RUN_LOG_ROOT = Path("/home/niic/robot_ros2_deploy_4_10_humanoid/run_logs/realsense")
SESSION_PREFIX = "realsense_start_"

ERROR_DEFINITIONS = {
    "E001": ("ROS2 环境未加载", "无法找到 ros2 命令；请先 source ROS2 环境。"),
    "E002": ("相机节点不存在", "/camera/camera 未出现；请启动相机节点后返回菜单重试。"),
    "E003": ("输入非法", "布尔值、整型或浮点型格式不合法；已拒绝本次设置。"),
    "E004": ("参数设置失败", "参数超出驱动允许范围或驱动拒绝设置；请查看驱动返回错误。"),
    "E005": ("图像订阅超时", "在规定时间内未接收到 RGB/Depth 图像；已终止当前采集操作。"),
    "E006": ("视频写入初始化失败", "图像尺寸或帧率未准备完成，或 VideoWriter 打开失败；不创建视频文件。"),
    "E007": ("日志目录不存在", "运行日志目录不存在或不可访问；请检查 run_logs/realsense 目录权限。"),
    "E008": ("日志文件未找到", "未找到 realsense_start_* 启动日志；已输出告警。"),
    "E009": ("未检测到硬件设备", "pyrealsense2 未发现 RealSense D435i；请重新连接设备。"),
    "E010": ("取流测试失败", "设备存在但不能正常输出图像帧；已输出异常信息。"),
    "E999": ("未知错误", "程序发生未分类异常。"),
}


def now_str() -> str:
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def now_file_str() -> str:
    return datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")


def run_log_root() -> Path:
    return RUN_LOG_ROOT


def ensure_log_dir(create_if_missing: bool = True) -> Optional[Path]:
    """返回统一运行日志根目录。不可访问时返回 None。"""
    d = run_log_root()
    try:
        if not d.exists():
            if create_if_missing:
                d.mkdir(parents=True, exist_ok=True)
            else:
                return None
        if not d.is_dir() or not os.access(d, os.W_OK):
            return None
        return d
    except Exception:
        return None


def session_dir_name(timestamp: Optional[str] = None) -> str:
    return f"{SESSION_PREFIX}{timestamp or now_file_str()}"


def ensure_session_dir(timestamp: Optional[str] = None, create_if_missing: bool = True) -> Optional[Path]:
    root = ensure_log_dir(create_if_missing=create_if_missing)
    if root is None:
        return None
    session_dir = root / session_dir_name(timestamp)
    try:
        if not session_dir.exists():
            if create_if_missing:
                session_dir.mkdir(parents=True, exist_ok=True)
            else:
                return None
        if not session_dir.is_dir() or not os.access(session_dir, os.W_OK):
            return None
        return session_dir
    except Exception:
        return None


def find_latest_session_dir() -> Optional[Path]:
    root = ensure_log_dir(create_if_missing=False)
    if root is None or not root.exists():
        return None
    candidates = [p for p in root.iterdir() if p.is_dir() and p.name.startswith(SESSION_PREFIX)]
    if not candidates:
        return None
    candidates.sort(key=lambda p: p.name, reverse=True)
    return candidates[0]


def latest_or_new_session_dir() -> Optional[Path]:
    latest = find_latest_session_dir()
    if latest is not None:
        return latest
    return ensure_session_dir()


def startup_log_file(session_dir: Optional[os.PathLike | str] = None, timestamp: Optional[str] = None) -> Optional[Path]:
    if session_dir is None:
        session_dir = ensure_session_dir(timestamp=timestamp)
    if session_dir is None:
        return None
    directory = Path(session_dir)
    ts = timestamp or directory.name.removeprefix(SESSION_PREFIX)
    return directory / f"{SESSION_PREFIX}{ts}.log"


def default_log_file(prefix: str = "realsense_toolbox", session_dir: Optional[os.PathLike | str] = None) -> Optional[Path]:
    d = Path(session_dir) if session_dir is not None else latest_or_new_session_dir()
    if d is None:
        return None
    return d / f"{prefix}.log"


def write_log(message: str, log_file: Optional[os.PathLike | str] = None, *, level: str = "INFO") -> None:
    """写入日志；如果日志目录不可用，只在终端提示，不抛异常。"""
    path: Optional[Path]
    if log_file:
        path = Path(log_file)
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
        except Exception:
            path = None
    else:
        path = default_log_file()

    line = f"[{now_str()}] [{level}] {message}\n"
    if path is None:
        print(f"⚠️ 日志不可写，仅终端输出: {message}")
        return
    try:
        with open(path, "a", encoding="utf-8") as f:
            f.write(line)
    except Exception as e:
        print(f"⚠️ 日志写入失败: {path} | {e}")


def emit_error(code: str, *, detail: str = "", log_file: Optional[os.PathLike | str] = None) -> str:
    title, msg = ERROR_DEFINITIONS.get(code, ERROR_DEFINITIONS["E999"])
    text = f"[{code}] {title}：{msg}"
    if detail:
        text += f" | 详细信息: {detail}"
    print(f"❌ {text}")
    write_log(text, log_file, level="ERROR")
    return text


def emit_warn(message: str, *, log_file: Optional[os.PathLike | str] = None) -> None:
    print(f"⚠️ {message}")
    write_log(message, log_file, level="WARN")


def emit_info(message: str, *, log_file: Optional[os.PathLike | str] = None) -> None:
    print(f"✅ {message}")
    write_log(message, log_file, level="INFO")


def check_ros2_command(*, log_file: Optional[os.PathLike | str] = None) -> bool:
    """检查 ros2 命令是否存在。"""
    if shutil.which("ros2") is None:
        emit_error("E001", detail="终端中 command -v ros2 失败。示例：source /opt/ros/<distro>/setup.bash", log_file=log_file)
        return False
    return True
