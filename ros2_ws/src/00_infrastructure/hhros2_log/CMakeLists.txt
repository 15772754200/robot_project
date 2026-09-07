cmake_minimum_required(VERSION 3.16)
project(hhros2_log)

find_package(ament_cmake REQUIRED)

# 创建共享库
add_library(${PROJECT_NAME} SHARED
  src/log.cpp
  src/imu_log.cpp
)
# 包含头文件路径（让外部使用 #include "hhros2_log/log.h" 或 "hhros2_log/imu_log.h"）
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME}
  ${nlohmann_json_LIBRARIES}
  pthread
)

# 安装头文件（保留 include/ 下的目录结构）
install(DIRECTORY include/
  DESTINATION include
)

# 安装库文件
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}_export
  DESTINATION lib
)

# 导出目标以便其他包 find_package
ament_export_targets(${PROJECT_NAME}_export HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(nlohmann_json)

# 最终打包
ament_package()