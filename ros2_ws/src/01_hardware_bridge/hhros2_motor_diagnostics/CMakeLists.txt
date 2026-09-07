cmake_minimum_required(VERSION 3.16)
project(hhros2_motor_diagnostics)

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(hhros2_description REQUIRED)
find_package(hhros2_interfaces REQUIRED)
find_package(hhros2_motor_protocol REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>"
)

add_executable(single_shot_motor_probe src/single_shot_motor_probe.cpp)
target_include_directories(single_shot_motor_probe PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
ament_target_dependencies(single_shot_motor_probe
  hhros2_interfaces
  hhros2_motor_protocol
  rclcpp
  sensor_msgs
  std_msgs
)
target_link_libraries(single_shot_motor_probe
  ament_index_cpp::ament_index_cpp
  yaml-cpp
)

add_executable(ankle_parallel_sweep_probe src/ankle_parallel_sweep_probe.cpp)
ament_target_dependencies(ankle_parallel_sweep_probe
  ament_index_cpp
  hhros2_description
  hhros2_interfaces
  rclcpp
  sensor_msgs
  std_msgs
)
target_link_libraries(ankle_parallel_sweep_probe
  ament_index_cpp::ament_index_cpp
  yaml-cpp
)

install(
  DIRECTORY include/
  DESTINATION include
)

install(
  DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

install(
  TARGETS
    single_shot_motor_probe
    ankle_parallel_sweep_probe
  DESTINATION lib/${PROJECT_NAME}
)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
)

ament_export_include_directories(include)
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)

ament_package()
