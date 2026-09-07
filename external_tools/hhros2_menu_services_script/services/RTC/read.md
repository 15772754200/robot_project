在主机上启动服务程序：

sudo python3 rtc_server.py


程序输出：

Starting RTC server on 0.0.0.0:12345 . Log: rtc_log.txt



如果一切正常，客户端会看到：

RTC SERVER READY. Commands: READ | SET YYYY-MM-DD HH:MM:SS | QUIT


此时你可以输入命令：

READ
2025-10-20 12:31:00


或者：

SET 2025-10-20 12:35:00


返回：

SET OK

从 RTC 更新时间到系统
sudo hwclock --hctosys
