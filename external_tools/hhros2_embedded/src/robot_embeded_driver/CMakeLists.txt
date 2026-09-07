cmake_minimum_required(VERSION 3.8)
project(robot_embeded_driver)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(robot_embeded_interfaces REQUIRED)
find_package(Threads REQUIRED)

add_library(robot_embeded_hardware_driver STATIC
  src/hardware_driver.cpp
)
target_compile_features(robot_embeded_hardware_driver PUBLIC cxx_std_17)
target_include_directories(robot_embeded_hardware_driver
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(robot_embeded_hardware_driver PUBLIC Threads::Threads)

function(add_robot_embeded_node target source)
  add_executable(${target} ${source})
  target_compile_features(${target} PUBLIC cxx_std_17)
  target_link_libraries(${target} robot_embeded_hardware_driver)
  ament_target_dependencies(${target}
    rclcpp
    robot_embeded_interfaces
  )
endfunction()

add_robot_embeded_node(robot_embeded_spi_node src/spi_node.cpp)
add_robot_embeded_node(robot_embeded_usb_node src/usb_node.cpp)
add_robot_embeded_node(robot_embeded_bms_node src/bms_node.cpp)
add_robot_embeded_node(robot_embeded_power_node src/power_node.cpp)
add_robot_embeded_node(robot_embeded_ethercat_node src/ethercat_node.cpp)

install(TARGETS
  robot_embeded_hardware_driver
  robot_embeded_spi_node
  robot_embeded_usb_node
  robot_embeded_bms_node
  robot_embeded_power_node
  robot_embeded_ethercat_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
