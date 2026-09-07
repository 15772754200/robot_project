# robot_embeded_ws 使用说明

`robot_embeded_ws` 是一个 ROS2 工作区，用来把机器人控制器上的硬件功能封装成独立 ROS2 节点。每类功能独立成节点，launch 负责一次启动所有节点，后续增加或删除功能时只需要新增/移除对应节点。

## 工程结构

```text
hhros2_embebded/
├── src/
│   ├── robot_embeded_interfaces/   # 自定义 msg/srv 接口
│   ├── robot_embeded_driver/       # SPI、USB、BMS 等硬件节点
│   └── robot_embeded_bringup/      # launch 文件、参数配置和客户终端面板
└── README.md
```

## 功能

- SPI 节点：配置 `/dev/spidevX.Y`，也可以通过 `/query_spi` 列出所有 SPI 接口及当前 mode、speed、bits_per_word、lsb_first。
- USB 节点：通用查询 USB 设备，返回 sysfs、devnode、厂商、产品、接口类型等信息；同时保留串口和视频设备的专用配置服务。
- BMS 节点：通过 RS485 查询电池状态，并支持发送充电 MOS、放电 MOS、工厂模式等控制命令。
- 客户终端面板：客户只需要在终端里按菜单键，即可触发 SPI、USB、BMS 服务请求，不需要手写 `ros2 service call`。

`control_board_status` 和控制板自检接口已从 ROS 接口中移除。

## 编译

```bash
cd /home/niic/yidong_robot_project/external_tools/hhros2_embebded
source /opt/ros/humble/setup.bash
colcon build
source install/setup.bash
```

如果 ROS2 版本不是 Humble，把 `/opt/ros/humble/setup.bash` 换成实际版本。

## 启动

```bash
ros2 launch robot_embeded_bringup robot_embeded.launch.py
```

启动后会运行：

```text
/robot_embeded_spi_node
/robot_embeded_usb_node
/robot_embeded_bms_node
```

## 客户终端面板（推荐给客户使用）

先在一个终端启动硬件节点：

```bash
cd /home/niic/yidong_robot_project/external_tools/hhros2_embebded
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch robot_embeded_bringup robot_embeded.launch.py
```

再打开另一个终端启动客户面板：

```bash
cd /home/niic/yidong_robot_project/external_tools/hhros2_embebded
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run robot_embeded_bringup customer_panel
```

面板启动后按照菜单输入功能键并回车即可，常用功能如下：

```text
1  查询 BMS 电池状态
2  BMS 控制命令
3  查询全部 SPI 接口及配置
4  使用默认参数配置 SPI
5  查询全部 USB 设备
6  查询 HID USB 设备
7  查询输入事件 USB 设备
8  读取 USB 设备原始数据
9  使用默认参数配置 USB 串口
0  使用默认参数配置 USB 视频
q  退出
```

面板里的“默认参数”来自启动硬件节点时加载的 YAML 参数。默认示例文件是 `src/robot_embeded_bringup/config/robot_embeded.yaml`；如果不是 `--symlink-install` 构建，修改后需要重新 `colcon build`，或启动时显式指定配置文件：

```bash
ros2 launch robot_embeded_bringup robot_embeded.launch.py config:=/path/to/robot_embeded.yaml
```

## 参数配置

默认配置文件：

```text
src/robot_embeded_bringup/config/robot_embeded.yaml
```

主要参数按节点分组：

```yaml
robot_embeded_spi_node:
  ros__parameters:
    spi:
      device: "/dev/spidev0.0"
      mode: 0
      speed_hz: 1000000
      bits_per_word: 8
      lsb_first: 0
      verify: true

robot_embeded_usb_node:
  ros__parameters:
    usb:
      subsystem: ""
      devnode_prefix: ""
      include_without_devnode: true
      read_devnode: ""
      read_max_bytes: 256
      read_timeout_ms: 100
    usb_serial:
      device: "/dev/ttyUSB0"
      baudrate: 115200
    usb_video:
      device: "/dev/video0"
      width: 640
      height: 480
      pixfmt: "YUYV"
      fps: 30

robot_embeded_bms_node:
  ros__parameters:
    bms:
      device: "/dev/ttyUSB0"
      baudrate: 9600
      response_timeout_ms: 1500
      poll_interval_ms: 1000
      publish_enabled: false
```

## Topic

### `/bms_state`

类型：`robot_embeded_interfaces/msg/BmsState`

调用 `/query_bms` 成功后会发布一次。如果把 `bms.publish_enabled` 改成 `true`，BMS 节点会按 `bms.poll_interval_ms` 周期查询并发布。

## Service（开发调试）

下面的 `ros2 service call` 命令主要给开发、联调和问题定位使用。客户现场优先使用上面的客户终端面板。

### SPI

查询机器人本机所有 `/dev/spidev*` 接口及当前配置：

```bash
ros2 service call /query_spi robot_embeded_interfaces/srv/QuerySpi "{use_parameter_defaults: true}"
```

使用 YAML 默认参数配置指定 SPI 接口：

```bash
ros2 service call /configure_spi robot_embeded_interfaces/srv/ConfigureSpi "{use_parameter_defaults: true}"
```

### 通用 USB 查询

查询所有 USB 相关设备：

```bash
ros2 service call /query_usb_devices robot_embeded_interfaces/srv/QueryUsbDevices "{use_parameter_defaults: true}"
```

只看 HID 设备，例如遥控器、手柄、键盘类设备：

```bash
ros2 service call /query_usb_devices robot_embeded_interfaces/srv/QueryUsbDevices "{use_parameter_defaults: false, subsystem: 'hidraw', devnode_prefix: '', include_without_devnode: true}"
```

只看输入事件设备：

```bash
ros2 service call /query_usb_devices robot_embeded_interfaces/srv/QueryUsbDevices "{use_parameter_defaults: false, subsystem: 'input', devnode_prefix: '/dev/input/', include_without_devnode: false}"
```

读取任意 USB devnode 的原始字节，例如 HID 遥控器、手柄或输入设备：

```bash
ros2 service call /read_usb_device robot_embeded_interfaces/srv/ReadUsbDevice "{use_parameter_defaults: false, devnode: '/dev/hidraw0', max_bytes: 64, timeout_ms: 1000}"
ros2 service call /read_usb_device robot_embeded_interfaces/srv/ReadUsbDevice "{use_parameter_defaults: false, devnode: '/dev/input/event0', max_bytes: 256, timeout_ms: 1000}"
```

### USB 专用配置

```bash
ros2 service call /configure_usb_serial robot_embeded_interfaces/srv/ConfigureUsbSerial "{use_parameter_defaults: true}"
ros2 service call /configure_usb_video robot_embeded_interfaces/srv/ConfigureUsbVideo "{use_parameter_defaults: true}"
```

### BMS

```bash
ros2 service call /query_bms robot_embeded_interfaces/srv/QueryBms "{use_parameter_defaults: true}"
```

支持的控制命令：

```text
close-charge-mos
close-discharge-mos
open-charge-mos
open-discharge-mos
enter-factory-mode
```

示例：

```bash
ros2 service call /send_bms_control robot_embeded_interfaces/srv/SendBmsControl "{use_parameter_defaults: true, command: 'open-discharge-mos'}"
```

## 查看接口

```bash
ros2 node list
ros2 topic list
ros2 service list
ros2 interface show robot_embeded_interfaces/srv/QueryUsbDevices
ros2 interface show robot_embeded_interfaces/srv/ReadUsbDevice
```

## 权限说明

这些路径通常需要 root 或 udev 权限：

```text
/dev/spidev*
/dev/ttyUSB*
/dev/ttyTHS*
/dev/video*
/dev/hidraw*
/dev/input/*
/sys/class/*
/sys/bus/usb/devices/*
```

现场调试可以临时用 `sudo` 启动，正式部署建议配置 udev 规则，让运行用户拥有所需设备访问权限。


## 485接口说明

靠近电源的一端是右侧 gpio460，对应/dev/ttyTHS0 4851
      另一端是左侧 gpio477 对应/dev/ttyTHS2 4852

```bash
修改寄存器：
sudo busybox devmem 0x0243d070 w 0x00000400
sudo busybox devmem 0x0243d078 w 0x00000458
实例化 GPIO：
cd /sys/class/gpio
sudo chmod 666 export
echo 460 > export
echo 477 > export
此时当前目录下会多出 PR.04(460)和 PY.07(477)两个目录，选择当前测试的端口
然后进入对应目录
cd PR.04
cd PY.07
sudo chmod 666 direction value
设置 direction 方向为 out
echo "out" > direction
设置 value
echo 0/1 > value 当 value 为 0 时为发送数据，为 1 时为接收数据

```


```bash
//查询是否引脚复用配置成功  
sudo busybox devmem 0x0243d070
sudo busybox devmem 0x0243d078

```
```bash
// 理论输出
  0x00000400
  0x00000458
```

```bash
// 查看串口设备是否存在
ls -l /dev/ttyTHS*

// 对应关系
/dev/ttyTHS0 -> 4851
/dev/ttyTHS2 -> 4852
```

```bash
// 例如测试 /dev/ttyTHS0 对应的 485：

sudo chmod 666 /sys/class/gpio/gpio460/direction /sys/class/gpio/gpio460/value
echo out > /sys/class/gpio/gpio460/direction
切到发送：

echo 0 > /sys/class/gpio/gpio460/value
echo "test" > /dev/ttyTHS0
切回接收：

echo 1 > /sys/class/gpio/gpio460/value
cat /dev/ttyTHS0
```
