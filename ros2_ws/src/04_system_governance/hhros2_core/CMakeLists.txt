cmake_minimum_required(VERSION 3.16)
project(hhros2_core)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(controller_manager_msgs REQUIRED)
find_package(hhros2_interfaces REQUIRED)
find_package(hhros2_hal REQUIRED)
find_package(hhros2_motor_diagnostics REQUIRED)
find_package(hhros2_log REQUIRED)

add_executable(hhros2_core
  src/hhros2_core_main.cpp
  src/safety_governor_node.cpp
)
target_include_directories(hhros2_core PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
ament_target_dependencies(hhros2_core
  rclcpp
  sensor_msgs
  controller_manager_msgs
  hhros2_interfaces
  hhros2_hal
  hhros2_motor_diagnostics
  hhros2_log
)

install(
  TARGETS hhros2_core
  DESTINATION lib/${PROJECT_NAME}
)
install(
  DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
