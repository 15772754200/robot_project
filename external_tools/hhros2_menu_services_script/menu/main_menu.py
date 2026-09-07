import subprocess
import os
import stat
from pathlib import Path

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVICES_DIR = os.path.join(BASE, "services")

# ---------- 自动定位项目根目录 ----------
def find_project_root() -> Path | None:
    """从当前目录向上查找，直到找到同时包含 external_tools 和 ros2_ws 的目录"""
    current = Path(__file__).resolve().parent
    for parent in [current] + list(current.parents):
        if (parent / "external_tools").exists() and (parent / "ros2_ws").exists():
            return parent
    return None

PROJECT_ROOT = find_project_root()
if PROJECT_ROOT is None:
    LOG_BASE = Path(SERVICES_DIR) / "logs"
    print(f"⚠️ 警告：未找到项目根目录，日志将保存在 {LOG_BASE}")
else:
    LOG_BASE = PROJECT_ROOT / "ros2_ws" / "run_logs"

# 统一的日志路径
BT_LOG  = str(LOG_BASE / "bluetooth" / "bluetooth_connections.log")
ETH_LOG = str(LOG_BASE / "ethernet" / "network_info.log")
RTC_LOG = str(LOG_BASE / "rtc" / "rtc_log.log")
TOF_LOG = str(LOG_BASE / "tof" / "tof_data.log")
TOUCH_LOG = str(LOG_BASE / "touch" / "touch_data.log")

# 创建日志目录
for log_dir in [LOG_BASE / "bluetooth", LOG_BASE / "ethernet", LOG_BASE / "rtc", 
                LOG_BASE / "tof", LOG_BASE / "touch"]:
    log_dir.mkdir(parents=True, exist_ok=True)


def ensure_executable(script_path):
    """
    检查脚本是否有执行权限，如果没有则自动添加
    同时修复换行符
    """
    if not os.path.exists(script_path):
        print(f"❌ 错误: 脚本不存在: {script_path}")
        return False
    
    # 修复换行符 (CRLF -> LF)
    try:
        with open(script_path, 'rb') as f:
            content = f.read()
        if b'\r\n' in content:
            content = content.replace(b'\r\n', b'\n')
            with open(script_path, 'wb') as f:
                f.write(content)
            print(f"🔧 修复换行符: {script_path}")
    except Exception as e:
        pass
    
    # 检查是否有执行权限
    if not os.access(script_path, os.X_OK):
        print(f"🔧 添加执行权限: {script_path}")
        try:
            st = os.stat(script_path)
            os.chmod(script_path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            print(f"✅ 执行权限已添加: {script_path}")
        except Exception as e:
            print(f"❌ 添加执行权限失败: {e}")
            return False
    return True


def run_script(script_path, background=False):
    """
    使用 bash 显式执行脚本
    """
    if not ensure_executable(script_path):
        return False
    
    try:
        if background:
            subprocess.Popen(["bash", script_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            subprocess.run(["bash", script_path], check=False)
        return True
    except Exception as e:
        print(f"❌ 执行脚本失败: {e}")
        return False


def bluetooth_menu():
    print("1. 客户使用蓝牙功能")
    print("2. 停止使用蓝牙功能")
    c = input("选择: ")

    if c == "1":
        script = f"{SERVICES_DIR}/bluetooth/start.sh"
        run_script(script)
    elif c == "2":
        script = f"{SERVICES_DIR}/bluetooth/stop.sh"
        run_script(script)


def show_bluetooth_history():
    print("\n===== 蓝牙连接历史 =====\n")

    if not os.path.exists(BT_LOG):
        print("暂无蓝牙连接记录。")
        input("\n按回车返回...")
        return

    try:
        with open(BT_LOG, "r", encoding="utf-8") as f:
            content = f.read().strip()
    except Exception as e:
        print(f"读取日志失败: {e}")
        input("\n按回车返回...")
        return

    if not content:
        print("日志文件为空。")
    else:
        print(content)

    print("\n===== 结束 =====")
    input("\n按回车返回...")


def enthernet():
    print("1. 客户使用有线网络功能")
    print("2. 停止使用有线网络功能")
    c = input("选择: ")

    if c == "1":
        # 先确保 network.py 有执行权限
        network_script = f"{SERVICES_DIR}/enthernet/network.py"
        ensure_executable(network_script)
        script = f"{SERVICES_DIR}/enthernet/start.sh"
        run_script(script)
    elif c == "2":
        script = f"{SERVICES_DIR}/enthernet/stop.sh"
        run_script(script)


def show_enthernet_history():
    print("\n===== 有线网络连接历史 =====\n")

    if not os.path.exists(ETH_LOG):
        print("暂无有线网络连接记录。")
        input("\n按回车返回...")
        return

    try:
        with open(ETH_LOG, "r", encoding="utf-8") as f:
            content = f.read().strip()
    except Exception as e:
        print(f"读取日志失败: {e}")
        input("\n按回车返回...")
        return

    if not content:
        print("日志文件为空。")
    else:
        print(content)

    print("\n===== 结束 =====")
    input("\n按回车返回...")


def RTC():
    print("1. 客户修改RTC时钟")
    print("2. 停止修改RTC时钟")
    c = input("选择: ")

    if c == "1":
        script = f"{SERVICES_DIR}/RTC/start.sh"
        if ensure_executable(script):
            print("进入 RTC 本地交互模式（退出请输入 QUIT）")
            os.system(f"bash {script}")
    elif c == "2":
        script = f"{SERVICES_DIR}/RTC/stop.sh"
        run_script(script)


def show_RTC_history():
    print("\n===== RTC修改历史 =====\n")

    if not os.path.exists(RTC_LOG):
        print("暂无RTC修改记录。")
        input("\n按回车返回...")
        return

    try:
        with open(RTC_LOG, "r", encoding="utf-8") as f:
            content = f.read().strip()
    except Exception as e:
        print(f"读取日志失败: {e}")
        input("\n按回车返回...")
        return

    if not content:
        print("日志文件为空。")
    else:
        print(content)

    print("\n===== 结束 =====")
    input("\n按回车返回...")


def remote_control():
    print("1. 客户使用遥控功能")
    print("2. 停止使用遥控功能")
    c = input("选择: ")

    if c == "1":
        script = f"{SERVICES_DIR}/remotecontrol/start.sh"
        run_script(script)
    elif c == "2":
        script = f"{SERVICES_DIR}/remotecontrol/stop.sh"
        run_script(script)


def tof_sensor():
    print("1. 客户获取 TOF 传感器数据")
    print("2. 停止获取 TOF 传感器数据")
    c = input("选择: ")

    if c == "1":
        script = f"{SERVICES_DIR}/tof/start.sh"
        run_script(script)
    elif c == "2":
        script = f"{SERVICES_DIR}/tof/stop.sh"
        run_script(script)


def touch_sensor():
    print("1. 客户获取触摸传感器数据")
    print("2. 停止获取触摸传感器数据")
    c = input("选择: ")

    if c == "1":
        script = f"{SERVICES_DIR}/touch/start.sh"
        run_script(script)
    elif c == "2":
        script = f"{SERVICES_DIR}/touch/stop.sh"
        run_script(script)


def show_tof_history():
    print("\n===== TOF 数据历史 =====\n")
    
    if not os.path.exists(TOF_LOG):
        print("暂无 TOF 数据记录。")
        input("\n按回车返回...")
        return

    try:
        with open(TOF_LOG, "r", encoding="utf-8") as f:
            content = f.read().strip()
    except Exception as e:
        print(f"读取日志失败: {e}")
        input("\n按回车返回...")
        return

    if not content:
        print("日志文件为空。")
    else:
        print(content)

    print("\n===== 结束 =====")
    input("\n按回车返回...")


def show_touch_history():
    print("\n===== 触摸数据历史 =====\n")
    
    if not os.path.exists(TOUCH_LOG):
        print("暂无触摸数据记录。")
        input("\n按回车返回...")
        return

    try:
        with open(TOUCH_LOG, "r", encoding="utf-8") as f:
            content = f.read().strip()
    except Exception as e:
        print(f"读取日志失败: {e}")
        input("\n按回车返回...")
        return

    if not content:
        print("日志文件为空。")
    else:
        print(content)

    print("\n===== 结束 =====")
    input("\n按回车返回...")


def batch_fix_all_scripts():
    """
    一次性修复所有服务的脚本权限和换行符
    """
    print("\n🔧 批量修复所有服务脚本...")
    
    services = [
        "bluetooth",
        "enthernet",
        "RTC",
        "remotecontrol",
        "tof",
        "touch"
    ]
    
    for service in services:
        service_dir = os.path.join(SERVICES_DIR, service)
        if os.path.exists(service_dir):
            for root, dirs, files in os.walk(service_dir):
                for file in files:
                    # 同时处理 .sh 和 .py 文件
                    if file.endswith('.sh') or file.endswith('.py'):
                        script_path = os.path.join(root, file)
                        ensure_executable(script_path)
    
    print("✅ 批量修复完成！")


def main():
    # 启动时自动修复所有脚本权限
    batch_fix_all_scripts()
    
    while True:
        print("\n主菜单")
        print("1. 客户使用蓝牙功能")
        print("2. 客户查询蓝牙使用历史")
        print("3. 客户使用有线网络功能")
        print("4. 客户查询有线网络使用历史")
        print("5. 客户修改RTC时钟")
        print("6. 客户查询修改RTC时钟历史")
        print("7. 客户使用遥控功能")
        print("8. 客户获取 TOF 传感器数据")
        print("9. 客户查询 TOF 数据历史")
        print("10. 客户获取触摸传感器数据")
        print("11. 客户查询触摸数据历史")
        print("0. 退出")
        print("按回车键返回主菜单")

        choice = input("选择: ")

        if choice == "1":
            bluetooth_menu()
        elif choice == "2":
            show_bluetooth_history()
        elif choice == "3":
            enthernet()
        elif choice == "4":
            show_enthernet_history()
        elif choice == "5":
            RTC()
        elif choice == "6":
            show_RTC_history()
        elif choice == "7":
            remote_control()
        elif choice == "8":
            tof_sensor()
        elif choice == "9":
            show_tof_history()
        elif choice == "10":
            touch_sensor()
        elif choice == "11":
            show_touch_history()
        elif choice == "0":
            break
        elif choice == "":
            continue
        else:
            print("❌ 无效选择，请重新输入。")


if __name__ == "__main__":
    main()