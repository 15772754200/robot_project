cmake_minimum_required(VERSION 3.16)
project(hhros2_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  # --- Control data plane ---
  "msg/JointMotor.msg"
  "msg/BaseState.msg"
  # --- System governance (one-horizontal: hhros2_core) ---
  "msg/SafetyStatus.msg"
  "msg/SystemHealth.msg"
  "msg/Heartbeat.msg"
  "msg/ArbitrationMode.msg"
  # --- Services ---
  "srv/SetControlMode.srv"
  "srv/SetSystemState.srv"
  # --- Actions ---
  "action/SetGait.action"
  "action/ExecuteMotion.action"
  DEPENDENCIES std_msgs geometry_msgs builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
