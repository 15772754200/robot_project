# 麦克风使用指南
# 用户只需要直接运行mic.py脚本即可，下面是操作麦克风的一些步骤，所有步骤已经集成到mic.py中，会自动执行下面的步骤
1. 串口规则配置
   - 将麦克风接入 Linux 主机，执行 `ll /dev` 命令可看到设备名 `ttyACM0`。
   - 进入 `hhros2_mic/serial_driver/`文件夹，运行 `sudo ./ch9102_udev.sh` 命令配置规则。
   - 插拔设备后再次输入 `ll /dev`命令，设备名显示为 `wheeltec_mic` 即配置成功。
2. cJSON 安装（只需要初次使用时执行）
   - 进入`hhros2_mic/cJSON`依次执行 `mkdir build`, `cmake ..`, `make`, `sudo make install` 命令安装头文件与库。
   - 运行 `sudo vim /etc/ld.so.conf` 命令将 `/usr/local/lib` 这一行地址添加到文件末尾中，然后保存关闭编辑器，执行 `sudo /sbin/ldconfig` 命令更新动态库配置。
3. 编译麦克风驱动文件（只需要初次使用时执行）
   - 进入 `hhros2_micM2_SDK/offline_mic/samples/record_sample` 文件夹，执行 `cmode +x ./make.sh && sh ./make.sh` 命令编译驱动。
4. 修改麦克风py文件地址(如果需要)
   - 打开 `hhros2_mic/mic.py` ，修改文件第9行地址为 `/home/yourusrname/yidong_robot_project/external_tools/hhros2_mic/M2_SDK/offline_mic/bin` 地址后保存，yourusrname为你的用户名称
5. 运行麦克风
   - 进入 `/mic`，运行 `chmod +x ./mic.py && python mic.py`，说口令 “小薇小薇” 可唤醒麦克风。
6. setup_mic.sh
   - 该脚本主要用来自动设置1-4步骤的操作，简化用户操作步骤，已经集成到mic.py中，最终用户只需要直接运行mic.py脚本即可
