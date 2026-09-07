#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import time
import glob
import json
import logging
import subprocess
import re
import select
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import cv2


# ----------------------------
# helpers
# ----------------------------
def now_str(fmt: str = "%Y%m%d_%H%M%S") -> str:
    return time.strftime(fmt)


def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)


def prompt_str(msg: str, default: Optional[str] = None) -> str:
    if default is None:
        return input(msg).strip()
    s = input(f"{msg} (默认: {default}) ").strip()
    return s if s else default


def prompt_int(msg: str, default: int) -> int:
    s = input(f"{msg} (默认: {default}) ").strip()
    if not s:
        return default
    try:
        return int(s)
    except ValueError:
        print("输入不是整数，使用默认值。")
        return default


def prompt_float(msg: str, default: float) -> float:
    s = input(f"{msg} (默认: {default}) ").strip()
    if not s:
        return default
    try:
        return float(s)
    except ValueError:
        print("输入不是数字，使用默认值。")
        return default


def read_terminal_cmd_nonblocking() -> Optional[str]:
    """
    终端输入控制：需要按回车才会触发。
    无输入返回 None。
    """
    try:
        r, _, _ = select.select([sys.stdin], [], [], 0)
        if r:
            cmd = sys.stdin.readline()
            if cmd:
                return cmd.strip()
    except Exception:
        return None
    return None


def list_video_devices() -> List[str]:
    devs = sorted(glob.glob("/dev/video*"))
    out = []
    for d in devs:
        base = os.path.basename(d)
        if base.startswith("video") and base[5:].isdigit():
            out.append(d)
    return out


def fourcc_str(v: float) -> str:
    v = int(v)
    return "".join([chr((v >> 8 * i) & 0xFF) for i in range(4)])


def get_actual_props(cap: cv2.VideoCapture) -> dict:
    return {
        "width": int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)),
        "height": int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)),
        "fps": float(cap.get(cv2.CAP_PROP_FPS)),
        "fourcc": fourcc_str(cap.get(cv2.CAP_PROP_FOURCC)),
    }


def run_cmd(args: List[str], timeout_s: int = 8) -> Tuple[int, str]:
    """
    返回 (returncode, stdout+stderr)
    """
    try:
        p = subprocess.run(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout_s,
        )
        return p.returncode, p.stdout
    except Exception as e:
        return 1, f"[run_cmd error] {e}"


def log_multiline(logger: logging.Logger, header: str, text: str) -> None:
    logger.info(header)
    for line in (text or "").splitlines():
        logger.info(line)


# ----------------------------
# v4l2 controls
# ----------------------------
_CTRL_LINE_RE = re.compile(
    r"^\s*([a-zA-Z0-9_]+)\s+0x[0-9a-fA-F]+\s+\(([^)]+)\)\s*:\s*(.*)$"
)


def v4l2_list_ctrls_text(device: str) -> str:
    rc, out = run_cmd(["v4l2-ctl", "-d", device, "--list-ctrls"], timeout_s=8)
    return out if rc == 0 else out


def v4l2_list_ctrls_menus_text(device: str) -> str:
    rc, out = run_cmd(["v4l2-ctl", "-d", device, "--list-ctrls-menus"], timeout_s=8)
    return out if rc == 0 else out


def v4l2_all_text(device: str) -> str:
    rc, out = run_cmd(["v4l2-ctl", "-d", device, "--all"], timeout_s=10)
    return out if rc == 0 else out


def v4l2_get_fmt_text(device: str) -> str:
    rc, out = run_cmd(["v4l2-ctl", "-d", device, "--get-fmt-video"], timeout_s=8)
    return out if rc == 0 else out


def parse_v4l2_ctrls(ctrls_text: str) -> Dict[str, dict]:
    """
    解析 --list-ctrls 输出，返回：
    name -> {type, min, max, step, default, value, flags, value_label}
    """
    infos: Dict[str, dict] = {}
    for line in ctrls_text.splitlines():
        m = _CTRL_LINE_RE.match(line)
        if not m:
            continue
        name = m.group(1)
        ctype = m.group(2).strip()
        rest = m.group(3).strip()

        # 统一解析字段
        info = {"type": ctype, "min": None, "max": None, "step": None, "default": None, "value": None, "flags": "", "value_label": ""}

        # min/max/step/default/value
        for key in ["min", "max", "step", "default", "value"]:
            km = re.search(rf"\b{key}=(-?\d+)", rest)
            if km:
                info[key] = int(km.group(1))

        # value label e.g. value=3 (Aperture Priority Mode)
        vm = re.search(r"\bvalue=(-?\d+)\s*\(([^)]+)\)", rest)
        if vm:
            info["value"] = int(vm.group(1))
            info["value_label"] = vm.group(2).strip()

        # flags=inactive
        fm = re.search(r"\bflags=([a-zA-Z0-9_,]+)", rest)
        if fm:
            info["flags"] = fm.group(1).strip()
        infos[name] = info
    return infos


def parse_v4l2_menus(menus_text: str) -> Dict[str, Dict[int, str]]:
    """
    解析 --list-ctrls-menus 输出
    返回： ctrl_name -> {val: label}
    """
    menus: Dict[str, Dict[int, str]] = {}
    current = None

    # 先定位 "xxx (menu)" 行，再解析后续的 "N: Label"
    for line in menus_text.splitlines():
        m = _CTRL_LINE_RE.match(line)
        if m:
            name = m.group(1)
            ctype = m.group(2).strip()
            current = name if "menu" in ctype else None
            if current and current not in menus:
                menus[current] = {}
            continue

        if current:
            mm = re.match(r"^\s*(\d+)\s*:\s*(.+?)\s*$", line)
            if mm:
                menus[current][int(mm.group(1))] = mm.group(2).strip()
    return menus


def v4l2_set_ctrl(device: str, name: str, value) -> Tuple[bool, str]:
    rc, out = run_cmd(["v4l2-ctl", "-d", device, f"--set-ctrl={name}={value}"], timeout_s=8)
    ok = (rc == 0)
    return ok, out.strip()


def apply_v4l2_controls(device: str, controls: Dict[str, int], logger: logging.Logger) -> Dict[str, int]:
    """
    按合理顺序设置控制项，返回成功设置的 controls
    """
    if not controls:
        return {}

    # 依赖顺序：先切模式，后设值
    order = [
        "auto_exposure",
        "white_balance_automatic",
        "exposure_dynamic_framerate",
        "exposure_time_absolute",
        "white_balance_temperature",
        "power_line_frequency",
        "gain",
        "brightness",
        "contrast",
        "saturation",
        "hue",
        "gamma",
        "sharpness",
        "backlight_compensation",
    ]
    keys = order + [k for k in controls.keys() if k not in order]

    applied: Dict[str, int] = {}
    for k in keys:
        if k not in controls:
            continue
        ok, out = v4l2_set_ctrl(device, k, controls[k])
        if ok:
            logger.info(f"设置控制项成功: {k}={controls[k]}")
            applied[k] = int(controls[k])
        else:
            logger.warning(f"设置控制项失败: {k}={controls[k]} | {out}")
    return applied


def print_and_log_all_params(device: str, logger: logging.Logger) -> None:
    """
    查询当前“所有参数”（控制项+格式+设备信息）并打印 & 写入日志
    """
    all_txt = v4l2_all_text(device)
    fmt_txt = v4l2_get_fmt_text(device)
    ctrls_txt = v4l2_list_ctrls_text(device)

    print("\n========== v4l2-ctl --all ==========")
    print(all_txt)
    print("\n========== v4l2-ctl --get-fmt-video ==========")
    print(fmt_txt)
    print("\n========== v4l2-ctl --list-ctrls ==========")
    print(ctrls_txt)
    print("========== END ==========\n")

    log_multiline(logger, "===== v4l2-ctl --all =====", all_txt)
    log_multiline(logger, "===== v4l2-ctl --get-fmt-video =====", fmt_txt)
    log_multiline(logger, "===== v4l2-ctl --list-ctrls =====", ctrls_txt)


def configure_controls_interactive(device: str, logger: logging.Logger) -> Dict[str, int]:
    """
    在选择分辨率/帧率后，询问是否需要调整曝光/白平衡/增益等参数。
    需要则打印范围并设置；不需要则采用“默认模式”（默认=不改动当前控制值）。
    """
    s = prompt_str("是否需要在本次启动/重开相机时调整曝光/白平衡/增益等参数？(y/n)", "n").lower()
    if s not in ("y", "yes"):
        logger.info("未调整控制参数：使用默认模式（保持当前控制值不变）")
        return {}

    ctrls_text = v4l2_list_ctrls_text(device)
    menus_text = v4l2_list_ctrls_menus_text(device)
    infos = parse_v4l2_ctrls(ctrls_text)
    menus = parse_v4l2_menus(menus_text)

    def show_ctrl(name: str) -> None:
        if name not in infos:
            return
        inf = infos[name]
        rng = f"min={inf['min']} max={inf['max']} step={inf['step']} default={inf['default']} value={inf['value']}"
        extra = f" flags={inf['flags']}" if inf.get("flags") else ""
        label = f" ({inf['value_label']})" if inf.get("value_label") else ""
        print(f"  - {name} [{inf['type']}]: {rng}{label}{extra}")
        if name in menus and menus[name]:
            for k, v in sorted(menus[name].items()):
                print(f"      {k}: {v}")

    print("\n=== 可调整控制项（来自 v4l2-ctl --list-ctrls）===")
    for n in [
        "auto_exposure",
        "exposure_time_absolute",
        "exposure_dynamic_framerate",
        "gain",
        "white_balance_automatic",
        "white_balance_temperature",
        "power_line_frequency",
        "brightness",
        "contrast",
        "saturation",
        "hue",
        "gamma",
        "sharpness",
        "backlight_compensation",
    ]:
        show_ctrl(n)

    controls_to_set: Dict[str, int] = {}

    # 1) Exposure
    if "auto_exposure" in infos:
        cur = infos["auto_exposure"]["value"]
        print("\n[曝光设置]")
        if "auto_exposure" in menus and menus["auto_exposure"]:
            print("auto_exposure 可选：")
            for k, v in sorted(menus["auto_exposure"].items()):
                print(f"  {k}: {v}")
        ae_in = input(f"设置 auto_exposure（回车=保持当前 {cur}）：").strip()
        if ae_in:
            try:
                ae_v = int(ae_in)
                controls_to_set["auto_exposure"] = ae_v
                cur = ae_v
            except ValueError:
                print("输入无效，保持当前 auto_exposure。")

        # exposure time only when manual
        if "exposure_time_absolute" in infos:
            inf = infos["exposure_time_absolute"]
            if cur == 1:
                mn, mx, st, cur_et = inf["min"], inf["max"], inf["step"], inf["value"]
                et_in = input(f"设置 exposure_time_absolute（范围 {mn}~{mx} step={st}，回车=保持 {cur_et}）：").strip()
                if et_in:
                    try:
                        et_v = int(et_in)
                        if mn is not None and mx is not None and (et_v < mn or et_v > mx):
                            print("超出范围，忽略该设置。")
                        else:
                            controls_to_set["exposure_time_absolute"] = et_v
                    except ValueError:
                        print("输入无效，忽略 exposure_time_absolute。")
            else:
                print("提示：当前不是 Manual Mode（auto_exposure!=1），exposure_time_absolute 会处于 inactive。")

    if "exposure_dynamic_framerate" in infos:
        cur = infos["exposure_dynamic_framerate"]["value"]
        df_in = input(f"设置 exposure_dynamic_framerate 0/1（回车=保持 {cur}）：").strip()
        if df_in:
            if df_in in ("0", "1"):
                controls_to_set["exposure_dynamic_framerate"] = int(df_in)
            else:
                print("输入无效，忽略 exposure_dynamic_framerate。")

    # 2) White balance
    if "white_balance_automatic" in infos:
        print("\n[白平衡设置]")
        cur_wba = infos["white_balance_automatic"]["value"]
        wba_in = input(f"设置 white_balance_automatic 0/1（回车=保持 {cur_wba}）：").strip()
        if wba_in in ("0", "1"):
            controls_to_set["white_balance_automatic"] = int(wba_in)
            cur_wba = int(wba_in)
        elif wba_in:
            print("输入无效，保持 white_balance_automatic。")

        if "white_balance_temperature" in infos:
            inf = infos["white_balance_temperature"]
            if cur_wba == 0:
                mn, mx, st, cur_t = inf["min"], inf["max"], inf["step"], inf["value"]
                t_in = input(f"设置 white_balance_temperature（范围 {mn}~{mx} step={st}，回车=保持 {cur_t}）：").strip()
                if t_in:
                    try:
                        t_v = int(t_in)
                        if mn is not None and mx is not None and (t_v < mn or t_v > mx):
                            print("超出范围，忽略该设置。")
                        else:
                            controls_to_set["white_balance_temperature"] = t_v
                    except ValueError:
                        print("输入无效，忽略 white_balance_temperature。")
            else:
                print("提示：white_balance_automatic=1 时，white_balance_temperature 会处于 inactive。")

    # 3) Gain
    if "gain" in infos:
        inf = infos["gain"]
        mn, mx, st, cur_g = inf["min"], inf["max"], inf["step"], inf["value"]
        g_in = input(f"\n设置 gain（范围 {mn}~{mx} step={st}，回车=保持 {cur_g}）：").strip()
        if g_in:
            try:
                g_v = int(g_in)
                if mn is not None and mx is not None and (g_v < mn or g_v > mx):
                    print("超出范围，忽略该设置。")
                else:
                    controls_to_set["gain"] = g_v
            except ValueError:
                print("输入无效，忽略 gain。")

    # 4) Image tuning group
    tune = prompt_str("\n是否要调整亮度/对比度/饱和度/锐度等图像参数？(y/n)", "n").lower()
    if tune in ("y", "yes"):
        for name in [
            "brightness",
            "contrast",
            "saturation",
            "hue",
            "gamma",
            "sharpness",
            "backlight_compensation",
            "power_line_frequency",
        ]:
            if name not in infos:
                continue
            inf = infos[name]
            cur_v = inf["value"]
            mn, mx, st = inf["min"], inf["max"], inf["step"]
            if inf["type"].startswith("menu") and name in menus and menus[name]:
                print(f"\n{name} 可选：")
                for k, v in sorted(menus[name].items()):
                    print(f"  {k}: {v}")
                s_in = input(f"设置 {name}（回车=保持 {cur_v}）：").strip()
                if s_in:
                    try:
                        vv = int(s_in)
                        controls_to_set[name] = vv
                    except ValueError:
                        print("输入无效，忽略。")
            else:
                s_in = input(f"\n设置 {name}（范围 {mn}~{mx} step={st}，回车=保持 {cur_v}）：").strip()
                if s_in:
                    try:
                        vv = int(s_in)
                        if mn is not None and mx is not None and (vv < mn or vv > mx):
                            print("超出范围，忽略。")
                        else:
                            controls_to_set[name] = vv
                    except ValueError:
                        print("输入无效，忽略。")

    # 5) Advanced: freeform
    adv = prompt_str("\n高级：是否额外设置其它控制项（输入 name=value）？(y/n)", "n").lower()
    if adv in ("y", "yes"):
        print("逐行输入 name=value，直接回车结束。例：brightness=10")
        while True:
            line = input("> ").strip()
            if not line:
                break
            if "=" not in line:
                print("格式不对，应为 name=value")
                continue
            n, v = line.split("=", 1)
            n = n.strip()
            v = v.strip()
            if n not in infos:
                print(f"未知控制项：{n}")
                continue
            try:
                controls_to_set[n] = int(float(v))
            except ValueError:
                print("value 不是数字，忽略。")

    if controls_to_set:
        print("\n将要设置的控制项：")
        for k, v in controls_to_set.items():
            print(f"  {k}={v}")
    else:
        print("\n未选择任何控制项修改。")

    return controls_to_set


# ----------------------------
# v4l2 capability parsing
# ----------------------------
def query_v4l2_combos(device: str) -> List[dict]:
    """
    解析 v4l2-ctl --list-formats-ext
    返回：[{pixfmt, w, h, fps:[...]}]
    """
    rc, out = run_cmd(["v4l2-ctl", "-d", device, "--list-formats-ext"], timeout_s=10)
    if rc != 0:
        return []

    combos: List[dict] = []
    current_fmt = None
    current_wh = None

    fmt_re = re.compile(r"\[\d+\]:\s+'([A-Z0-9]+)'")
    size_re = re.compile(r"Size:\s+Discrete\s+(\d+)x(\d+)")
    fps_re = re.compile(r"\(([\d.]+)\s+fps\)")

    fps_map: Dict[Tuple[str, int, int], List[float]] = {}

    for line in out.splitlines():
        line = line.strip()

        m = fmt_re.search(line)
        if m:
            current_fmt = m.group(1)
            current_wh = None
            continue

        m = size_re.search(line)
        if m and current_fmt:
            w = int(m.group(1))
            h = int(m.group(2))
            current_wh = (w, h)
            fps_map.setdefault((current_fmt, w, h), [])
            continue

        if "Interval:" in line and current_fmt and current_wh:
            m = fps_re.search(line)
            if m:
                f = float(m.group(1))
                key = (current_fmt, current_wh[0], current_wh[1])
                if f not in fps_map[key]:
                    fps_map[key].append(f)

    for (pixfmt, w, h), fps_list in fps_map.items():
        combos.append({"pixfmt": pixfmt, "w": w, "h": h, "fps": sorted(fps_list, reverse=True)})

    # 排序：优先 MJPG，其次大分辨率，其次高 fps
    def score(c):
        return (1 if c["pixfmt"] == "MJPG" else 0, c["w"] * c["h"], c["fps"][0] if c["fps"] else 0)

    combos.sort(key=score, reverse=True)
    return combos


def choose_combo(device: str) -> Optional[dict]:
    combos = query_v4l2_combos(device)
    if not combos:
        print("未能读取 v4l2-ctl 能力列表（可能没装 v4l-utils 或该节点不支持枚举）。")
        return None

    print("\n=== 该设备支持的格式/分辨率/帧率组合（v4l2-ctl）===")
    for i, c in enumerate(combos):
        fps_str = ",".join([str(int(x)) if abs(x - int(x)) < 1e-6 else f"{x:.2f}" for x in c["fps"]]) if c["fps"] else "-"
        print(f"  [{i}] {c['pixfmt']:<4}  {c['w']}x{c['h']}   fps: {fps_str}")

    raw = input("请选择：组合编号 [fps]（例如：2 或 2 25）(默认: 0) ").strip()
    if not raw:
        idx = 0
        fps_sel = None
    else:
        parts = raw.split()
        try:
            idx = int(parts[0])
        except ValueError:
            print("组合编号不是整数，默认选择 0")
            idx = 0
        fps_sel = None
        if len(parts) >= 2:
            try:
                fps_sel = float(parts[1])
            except ValueError:
                print("fps 不是数字，将使用该组合的默认最高 fps")
                fps_sel = None

    if idx < 0 or idx >= len(combos):
        print("编号无效，默认选择 0")
        idx = 0

    chosen = combos[idx].copy()
    default_fps = chosen["fps"][0] if chosen["fps"] else 30.0

    if fps_sel is None:
        chosen["fps_selected"] = default_fps
    else:
        ok = any(abs(f - fps_sel) < 0.51 for f in chosen["fps"])
        if ok:
            chosen["fps_selected"] = fps_sel
        else:
            print(f"该组合不支持 fps={fps_sel}，将使用默认 fps={default_fps}")
            chosen["fps_selected"] = default_fps

    return chosen


# ----------------------------
# camera open / test
# ----------------------------
def open_camera(device: str, width: int, height: int, fps: int, fourcc: Optional[str]) -> Tuple[cv2.VideoCapture, dict]:
    cap = cv2.VideoCapture(device, cv2.CAP_V4L2)
    if not cap.isOpened():
        raise RuntimeError(f"无法打开摄像头: {device}")

    cap.set(cv2.CAP_PROP_BUFFERSIZE, 2)

    if fourcc:
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*fourcc))

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, int(width))
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, int(height))
    cap.set(cv2.CAP_PROP_FPS, int(fps))

    actual = get_actual_props(cap)
    return cap, actual


def test_stream(cap: cv2.VideoCapture, test_frames: int = 60, max_seconds: float = 5.0) -> dict:
    ok_cnt = 0
    fail_cnt = 0
    t0 = time.time()
    first_frame_time = None

    while True:
        if time.time() - t0 > max_seconds:
            break
        ok, frame = cap.read()
        if ok and frame is not None and frame.size > 0:
            ok_cnt += 1
            if first_frame_time is None:
                first_frame_time = time.time()
        else:
            fail_cnt += 1
        if ok_cnt >= test_frames:
            break

    duration = max(1e-6, time.time() - t0)
    est_fps = (ok_cnt / duration) if ok_cnt > 0 else 0.0

    return {
        "read_ok_frames": ok_cnt,
        "read_fail_frames": fail_cnt,
        "duration_s": duration,
        "est_fps": est_fps,
        "first_frame_latency_s": (first_frame_time - t0) if first_frame_time else None,
    }


# ----------------------------
# logging/session
# ----------------------------
@dataclass
class SessionInfo:
    session_id: str
    device: str
    start_time: str
    end_time: Optional[str] = None

    operations: List[str] = field(default_factory=list)
    saved_images: List[str] = field(default_factory=list)
    saved_videos: List[str] = field(default_factory=list)

    test_result: Optional[dict] = None

    # 配置历史：每次 open/reopen 记录一条
    config_history: List[dict] = field(default_factory=list)


def setup_logger(log_dir: Path, session_id: str) -> logging.Logger:
    ensure_dir(log_dir)
    log_path = log_dir / f"camera_session_{session_id}.log"

    logger = logging.getLogger(f"cam_session_{session_id}")
    logger.setLevel(logging.INFO)
    logger.handlers.clear()

    fmt = logging.Formatter("[%(asctime)s] %(levelname)s: %(message)s")

    fh = logging.FileHandler(log_path, encoding="utf-8")
    fh.setFormatter(fmt)
    fh.setLevel(logging.INFO)

    sh = logging.StreamHandler(sys.stdout)
    sh.setFormatter(fmt)
    sh.setLevel(logging.INFO)

    logger.addHandler(fh)
    logger.addHandler(sh)
    logger.propagate = False

    logger.info(f"Log file: {log_path}")
    return logger


def safe_release(cap: Optional[cv2.VideoCapture]) -> None:
    try:
        if cap is not None:
            cap.release()
    except Exception:
        pass
    try:
        cv2.destroyAllWindows()
    except Exception:
        pass


def append_config_history(session: SessionInfo, requested: dict, actual: dict, applied_ctrls: Optional[dict] = None) -> None:
    entry = {
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "requested": requested,
        "actual": actual,
    }
    if applied_ctrls is not None:
        entry["applied_ctrls"] = applied_ctrls
    session.config_history.append(entry)


# ----------------------------
# recent log reader
# ----------------------------
def list_log_files(log_dir: Path) -> List[Path]:
    if not log_dir.exists():
        return []
    files = sorted(log_dir.glob("camera_session_*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
    return files


def print_recent_logs(log_dir: Path) -> None:
    files = list_log_files(log_dir)
    if not files:
        print("没有找到日志文件。")
        return

    n = prompt_int("读取最近几个日志？", 3)
    if n <= 0:
        return

    tail_lines = prompt_int("每个日志打印最后多少行？(0=全文)", 120)

    pick = files[: min(n, len(files))]
    print("\n========== 打印最近日志 ==========")
    for f in pick:
        print(f"\n----- {f.name} -----")
        try:
            text = f.read_text(encoding="utf-8", errors="replace")
            if tail_lines > 0:
                lines = text.splitlines()
                text = "\n".join(lines[-tail_lines:])
            print(text)
        except Exception as e:
            print(f"读取失败: {e}")
    print("========== 结束 ==========\n")


# ----------------------------
# modes: photo / video
# ----------------------------
def photo_mode(cap: cv2.VideoCapture, out_dir: Path, logger: logging.Logger, session: SessionInfo) -> str:
    ensure_dir(out_dir)
    logger.info("进入【拍照模式】按键：s=保存  b=返回主菜单  q=退出程序")
    logger.info("提示：窗口按键需要点击窗口获得焦点；终端输入需要回车。")
    session.operations.append("photo_mode")

    cv2.namedWindow("Photo Mode", cv2.WINDOW_NORMAL)

    while True:
        ok, frame = cap.read()
        if not ok or frame is None:
            logger.warning("读取帧失败")
            cmd = read_terminal_cmd_nonblocking()
            if cmd == "q":
                return "quit"
            if cmd == "b":
                return "back"
            cv2.waitKey(30)
            continue

        cv2.imshow("Photo Mode", frame)
        key = cv2.waitKey(1) & 0xFF
        cmd = read_terminal_cmd_nonblocking()

        if key == ord("q") or cmd == "q":
            return "quit"
        if key == ord("b") or cmd == "b":
            return "back"

        if key == ord("s") or cmd == "s":
            fn = out_dir / f"photo_{now_str()}.jpg"
            cv2.imwrite(str(fn), frame)
            logger.info(f"保存图片: {fn}")
            session.saved_images.append(str(fn))


def create_video_writer(out_base: Path, fps: float, size_wh: Tuple[int, int], logger: logging.Logger) -> Tuple[Optional[cv2.VideoWriter], Path]:
    w, h = size_wh

    out_mp4 = out_base.with_suffix(".mp4")
    writer = cv2.VideoWriter(str(out_mp4), cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
    if writer.isOpened():
        return writer, out_mp4

    out_avi = out_base.with_suffix(".avi")
    writer = cv2.VideoWriter(str(out_avi), cv2.VideoWriter_fourcc(*"XVID"), fps, (w, h))
    if writer.isOpened():
        logger.warning("mp4v 打开失败，回退到 AVI/XVID。")
        return writer, out_avi

    return None, out_mp4


def video_mode(cap: cv2.VideoCapture, out_dir: Path, logger: logging.Logger, session: SessionInfo, duration_s: Optional[float]) -> str:
    ensure_dir(out_dir)
    session.operations.append("video_mode")

    # 用当前实际参数
    actual = get_actual_props(cap)
    w = int(actual["width"])
    h = int(actual["height"])
    fps = actual["fps"] if actual["fps"] and actual["fps"] > 1e-3 else 30.0

    cv2.namedWindow("Video Mode", cv2.WINDOW_NORMAL)

    # timed record
    if duration_s is not None and duration_s > 0:
        out_base = out_dir / f"video_{now_str()}"
        writer, real_path = create_video_writer(out_base, float(fps), (w, h), logger)
        if writer is None:
            raise RuntimeError("无法创建 VideoWriter（编码器不可用）")

        logger.info(f"开始录制【定时】{duration_s:.2f}s -> {real_path}")
        session.operations.append(f"record_timed_{duration_s}s")

        t0 = time.time()
        while True:
            ok, frame = cap.read()
            if not ok or frame is None:
                logger.warning("读取帧失败（录制中断）")
                break

            cv2.imshow("Video Mode", frame)
            writer.write(frame)

            key = cv2.waitKey(1) & 0xFF
            cmd = read_terminal_cmd_nonblocking()

            if key == ord("q") or cmd == "q":
                writer.release()
                return "quit"
            if key == ord("b") or cmd == "b":
                writer.release()
                logger.info(f"中途返回主菜单，已保存: {real_path}")
                session.saved_videos.append(str(real_path))
                session.operations.append("record_back_to_menu")
                return "back"

            if time.time() - t0 >= duration_s:
                break

        writer.release()
        logger.info(f"录制完成: {real_path}")
        session.saved_videos.append(str(real_path))
        return "back"

    # manual record
    logger.info("进入【视频模式-手动】按键：r=开始/停止  b=返回主菜单  q=退出程序")
    logger.info("提示：窗口按键需要点击窗口获得焦点；终端输入需要回车。")
    recording = False
    writer = None
    current_path = None

    while True:
        ok, frame = cap.read()
        if not ok or frame is None:
            logger.warning("读取帧失败")
            cmd = read_terminal_cmd_nonblocking()
            if cmd == "q":
                if recording and writer:
                    writer.release()
                return "quit"
            if cmd == "b":
                if recording and writer and current_path:
                    writer.release()
                    session.saved_videos.append(str(current_path))
                return "back"
            cv2.waitKey(30)
            continue

        if recording and writer is not None:
            writer.write(frame)

        cv2.imshow("Video Mode", frame)
        key = cv2.waitKey(1) & 0xFF
        cmd = read_terminal_cmd_nonblocking()

        if key == ord("q") or cmd == "q":
            if recording and writer is not None:
                writer.release()
                if current_path:
                    session.saved_videos.append(str(current_path))
            return "quit"

        if key == ord("b") or cmd == "b":
            if recording and writer is not None:
                writer.release()
                if current_path:
                    session.saved_videos.append(str(current_path))
            return "back"

        if key == ord("r") or cmd == "r":
            recording = not recording
            if recording:
                out_base = out_dir / f"video_{now_str()}"
                writer, real_path = create_video_writer(out_base, float(fps), (w, h), logger)
                if writer is None:
                    logger.error("无法创建 VideoWriter，录制未开始。")
                    recording = False
                    writer = None
                    continue
                current_path = real_path
                logger.info(f"录制开始 -> {current_path}")
                session.operations.append("record_manual_start")
            else:
                if writer is not None:
                    writer.release()
                logger.info(f"录制停止并保存 -> {current_path}")
                session.operations.append("record_manual_stop")
                if current_path:
                    session.saved_videos.append(str(current_path))
                writer = None
                current_path = None


# ----------------------------
# main
# ----------------------------
def choose_device() -> str:
    devs = list_video_devices()
    if not devs:
        raise RuntimeError("未找到 /dev/video* 设备。")
    print("检测到视频设备：")
    for i, d in enumerate(devs):
        print(f"  [{i}] {d}")
    idx = prompt_int("选择设备编号", 0)
    if idx < 0 or idx >= len(devs):
        raise RuntimeError("设备编号无效。")
    return devs[idx]


def combo_to_requested(combo: dict) -> Tuple[dict, Optional[str]]:
    """
    将 combo 转成 requested 参数 + fourcc
    """
    width = int(combo["w"])
    height = int(combo["h"])
    fps = int(round(float(combo.get("fps_selected", combo["fps"][0] if combo["fps"] else 30.0))))

    pixfmt = combo["pixfmt"]
    # 常用：MJPG / YUYV
    fourcc = pixfmt if pixfmt in ("MJPG", "YUYV") else None

    requested = {"width": width, "height": height, "fps": fps, "pixfmt": pixfmt}
    return requested, fourcc


def open_and_maybe_apply_ctrls(
    device: str,
    requested: dict,
    fourcc: Optional[str],
    wanted_ctrls: Dict[str, int],
    logger: logging.Logger,
    session: SessionInfo,
) -> Tuple[cv2.VideoCapture, dict, Dict[str, int]]:
    cap, actual = open_camera(device, requested["width"], requested["height"], requested["fps"], fourcc=fourcc)
    logger.info(f"实际参数: {actual}")

    applied_ctrls: Dict[str, int] = {}
    if wanted_ctrls:
        logger.info("开始应用 v4l2 控制参数（曝光/白平衡/增益等）")
        applied_ctrls = apply_v4l2_controls(device, wanted_ctrls, logger)
        session.operations.append("apply_v4l2_controls")
        # 记录当前参数快照
        try:
            print_and_log_all_params(device, logger)
        except Exception as e:
            logger.warning(f"查询参数失败: {e}")
    else:
        # 也记录一次快照，便于你知道默认值是什么
        try:
            log_multiline(logger, "===== v4l2-ctl --list-ctrls (snapshot) =====", v4l2_list_ctrls_text(device))
        except Exception:
            pass

    append_config_history(session, requested=requested, actual=actual, applied_ctrls=applied_ctrls or None)
    return cap, actual, applied_ctrls


def reopen_camera_flow(
    current_device: str,
    cap: Optional[cv2.VideoCapture],
    logger: logging.Logger,
    session: SessionInfo,
) -> Tuple[cv2.VideoCapture, dict, dict, Optional[str], Dict[str, int]]:
    """
    重新选择分辨率/帧率并重开相机
    返回：cap, actual, requested, fourcc, applied_ctrls
    """
    logger.info("执行：重新配置相机（选择分辨率/帧率并重开）")
    session.operations.append("reconfigure_camera")

    # close current cap first
    safe_release(cap)

    combo = choose_combo(current_device)
    if combo is None:
        raise RuntimeError("无法读取能力列表，无法重新配置。")

    requested, fourcc = combo_to_requested(combo)
    logger.info(f"重新配置 requested: {requested}, fourcc={fourcc}")

    wanted_ctrls = configure_controls_interactive(current_device, logger)
    applied_ctrls: Dict[str, int] = {}
    cap, actual, applied_ctrls = open_and_maybe_apply_ctrls(
        current_device, requested, fourcc, wanted_ctrls, logger, session
    )
    return cap, actual, requested, fourcc, applied_ctrls


def main():
    print("=== Camera Controller (OpenCV + V4L2) ===")

    LOG_DIR = Path("./camera_logs")
    OUT_BASE = Path("./camera_outputs")

    device = choose_device()

    combo = choose_combo(device)
    if combo is None:
        # fallback manual
        width = prompt_int("输入分辨率宽度 width", 1920)
        height = prompt_int("输入分辨率高度 height", 1080)
        fps = prompt_int("输入帧率 fps", 30)
        pixfmt = prompt_str("输入像素格式(MJPG/YUYV)，留空=不强制", "MJPG").upper()
        fourcc = pixfmt if pixfmt in ("MJPG", "YUYV") else None
        requested = {"width": width, "height": height, "fps": fps, "pixfmt": pixfmt}
    else:
        requested, fourcc = combo_to_requested(combo)

    session_id = now_str()
    out_root = OUT_BASE / f"session_{session_id}"
    ensure_dir(out_root)

    logger = setup_logger(LOG_DIR, session_id)
    session = SessionInfo(
        session_id=session_id,
        device=device,
        start_time=time.strftime("%Y-%m-%d %H:%M:%S"),
    )

    cap = None
    actual = None
    applied_ctrls: Dict[str, int] = {}
    try:
        logger.info(f"准备打开摄像头: {device}")
        logger.info(f"请求参数: {requested}, fourcc={fourcc}")

        # ★ 新增：在启动/重开时决定是否需要调整控制项，并显示范围
        wanted_ctrls = configure_controls_interactive(device, logger)

        cap, actual, applied_ctrls = open_and_maybe_apply_ctrls(
            device, requested, fourcc, wanted_ctrls, logger, session
        )

        # 保存 meta
        (out_root / "session_meta.json").write_text(
            json.dumps(
                {
                    "session_id": session_id,
                    "device": device,
                    "start_time": session.start_time,
                    "config_history": session.config_history,
                },
                ensure_ascii=False,
                indent=2,
            ),
            encoding="utf-8",
        )

        # 主循环（模式退出回主菜单）
        while True:
            print("\n--- 主菜单 ---")
            print("1) 检测相机流是否正常（连续读帧/估计fps）")
            print("2) 拍照模式（s 保存，b 返回菜单，q 退出程序）")
            print("3) 视频模式（可定时 or 手动 r 开始/停止；b 返回菜单，q 退出程序）")
            print("4) 读取最近几个日志并打印")
            print("5) 检测/打印当前相机实际分辨率与参数（OpenCV cap.get：width/height/fps/fourcc）")
            print("6) 重新选择分辨率/帧率并重开相机（可选择是否设置曝光等控制项）")
            print("7) 查询并记录当前相机“所有参数”（v4l2-ctl --all/--get-fmt-video/--list-ctrls）")
            print("q) 退出并关闭相机")

            choice = prompt_str("选择", "1").lower()

            if choice == "1":
                logger.info("执行：相机流检测")
                session.operations.append("stream_test")
                result = test_stream(cap, test_frames=60, max_seconds=5.0)
                session.test_result = result

                print("\n[检测结果]")
                print(f"  读帧成功: {result['read_ok_frames']}")
                print(f"  读帧失败: {result['read_fail_frames']}")
                print(f"  用时(s):  {result['duration_s']:.3f}")
                print(f"  估计fps:  {result['est_fps']:.2f}")
                print(f"  首帧延迟(s): {result['first_frame_latency_s']}")
                logger.info(f"检测结果: {result}")

            elif choice == "2":
                ret = photo_mode(cap, out_root / "photos", logger, session)
                if ret == "quit":
                    break

            elif choice == "3":
                s = prompt_str("输入录制时长(秒)，直接回车=手动按键录制", "")
                duration = None
                if s.strip():
                    try:
                        duration = float(s.strip())
                    except ValueError:
                        print("输入不是数字，进入手动录制。")
                        duration = None
                ret = video_mode(cap, out_root / "videos", logger, session, duration)
                if ret == "quit":
                    break

            elif choice == "4":
                session.operations.append("print_recent_logs")
                print_recent_logs(LOG_DIR)

            elif choice == "5":
                session.operations.append("print_current_props")
                cur = get_actual_props(cap)
                print("\n[当前相机实际参数（OpenCV cap.get）]")
                print(f"  width : {cur['width']}")
                print(f"  height: {cur['height']}")
                print(f"  fps   : {cur['fps']}")
                print(f"  fourcc: {cur['fourcc']}")
                logger.info(f"当前相机参数(OpenCV): {cur}")

            elif choice == "6":
                cap, actual, requested, fourcc, applied_ctrls = reopen_camera_flow(device, cap, logger, session)
                print(f"\n已重新打开：{requested['pixfmt']} {requested['width']}x{requested['height']} @{requested['fps']}fps")
                print(f"实际参数：{actual}")
                if applied_ctrls:
                    print("已应用控制项：", applied_ctrls)

                # 更新 meta 文件（覆盖写入，保留 config_history）
                (out_root / "session_meta.json").write_text(
                    json.dumps(
                        {
                            "session_id": session_id,
                            "device": device,
                            "start_time": session.start_time,
                            "config_history": session.config_history,
                        },
                        ensure_ascii=False,
                        indent=2,
                    ),
                    encoding="utf-8",
                )

            elif choice == "7":
                session.operations.append("print_and_log_all_params")
                print_and_log_all_params(device, logger)

            elif choice == "q":
                logger.info("用户选择退出。")
                break
            else:
                print("无效选项。")

    except Exception as e:
        logger.exception(f"发生异常: {e}")
        return 2
    finally:
        session.end_time = time.strftime("%Y-%m-%d %H:%M:%S")
        logger.info("关闭相机与资源。")
        safe_release(cap)

        summary = {
            "session_id": session.session_id,
            "device": session.device,
            "start_time": session.start_time,
            "end_time": session.end_time,
            "operations": session.operations,
            "saved_images": session.saved_images,
            "saved_videos": session.saved_videos,
            "test_result": session.test_result,
            "config_history": session.config_history,
        }
        (out_root / "session_summary.json").write_text(
            json.dumps(summary, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        logger.info("=== Session Summary ===")
        logger.info(json.dumps(summary, ensure_ascii=False, indent=2))
        logger.info(f"已保存 session_summary.json 到: {out_root}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
