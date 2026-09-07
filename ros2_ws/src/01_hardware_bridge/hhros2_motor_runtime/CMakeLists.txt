# 这是一个 ROS 2 电机运行时包的顶层构建脚本，
# 它负责检查 Necro SDK、加载 NIIC EtherCAT SDK 中的 ethercat-cpp、编译实际电机 runtime 子模块，
# 并把运行所需的配置文件和 EtherCAT 动态库安装到 ROS 2 install 环境中。

cmake_minimum_required(VERSION 3.16)  # 所需的最低 CMake 版本
project(hhros2_motor_runtime)  # 定义 ROS 2 包的名称

find_package(ament_cmake REQUIRED)  # 构建系统依赖
find_package(hhros2_motor_protocol REQUIRED)  # 电机 IPC wire format 纯协议契约
find_package(hhros2_motor_diagnostics REQUIRED)  # 负责电机诊断、状态检查的 ROS 2 包
find_package(spdlog REQUIRED) # 用于高性能日志记录的 spdlog 库
find_package(hhros2_log REQUIRED) # 日志管理包
# hhros2_mechanisms
find_package(hhros2_mechanisms REQUIRED)
find_package(Eigen3 REQUIRED)   

if(BUILD_TESTING) # 如果开启了测试构建
  include(CTest)
endif()

option(ROBOT_MOTOR_ENABLE_SINGLE_SHOT_PROBE
  "Enable shared-memory one-shot latency probe instrumentation"
  ON
)

option(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME
  "Build the EtherCAT hardware runtime that requires Necro and ethercat-cpp"
  OFF
)

set(NIIC_ETHERCAT_SDK_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/niic_ethercat/niic_ethercat_demo_4.1.3
)

if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  find_package(ament_index_cpp REQUIRED)  # 常用于在C++代码中查找ros2包的安装路径
  find_package(Boost REQUIRED COMPONENTS program_options) # 用于解析命令行参数的 Boost 库组件
  find_package(Necro QUIET) # 找不到Necro时先不要立刻报错，因为我们会在后面检查它是否存在
  find_package(fmt REQUIRED)
    
  if(NOT Necro_FOUND)
    message(FATAL_ERROR
      "Necro SDK was not found. hhros2_motor_runtime requires Necro before the "
      "bundled ethercat-cpp package can configure.\n"
      "Set one of the following before building:\n"
      "  export Necro_DIR=/path/to/directory/containing/NecroConfig.cmake\n"
      "  export CMAKE_PREFIX_PATH=/path/to/necro/install:$CMAKE_PREFIX_PATH\n"
      "Or disable the hardware runtime for mock/protocol tests:\n"
      "  -DROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=OFF"
    )
  endif()

  if(NOT TARGET ethercat-cpp::ethercat-cpp)
    find_package(
      ethercat-cpp
      REQUIRED
      PATHS
        "${NIIC_ETHERCAT_SDK_DIR}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
  endif()
else()
  message(STATUS
    "ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME=OFF: skipping Necro, "
    "ethercat-cpp, EtherCAT runtime, and demo targets."
  )
endif()

add_subdirectory(src/ti5_encos_3master)

install(
  DIRECTORY src/ti5_encos_3master/config
  DESTINATION share/${PROJECT_NAME}
)

# ===== hhros2 L0 shared-memory runtime (NO ROS; depends only on the protocol ABI) =====  # tmny edit
# Standalone hard real-time process that owns the bus and exposes 23 joints + IMU  # tmny edit
# through hhros2 shared memory. Builds regardless of the EtherCAT vendor SDK.  # tmny edit
find_package(Threads REQUIRED)

# 定义 L0 公共源文件
set(L0_COMMON_SOURCES
  tools/hhros2_l0_runtime_main.cpp
  src/hhros2_l0/hhros2_l0_shm_server.cpp
)

# 定义硬件运行时相关源文件（有条件）
set(L0_EXTRA_SOURCES)
if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  list(APPEND L0_EXTRA_SOURCES src/hhros2_l0/hhros2_ecat_l0_backend.cpp)
endif()

add_executable(hhros2_l0_runtime ${L0_COMMON_SOURCES} ${L0_EXTRA_SOURCES})

target_include_directories(hhros2_l0_runtime PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/ti5_encos_3master"
)
ament_target_dependencies(
  hhros2_l0_runtime
  hhros2_motor_protocol
  hhros2_motor_diagnostics
  hhros2_log
  hhros2_mechanisms
  Eigen3
)
target_link_libraries(hhros2_l0_runtime Threads::Threads)
if(UNIX AND NOT APPLE)
  target_link_libraries(hhros2_l0_runtime rt)  # POSIX shm_open / shm_unlink  # tmny edit
endif()

if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  # 添加宏定义，让 C++ 代码知道硬件后端可用
  target_compile_definitions(hhros2_l0_runtime PRIVATE HHROS2_HAVE_ECAT_BACKEND)

  target_link_libraries(hhros2_l0_runtime
    ti5_encos_3master_runtime          # 硬件运行时库
    ti5_encos_3master_protocol         # 协议库
    ethercat-cpp::ethercat-cpp         # EtherCAT SDK
    ${Boost_LIBRARIES}                 # Boost program_options
    fmt::fmt                           # fmt 库
  )
  
  message(STATUS "Hardware runtime ENABLED for hhros2_l0_runtime")
endif()

install(
  TARGETS hhros2_l0_runtime
  DESTINATION lib/${PROJECT_NAME}
)

# ethercat-cpp is shipped as part of the NIIC SDK vendor bundle.  The CMake
# imported target is enough for linking, but the runtime loader also needs the
# shared object in an installed library path after `source install/setup.bash`.
if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  install(
    DIRECTORY "${NIIC_ETHERCAT_SDK_DIR}/lib/"
    DESTINATION lib
    FILES_MATCHING
      PATTERN "libethercat-cpp.so*"
  )

  install(
    DIRECTORY "${NIIC_ETHERCAT_SDK_DIR}/lib/cmake/ethercat-cpp"
    DESTINATION lib/cmake
  )

  install(
    DIRECTORY "${NIIC_ETHERCAT_SDK_DIR}/include/"
    DESTINATION include
  )
endif()

ament_export_targets(export_ti5_encos_3master_protocol HAS_LIBRARY_TARGET)

if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  ament_export_targets(export_ti5_encos_3master_runtime HAS_LIBRARY_TARGET)
endif()

ament_export_dependencies(
  hhros2_motor_protocol
  hhros2_motor_diagnostics
  hhros2_log
  hhros2_mechanisms
  Eigen3
)

if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  ament_export_dependencies(
    ament_index_cpp
    ethercat-cpp
    Necro
  )
endif()

ament_package()
