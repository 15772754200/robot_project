cmake_minimum_required(VERSION 3.16)
project(hhros2_motion_cores)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ONNXRUNTIME_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external_tools/hhros2_thirdparty/onnxruntime-linux-aarch64-1.27.0" CACHE PATH "ONNX Runtime root directory")

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(hhros2_interfaces REQUIRED)
find_package(hhros2_log REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(sensor_msgs REQUIRED)

find_path(ONNXRUNTIME_INCLUDE_DIR
  NAMES onnxruntime_cxx_api.h
  HINTS "${ONNXRUNTIME_ROOT_DIR}/include"
  REQUIRED
)

find_library(ONNXRUNTIME_LIBRARY
  NAMES onnxruntime libonnxruntime.so.1.27.0
  HINTS "${ONNXRUNTIME_ROOT_DIR}/lib"
  REQUIRED
)

add_library(${PROJECT_NAME} SHARED
  src/core/motion_core_base.cpp
  src/core/rl_policy_core.cpp
  src/core/wbc_core.cpp
  src/rl/gait/gait_utils.cpp
  src/rl/inference/onnx_backend.cpp
  src/rl/observation/observation_builder.cpp
  src/rl/observation/observation_config.cpp
  src/rl/observation/observation_utils.cpp
  src/rl/policy/policy_resolver.cpp
  src/rl/runtime/action_mapper.cpp
  src/rl/runtime/joint_limits.cpp
  src/rl/runtime/rl_runtime.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>"
  ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
  ${ONNXRUNTIME_LIBRARY}
  Eigen3::Eigen
  yaml-cpp
)

ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  rclcpp_components
  geometry_msgs
  sensor_msgs
  hhros2_interfaces
  hhros2_log
)

# ========== 注册 rclcpp 组件 ==========
rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "hhros2_motion_cores::RlStandCore"
  EXECUTABLE rl_stand_core
)

rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "hhros2_motion_cores::RlWalkCore"
  EXECUTABLE rl_walk_core
)

rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "hhros2_motion_cores::RlRunCore"
  EXECUTABLE rl_run_core
)

rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "hhros2_motion_cores::WbcCore"
  EXECUTABLE wbc_core
)

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
  DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)
install(
  DIRECTORY model
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_observation_history
    test/test_observation_history.cpp
  )
  target_link_libraries(test_observation_history ${PROJECT_NAME})
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp 
  rclcpp_components 
  geometry_msgs 
  sensor_msgs 
  hhros2_interfaces 
  yaml-cpp 
  Eigen3 
  hhros2_log
)
ament_package()
