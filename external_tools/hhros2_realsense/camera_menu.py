"""
RealSense D435i ROS2 运行时调参 + topic 打印小工具

使用说明：
  1. 先启动相机节点（例如你的 start.sh）
  2. 确认终端已经 source ROS2 环境
  3. python3 realsense_menu.py
"""

import subprocess
import sys

# === 默认/候选 RealSense 节点名称 ===
CAMERA_NODE = "/camera/camera"
CAMERA_NODE_CANDIDATES = [
    "/camera/camera",
    "/camera",
    "/camera/realsense2_camera",
    "/realsense2_camera",
]

# === 我们关心的、在运行时可调的典型参数（D435i） ===
RUNTIME_PARAMS = {
    # ---------- 深度模块 ----------
    "depth_module.enable_auto_exposure": {
        "type": "bool",
        "desc": "深度/IR 自动曝光开关 (0=关, 1=开)",
        "range": "0 或 1"
    },
    "depth_module.exposure": {
        "type": "int",
        "desc": "深度/IR 手动曝光，通常单位为微秒 (~1e3~1.6e5，对应 1~166ms)",
        "range": "1000 ~ 166000 (推荐)"
    },
    "depth_module.gain": {
        "type": "int",
        "desc": "深度/IR 增益 (D400 系列典型 16~248)",
        "range": "16 ~ 248 (推荐)"
    },
    "depth_module.laser_power": {
        "type": "float",
        "desc": "IR 投射器激光功率 (0 关闭, 最大约 360)",
        "range": "0 ~ 360"
    },
    "depth_module.emitter_enabled": {
        "type": "bool",
        "desc": "IR 发射器开关 (0=关, 1=开)",
        "range": "0 或 1"
    },

    # ---------- 彩色相机 ----------
    "rgb_camera.enable_auto_exposure": {
        "type": "bool",
        "desc": "彩色相机自动曝光开关 (0=关, 1=开)",
        "range": "0 或 1"
    },
    "rgb_camera.exposure": {
        "type": "int",
        "desc": "彩色相机手动曝光，通常为微秒，常见 1e2~1e5",
        "range": "100 ~ 100000 (推荐)"
    },
    "rgb_camera.gain": {
        "type": "int",
        "desc": "彩色相机增益，常见范围 0~128 或接近",
        "range": "0 ~ 128 (推荐)"
    },
    "rgb_camera.enable_auto_white_balance": {
        "type": "bool",
        "desc": "彩色相机自动白平衡开关 (0=关, 1=开)",
        "range": "0 或 1"
    },
    "rgb_camera.white_balance": {
        "type": "int",
        "desc": "彩色相机白平衡色温 (K)，典型 2800~6500",
        "range": "2800 ~ 6500"
    },
}


ACTIVE_CAMERA_NODE = CAMERA_NODE


def run_cmd(cmd, check=False, capture_output=False):
    """简单封装 subprocess 调用"""
    try:
        if capture_output:
            result = subprocess.check_output(cmd, text=True)
            return result
        else:
            subprocess.run(cmd, check=check)
            return ""
    except FileNotFoundError:
        print("[错误] 找不到 ros2 命令，请先 source ROS2 环境。")
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"[错误] 命令执行失败: {' '.join(cmd)}")
        print(e)
        return None


def detect_camera_node():
    """自动探测当前实际存在的 RealSense 节点名。"""
    out = run_cmd(["ros2", "node", "list"], capture_output=True)
    if out is None:
        return None
    nodes = out.splitlines()

    for candidate in CAMERA_NODE_CANDIDATES:
        if candidate in nodes:
            return candidate

    # 兜底：尝试从已发布的话题反推命名空间，例如 /camera/color/image_raw -> /camera
    topic_out = run_cmd(["ros2", "topic", "list"], capture_output=True)
    if topic_out is not None:
        topics = [t for t in topic_out.splitlines() if t]
        for topic in topics:
            if topic.endswith("/color/image_raw"):
                prefix = topic[: -len("/color/image_raw")]
                if prefix in nodes:
                    return prefix

    # 再兜底：查找名字里包含 camera / realsense 的节点
    for node in nodes:
        if "realsense" in node.lower() or "camera" in node.lower():
            return node

    return None


def ensure_camera_node():
    """检查并记录相机节点名称。"""
    global ACTIVE_CAMERA_NODE
    node = detect_camera_node()
    if node is not None:
        ACTIVE_CAMERA_NODE = node
        print(f"[信息] 检测到相机节点: {ACTIVE_CAMERA_NODE}")
    else:
        print(f"[警告] 在 node list 中没有找到预期节点 {CAMERA_NODE}")
        print("       可继续使用 topic 打印功能，但运行时参数调整可能失败。")


def get_param(node, param_name):
    """读取当前参数值"""
    out = run_cmd(["ros2", "param", "get", node, param_name], capture_output=True)
    if out is None:
        print(f"[警告] 读取参数 {param_name} 失败。")
        return None
    # 形如: "Integer value is: 150"
    parts = out.split(":", 1)
    if len(parts) > 1:
        return parts[1].strip()
    return out.strip()


def set_param(node, param_name, value_str):
    """设置参数值"""
    print(f"\n>>> 执行: ros2 param set {node} {param_name} {value_str}")
    result = run_cmd(["ros2", "param", "set", node, param_name, value_str], check=False)
    if result is not None:
        print("[信息] 参数设置命令已发送。若值超出硬件范围，驱动会报错。")


def convert_value(vtype, raw):
    """根据参数类型将用户输入转换为 ros2 可接受的字符串"""
    raw = raw.strip()
    if vtype == "bool":
        v = raw.lower()
        if v in ("1", "true", "t", "y", "yes", "on"):
            return "true"
        if v in ("0", "false", "f", "n", "no", "off"):
            return "false"
        raise ValueError("布尔值请输入 0/1 或 true/false/yes/no")
    elif vtype == "int":
        iv = int(raw)
        return str(iv)
    elif vtype == "float":
        fv = float(raw)
        return str(fv)
    else:
        # 保底：直接原样传
        return raw


def param_menu():
    """参数调整菜单"""
    while True:
        print("\n========== 相机运行时参数调整 ==========")
        for idx, (name, meta) in enumerate(RUNTIME_PARAMS.items(), start=1):
            print(f"{idx:2d}) {name}")
            print(f"    描述: {meta['desc']}")
            print(f"    推荐范围: {meta['range']}")
        print("---------------------------------------")
        print("b) 返回上一级菜单")
        print("q) 退出程序")

        sel = input("请选择要修改的参数序号: ").strip().lower()
        if sel == "b":
            return
        if sel == "q":
            print("退出程序。")
            sys.exit(0)

        try:
            idx = int(sel) - 1
            param_name = list(RUNTIME_PARAMS.keys())[idx]
        except (ValueError, IndexError):
            print("[警告] 无效选择，请重新输入。")
            continue

        meta = RUNTIME_PARAMS[param_name]
        cur_val = get_param(ACTIVE_CAMERA_NODE, param_name)
        if cur_val is not None:
            print(f"当前值: {cur_val}")
        else:
            print("当前值: <获取失败>")

        new_raw = input(f"请输入新的值（{meta['range']}，直接回车取消修改）: ").strip()
        if new_raw == "":
            print("已取消本次修改。")
            continue

        try:
            value_str = convert_value(meta["type"], new_raw)
        except ValueError as e:
            print(f"[错误] 输入格式不合法: {e}")
            continue

        set_param(ACTIVE_CAMERA_NODE, param_name, value_str)


def topic_menu():
    """话题选择 + 打印菜单"""
    while True:
        out = run_cmd(["ros2", "topic", "list"], capture_output=True)
        if out is None:
            return
        topics = [t for t in out.splitlines() if t]

        if not topics:
            print("[警告] 当前没有任何 topic。")
            return

        print("\n========== ROS2 Topic 列表 ==========")
        for idx, t in enumerate(topics, start=1):
            mark = " [camera]" if "/camera/" in t else ""
            print(f"{idx:2d}) {t}{mark}")
        print("------------------------------------")
        print("b) 返回上一级菜单")
        print("q) 退出程序")

        sel = input("请选择要 echo 的 topic 序号: ").strip().lower()
        if sel == "b":
            return
        if sel == "q":
            print("退出程序。")
            sys.exit(0)

        try:
            idx = int(sel) - 1
            topic = topics[idx]
        except (ValueError, IndexError):
            print("[警告] 无效选择，请重新输入。")
            continue

        echo_topic(topic)


def echo_topic(topic):
    """启动 ros2 topic echo，并通过按键退出"""
    print(f"\n>>> 开始打印 topic: {topic}")
    print("按 回车 停止打印并返回菜单；")
    print("输入 q 后回车 将直接退出整个程序。")

    # 启动 echo 进程
    try:
        proc = subprocess.Popen(["ros2", "topic", "echo", topic])
    except FileNotFoundError:
        print("[错误] 找不到 ros2 命令，请检查环境。")
        return

    try:
        user_in = input()
    except KeyboardInterrupt:
        user_in = ""

    # 停止 echo
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except Exception:
        proc.kill()

    if user_in.strip().lower() == "q":
        print("退出程序。")
        sys.exit(0)
    else:
        print("已停止当前 topic 打印，返回菜单。")


def main_menu():
    """主菜单"""
    ensure_camera_node()
    while True:
        print("\n========== RealSense D435i 工具 ==========")
        print("1) 调整相机运行时参数 (曝光/增益/激光等)")
        print("2) 选择 topic 并打印数据")
        print("q) 退出程序")
        choice = input("请选择: ").strip().lower()

        if choice == "1":
            param_menu()
        elif choice == "2":
            topic_menu()
        elif choice == "q":
            print("退出程序。")
            break
        else:
            print("[警告] 无效选项，请重新输入。")


if __name__ == "__main__":
    main_menu()
