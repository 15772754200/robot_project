cmake_minimum_required(VERSION 3.16)
project(hhros2_motor_protocol)

find_package(ament_cmake REQUIRED)

add_library(hhros2_motor_protocol INTERFACE)

target_include_directories(
  hhros2_motor_protocol INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

install(
  TARGETS hhros2_motor_protocol
  EXPORT export_hhros2_motor_protocol
)

install(
  DIRECTORY include/
  DESTINATION include
)

ament_export_include_directories(include)
ament_export_targets(export_hhros2_motor_protocol HAS_LIBRARY_TARGET)
ament_package()
