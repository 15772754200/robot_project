cmake_minimum_required(VERSION 3.5)
project(wave_control_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/WaveCommand.msg"
  "msg/MotorCommand.msg"
  "msg/WaveStatus.msg"
  "msg/JointTrajectory.msg"
  "msg/TeachRecord.msg"
  "srv/WaveControl.srv"
  "srv/TeachControl.srv"
)

ament_package()
