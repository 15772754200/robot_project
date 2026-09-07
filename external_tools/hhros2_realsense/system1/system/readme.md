1.进行WiFi查询与连接功能
sudo ./wifi_connect.sh
2.进行WiFi连接查询与写进日志功能
sudo python3 wifi_log.py
3.进行WiFi历史日志检测与查询功能
sudo python3 wifi_read.py
4.进行网络共享功能设置与开启功能，并且写进日志
sudo python3 hotspot.py
5.进行通信链路自检功能并且查询结果写进日志
sudo python3 tongxin_test.py
6、进行网络共享开启与设备连接记录查询，默认查询最新的共享日志信息
sudo python3 hotspot_log_viewer.py
7、统一WIFI系统脚本功能启动，可实现上述一到六具体功能。
python net_toolbox_menu.py