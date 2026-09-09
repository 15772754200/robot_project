cmake_minimum_required(VERSION 3.16)
project(hhros2_sim)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(yaml-cpp REQUIRED)

# MuJoCo is optional: when found we compile the real physics path (MujocoEngine), # tmny edit
# otherwise the plugin uses its built-in PD integrator fallback. Search the CMake
# package first, then the common ~/.mujoco and python-wheel install locations.
# 第一次部署时在/home/niic下创建mkdir .mujoco文件夹，然后 wget https://github.com/google-deepmind/mujoco/releases/download/3.3.7/mujoco-3.3.7-linux-aarch64.tar.gz
# 最后将压缩包解压到.mujoco文件夹下，解压后文件夹名为mujoco-3.3.7
find_package(mujoco QUIET)
if(NOT mujoco_FOUND)
  set(_mj_roots "$ENV{MUJOCO_ROOT}" "$ENV{HOME}/.mujoco/mujoco-3.3.7")
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" -c
        "import pathlib,mujoco;print(pathlib.Path(mujoco.__file__).resolve().parent)"
      OUTPUT_VARIABLE _mj_py ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_mj_py AND EXISTS "${_mj_py}")
      list(APPEND _mj_roots "${_mj_py}")
    endif()
  endif()
  find_path(MUJOCO_INCLUDE_DIR NAMES mujoco/mujoco.h HINTS ${_mj_roots}
    PATH_SUFFIXES include)
  find_library(MUJOCO_LIBRARY NAMES mujoco libmujoco.so HINTS ${_mj_roots}
    PATH_SUFFIXES lib)
  # The pip mujoco wheel ships only a versioned lib (libmujoco.so.3.x.y) directly # tmny edit
  # in its package root, which find_library does not match. Glob for it.
  if(NOT MUJOCO_LIBRARY)
    foreach(_r IN LISTS _mj_roots)
      if(_r AND EXISTS "${_r}")
        file(GLOB _mj_cand "${_r}/libmujoco.so*" "${_r}/lib/libmujoco.so*")
        if(_mj_cand)
          list(GET _mj_cand 0 _mj_found)
          set(MUJOCO_LIBRARY "${_mj_found}" CACHE FILEPATH
            "MuJoCo shared library" FORCE)
          break()
        endif()
      endif()
    endforeach()
  endif()
endif()

set(_sim_sources src/mujoco_system_hardware.cpp)
if(mujoco_FOUND OR (MUJOCO_INCLUDE_DIR AND MUJOCO_LIBRARY))
  list(APPEND _sim_sources src/mujoco_engine.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${_sim_sources})
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>"
)
ament_target_dependencies(${PROJECT_NAME}
  ament_index_cpp
  geometry_msgs
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
  std_msgs
  std_srvs
)

target_link_libraries(${PROJECT_NAME}
  yaml-cpp
)

if(mujoco_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HHROS2_HAVE_MUJOCO)
  target_link_libraries(${PROJECT_NAME} mujoco::mujoco)
  message(STATUS "hhros2_sim: building with MuJoCo physics (CMake package)")
elseif(MUJOCO_INCLUDE_DIR AND MUJOCO_LIBRARY)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HHROS2_HAVE_MUJOCO)
  target_include_directories(${PROJECT_NAME} PRIVATE ${MUJOCO_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${MUJOCO_LIBRARY})
  get_filename_component(_mj_libdir "${MUJOCO_LIBRARY}" DIRECTORY)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${_mj_libdir}" INSTALL_RPATH "${_mj_libdir}")
  message(STATUS "hhros2_sim: building with MuJoCo physics (${MUJOCO_LIBRARY})")
else()
  message(FATAL_ERROR
    "hhros2_sim requires MuJoCo; refusing to build a non-physical fallback")
endif()

pluginlib_export_plugin_description_file(
  hardware_interface hhros2_sim_plugins.xml)

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
  FILES hhros2_sim_plugins.xml
  DESTINATION share/${PROJECT_NAME}
)

# The viewer runs via `ros2 run hhros2_sim mujoco_state_viewer.py`.
# joint_model_config.py ships alongside as its sibling module.
install(PROGRAMS scripts/mujoco_state_viewer.py DESTINATION lib/${PROJECT_NAME})
install(FILES scripts/joint_model_config.py DESTINATION lib/${PROJECT_NAME})

install(
  DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_add_pytest_test(
    test_mujoco_state_viewer
    test/test_mujoco_state_viewer.py
    TIMEOUT 30
  )
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  ament_index_cpp geometry_msgs hardware_interface pluginlib rclcpp
  rclcpp_lifecycle std_msgs std_srvs yaml-cpp)
ament_package()
