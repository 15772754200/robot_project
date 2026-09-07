cmake_minimum_required(VERSION 3.16)
project(hhros2_estimation)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(hhros2_interfaces REQUIRED)
find_package(hhros2_log REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/state_estimator_component.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>"
)
ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  rclcpp_components
  sensor_msgs
  nav_msgs
  geometry_msgs
  hhros2_interfaces
  hhros2_log
)
rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "hhros2_estimation::StateEstimatorComponent"
  EXECUTABLE state_estimator_node
)

add_executable(state_estimator src/state_estimator_main.cpp)
target_link_libraries(state_estimator ${PROJECT_NAME})
ament_target_dependencies(state_estimator rclcpp)

install(
  DIRECTORY include/
  DESTINATION include
)
install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(
  TARGETS state_estimator
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

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp 
  rclcpp_components 
  sensor_msgs 
  nav_msgs 
  geometry_msgs 
  hhros2_interfaces 
  hhros2_log
  )
ament_package()
