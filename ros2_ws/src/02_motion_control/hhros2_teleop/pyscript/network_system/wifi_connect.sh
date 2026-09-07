#!/bin/bash

# 检查 Wi-Fi 当前状态（enabled/disabled）
wifi_status=$(nmcli radio wifi)

# 获取 Wi-Fi 设备名
wifi_device=$(nmcli device | awk '/wifi/{print $1; exit}')

# 如果 Wi-Fi 关闭，则开启并自动连接
if [ "$wifi_status" = "disabled" ]; then
    echo " Wi-Fi 当前为关闭状态，正在开启..."
    nmcli radio wifi on
    sleep 2

    # 获取最近连接的 Wi-Fi
    latest_wifi=$(nmcli -t -f NAME,TYPE,TIMESTAMP-REAL connection show | grep wifi | sort -t: -k3,3r | head -n1 | cut -d: -f1)

    if [ -n "$latest_wifi" ]; then
        echo " 找到最近连接过的 Wi-Fi: $latest_wifi"
        echo " 正在尝试连接..."
        nmcli connection up "$latest_wifi"
        if [ $? -eq 0 ]; then
            echo "已成功连接到 $latest_wifi"
        else
            echo "连接失败，改为扫描可用 Wi-Fi"
            latest_wifi=""
        fi
    fi

    # 如果没有保存的 Wi-Fi，则选择信号最强的一个
    if [ -z "$latest_wifi" ]; then
        echo " 正在扫描可用 Wi-Fi..."
        sleep 10
        best_wifi=$(nmcli -t -f SSID,SIGNAL device wifi list | sort -t: -k2 -nr | head -n1 | cut -d: -f1)
        if [ -z "$best_wifi" ]; then
            echo " 没有检测到任何 Wi-Fi 网络"
            exit 1
        fi
        echo " 信号最强的 Wi-Fi: $best_wifi"

        echo "请输入密码:"
        read -s wifi_pass
        echo

        if [ -z "$wifi_pass" ]; then
            echo " 未输入密码"
            nmcli device wifi connect "$best_wifi"
        else
            nmcli device wifi connect "$best_wifi" password "$wifi_pass"
        fi

        if [ $? -eq 0 ]; then
            echo "✅ 已成功连接到 $best_wifi"
        else
            echo "❌ 无法连接到 $best_wifi"
            exit 1
        fi
    fi

    # 获取当前连接信息
    connected_wifi=$(nmcli -t -f ACTIVE,SSID device wifi | grep "^yes" | cut -d: -f2)
    echo
    echo " 当前连接: $connected_wifi"

    echo " Wi-Fi 信息："
    nmcli -f GENERAL.DEVICE,GENERAL.CONNECTION,IP4.ADDRESS,IP4.GATEWAY device show "$wifi_device" | grep -E 'DEVICE|CONNECTION|IP4'

    echo
    echo " 信号详情："
    nmcli -f IN-USE,SSID,BSSID,SIGNAL,RATE,FREQ,SECURITY device wifi list | grep "$connected_wifi"

    echo
    echo -e "\n局域网已识别设备："
        arp -n | grep -v "incomplete" | awk '{printf "IP: %-15s MAC地址: %s\n", $1, $3}'

# 如果 Wi-Fi 已经开启，则关闭
else
    echo " Wi-Fi 当前为开启状态，正在关闭..."
    nmcli radio wifi off
    echo " Wi-Fi 已关闭"
fi
