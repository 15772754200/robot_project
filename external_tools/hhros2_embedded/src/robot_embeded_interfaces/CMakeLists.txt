cmake_minimum_required(VERSION 3.8)
project(robot_embeded_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BmsState.msg"
  "msg/EthercatMaster.msg"
  "msg/EthercatSlave.msg"
  "msg/SpiDevice.msg"
  "msg/UsbDevice.msg"
  "srv/ConfigureSpi.srv"
  "srv/ConfigureUsbSerial.srv"
  "srv/ConfigureUsbVideo.srv"
  "srv/QuerySpi.srv"
  "srv/QueryUsbDevices.srv"
  "srv/ReadUsbDevice.srv"
  "srv/QueryBms.srv"
  "srv/QueryEthercatMasters.srv"
  "srv/SendBmsControl.srv"
  "srv/SendPowerControl.srv"
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
