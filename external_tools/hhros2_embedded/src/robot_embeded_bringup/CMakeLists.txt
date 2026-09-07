cmake_minimum_required(VERSION 3.8)
project(robot_embeded_bringup)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(robot_embeded_interfaces REQUIRED)

add_executable(customer_panel src/customer_panel.cpp)
target_compile_features(customer_panel PUBLIC cxx_std_17)
ament_target_dependencies(customer_panel
  rclcpp
  robot_embeded_interfaces
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

install(TARGETS
  customer_panel
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
