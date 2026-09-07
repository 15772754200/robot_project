#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
bt_logger.py

监听 BlueZ 的 DBus 信号，当设备连接或断开时记录：
  - 设备名称(Name)
  - MAC 地址(Address)
  - 连接时间 或 断开时间(ISO8601)

日志默认保存到 ./bluetooth_connections.txt
依赖：
    sudo apt install python3-dbus python3-gi
"""

import dbus
import dbus.mainloop.glib
from gi.repository import GLib
import sys
from datetime import datetime
import os

# 日志文件路径
LOG_PATH = "run_logs/bluetooth/bluetooth_connections.log"


def log_event(name: str, address: str, when: datetime, connected: bool):
    """写入连接或断开事件"""
    status = "CONNECTED" if connected else "DISCONNECTED"
    line = f"{when.isoformat()} | {status} | {name} | {address}\n"
    try:
        """自动创建目录和文件 """
        log_dir = os.path.dirname(LOG_PATH)
        if log_dir and not os.path.exists(log_dir):
            os.makedirs(log_dir, exist_ok=True)

        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(line)
    except Exception as e:
        print("写入日志失败：", e, file=sys.stderr)
    else:
        print("Logged:", line.strip())


def on_properties_changed(interface, changed, invalidated, object_path):
    """DBus 设备属性变化回调"""
    if interface != "org.bluez.Device1":
        return

    if "Connected" in changed:
        connected = bool(changed["Connected"])
        try:
            bus = dbus.SystemBus()
            obj = bus.get_object("org.bluez", object_path)
            props = dbus.Interface(obj, "org.freedesktop.DBus.Properties")

            try:
                name = props.Get("org.bluez.Device1", "Name")
            except Exception:
                name = "<unknown>"

            try:
                address = props.Get("org.bluez.Device1", "Address")
            except Exception:
                address = "<unknown>"

            when = datetime.now()
            log_event(str(name), str(address), when, connected)

        except Exception as e:
            print("获取设备属性失败：", e, file=sys.stderr)


def on_interfaces_added(object_path, interfaces_and_props):
    """当设备刚被发现且已经处于连接状态时，也记录一次"""
    if "org.bluez.Device1" in interfaces_and_props:
        props = interfaces_and_props["org.bluez.Device1"]
        if "Connected" in props and bool(props["Connected"]):
            name = props.get("Name", "<unknown>")
            address = props.get("Address", "<unknown>")
            when = datetime.now()
            log_event(str(name), str(address), when, True)


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()

    # 监听设备连接状态变化
    bus.add_signal_receiver(
        handler_function=on_properties_changed,
        signal_name="PropertiesChanged",
        dbus_interface="org.freedesktop.DBus.Properties",
        arg0="org.bluez.Device1",
        path_keyword="object_path"
    )

    # 监听新设备接口添加
    bus.add_signal_receiver(
        handler_function=on_interfaces_added,
        signal_name="InterfacesAdded",
        dbus_interface="org.freedesktop.DBus.ObjectManager",
        path="/"
    )

    print("Bluetooth connection logger running.")
    print("Logging to:", LOG_PATH)
    print("按 Ctrl+C 退出\n")

    try:
        loop = GLib.MainLoop()
        loop.run()
    except KeyboardInterrupt:
        print("\n退出。")
        try:
            loop.quit()
        except:
            pass


if __name__ == "__main__":
    main()