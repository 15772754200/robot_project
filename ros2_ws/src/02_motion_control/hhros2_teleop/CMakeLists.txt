cmake_minimum_required(VERSION 3.8)
project(hhros2_teleop)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)          # 生命周期节点依赖
find_package(geometry_msgs REQUIRED)
find_package(hhros2_interfaces REQUIRED)
find_package(ament_index_cpp REQUIRED)           # 用于获取包路径
find_package(hhros2_log REQUIRED) # 日志管理包

# 添加可执行文件（包含所有源文件）
add_executable(key_publisher
  src/keyboard_cmd/key_publisher.cpp
  src/keyboard_cmd/keyboard.cpp
  src/keyboard_cmd/robotServer.cpp
  src/keyboard_cmd/cmdCall.cpp
)
target_include_directories(key_publisher PRIVATE include)

# 链接依赖
ament_target_dependencies(key_publisher
  rclcpp
  rclcpp_lifecycle
  geometry_msgs
  hhros2_interfaces
  ament_index_cpp
  hhros2_log
)

# 链接 uuid 库（用于 generate_uuid）
target_link_libraries(key_publisher uuid)

# 安装可执行文件
install(TARGETS key_publisher DESTINATION lib/${PROJECT_NAME})

# 安装 launch 文件
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

# 安装 pyscript 脚本目录（保持可执行权限）
install(DIRECTORY pyscript DESTINATION share/${PROJECT_NAME}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
