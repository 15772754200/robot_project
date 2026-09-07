首先替换代码库中的绝对路径 /home/yy
例如BASE = "/home/yy/robot_s/services"
BT_LOG = "/home/yy/robot_s/services/bluetooth/bluetooth_connections.txt"
ETH_LOG = "/home/yy/robot_s/services/enthernet/network_info.txt"
RTC_LOG = "/home/yy/robot_s/services/RTC/rtc_log.txt"


python3 menu/main_menu.py

涵盖功能如下
主菜单
1. 客户使用蓝牙功能
2. 客户查询蓝牙使用历史
3. 客户使用有线网络功能
4. 客户查询有线网络使用历史
5. 客户修改RTC时钟
6. 客户查询修改RTC时钟历史
7. 客户使用遥控功能
8. 客户获取 TOF 传感器数据
9. 客户查询 TOF 数据历史
10. 客户获取触摸传感器数据
11. 客户查询触摸数据历史
12. 客户查询 TOF 数据历史
0. 退出

