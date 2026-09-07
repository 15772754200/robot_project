1：rtc测试时，rtc.py脚本中添加了自动创建日志文件，解决需要手动创建日志文件问题
2：蓝牙测试时，bluetooth.py脚本中添加了自动创建日志文件，解决需要手动创建日志文件问题
3：热点测试时，hotspot.py添加自动创建日志文件
4：mic功能测试正常
5：speaker功能为空，没有相关控制脚本
6：sealsense相机endcam指令脚本运行以跑通，添加自动检测功能包是否编译，自动编译运行功能
7：sealsense相机查看日志指令ldcam测试正常
8：rgb相机enrgb脚本添加自动编译和修改日志路经，能够正常启动，无相机测试
9：lrgb指令测试正常，可以正常显示rgb相机日志记录内容
10：enlid指令添加自动搜索雷达功能包并编译执行，目前可以正常运行，未接雷达测试
11：控制机器人部分暂时未测试
12：system 指令执行后，关闭终端失败
13：peripheral指令功能正常，最后提示关闭终端指令执行失败
14：rgbd指令测试可以正常运行，这个指令不修改日志路径
15：camera_interface 修改执行脚本路经后正常
16：network 指令测试正常，不需要修改任何东西
17：imu指令待测
18：bule_rtc指令中，TOF没有相应功能，触摸传感器功能没有，脚本中有两个tof历史查看重复
19：dcch 指令添加初次使用时需要安装的依赖，可执行脚本，但是没设备测试
20：lich 指令修改json日志文件输出路径，统一放在run_logs目录下，测试正常
21：devps，adtp,lpm,qstg,cstg指令最后退出时显示关闭终端指令执行失败


ros2 service call /mujoco/unpause std_srvs/srv/Empty