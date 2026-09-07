set(TI5_ENCOS_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)
set(TI5_ENCOS_PUBLIC_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
set(TI5_ENCOS_INTERNAL_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(
  ti5_encos_3master_protocol STATIC
  encos/encos_command_builder.cpp
  encos/encos_feedback_parser.cpp
  io/io_data_access.cpp
  ipc/ipc_motor_message_codec.cpp
  protocol/motor_protocol_conversions.cpp
  runtime/runtime_state_adapter.cpp
)

add_library(ti5_encos_3master::protocol ALIAS ti5_encos_3master_protocol)

target_include_directories(
  ti5_encos_3master_protocol PUBLIC
    "$<BUILD_INTERFACE:${TI5_ENCOS_PUBLIC_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:include>"
  PRIVATE
    "${TI5_ENCOS_INTERNAL_INCLUDE_DIR}"
)

ament_target_dependencies(
  ti5_encos_3master_protocol PUBLIC
    hhros2_motor_protocol
    hhros2_motor_diagnostics
    hhros2_log
)

target_link_libraries(
  ti5_encos_3master_protocol PRIVATE
    spdlog::spdlog
)

install(
  TARGETS ti5_encos_3master_protocol
  EXPORT export_ti5_encos_3master_protocol
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  DIRECTORY
    ${TI5_ENCOS_PUBLIC_INCLUDE_DIR}/hhros2_motor_runtime
  DESTINATION include
)

if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
  set(TI5_ENCOS_VENDOR_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/niic_ethercat/niic_ethercat_demo_4.1.3/include
  )

  add_library(
    ti5_encos_3master_runtime STATIC
    encos/encos_io_controller.cpp
    ethercat/cia402_power_controller.cpp
    ethercat/ethercat_master_controller.cpp
    ethercat/ethercat_probe_recorder.cpp
    ethercat/ti5_motor_command_adapter.cpp
    ipc/ipc_motor_diag_sampler.cpp
    ipc/ipc_motor_server.cpp
    runtime/three_master_ti5_encos_runtime.cpp
  )

  add_library(ti5_encos_3master::runtime ALIAS ti5_encos_3master_runtime)

  target_include_directories(
    ti5_encos_3master_runtime PUBLIC
      "$<BUILD_INTERFACE:${TI5_ENCOS_PUBLIC_INCLUDE_DIR}>"
      "$<INSTALL_INTERFACE:include>"
    PRIVATE
      "${TI5_ENCOS_INTERNAL_INCLUDE_DIR}"
  )

  ament_target_dependencies(
    ti5_encos_3master_runtime PUBLIC
      hhros2_motor_diagnostics
  )

  target_compile_definitions(
    ti5_encos_3master_runtime PUBLIC
      TI5_ENCOS_CONFIG_DIR="${TI5_ENCOS_CONFIG_DIR}"
      ROBOT_MOTOR_ENABLE_SINGLE_SHOT_PROBE=$<IF:$<BOOL:${ROBOT_MOTOR_ENABLE_SINGLE_SHOT_PROBE}>,1,0>
  )

  target_link_libraries(
    ti5_encos_3master_runtime PUBLIC
      ti5_encos_3master::protocol
      ament_index_cpp::ament_index_cpp
      ethercat-cpp::ethercat-cpp
      Necro::qiuniu
    PRIVATE
      spdlog::spdlog
  )

  install(
    TARGETS ti5_encos_3master_runtime
    EXPORT export_ti5_encos_3master_runtime
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )

  add_executable(
    ti5_encos_3master_demo
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/ti5_encos_3master_demo.cpp
  )

  target_link_libraries(
    ti5_encos_3master_demo PRIVATE
      ti5_encos_3master::runtime
      Boost::program_options
  )

  install(
    TARGETS ti5_encos_3master_demo
    DESTINATION lib/${PROJECT_NAME}
  )
endif()

if(BUILD_TESTING)
  include(CTest)

  set(TI5_ENCOS_TEST_INCLUDE_DIRS
    "${TI5_ENCOS_INTERNAL_INCLUDE_DIR}"
    "${TI5_ENCOS_PUBLIC_INCLUDE_DIR}"
  )

  add_executable(
    encos_feedback_parser_test
    test/encos_feedback_parser_test.cpp
  )

  target_include_directories(
    encos_feedback_parser_test PRIVATE
      ${TI5_ENCOS_TEST_INCLUDE_DIRS}
  )

  target_link_libraries(
    encos_feedback_parser_test PRIVATE
      ti5_encos_3master::protocol
  )

  add_test(
    NAME encos_feedback_parser_test
    COMMAND encos_feedback_parser_test
  )

  add_executable(
    encos_command_builder_test
    test/encos_command_builder_test.cpp
  )

  target_include_directories(
    encos_command_builder_test PRIVATE
      ${TI5_ENCOS_TEST_INCLUDE_DIRS}
  )

  target_link_libraries(
    encos_command_builder_test PRIVATE
      ti5_encos_3master::protocol
  )

  add_test(
    NAME encos_command_builder_test
    COMMAND encos_command_builder_test
  )

  add_executable(
    ipc_motor_message_codec_test
    test/ipc_motor_message_codec_test.cpp
  )

  target_include_directories(
    ipc_motor_message_codec_test PRIVATE
      ${TI5_ENCOS_TEST_INCLUDE_DIRS}
  )

  target_link_libraries(
    ipc_motor_message_codec_test PRIVATE
      ti5_encos_3master::protocol
  )

  add_test(
    NAME ipc_motor_message_codec_test
    COMMAND ipc_motor_message_codec_test
  )

  add_executable(
    io_data_access_test
    test/io_data_access_test.cpp
  )

  target_include_directories(
    io_data_access_test PRIVATE
      ${TI5_ENCOS_TEST_INCLUDE_DIRS}
  )

  target_link_libraries(
    io_data_access_test PRIVATE
      ti5_encos_3master::protocol
  )

  add_test(
    NAME io_data_access_test
    COMMAND io_data_access_test
  )

  add_executable(
    runtime_state_adapter_test
    test/runtime_state_adapter_test.cpp
  )

  target_include_directories(
    runtime_state_adapter_test PRIVATE
      ${TI5_ENCOS_TEST_INCLUDE_DIRS}
  )

  target_link_libraries(
    runtime_state_adapter_test PRIVATE
      ti5_encos_3master::protocol
  )

  add_test(
    NAME runtime_state_adapter_test
    COMMAND runtime_state_adapter_test
  )

  add_executable(
    joint_wiring_map_test
    test/joint_wiring_map_test.cpp
  )

  target_include_directories(
    joint_wiring_map_test PRIVATE
      ${TI5_ENCOS_TEST_INCLUDE_DIRS}
  )

  target_link_libraries(
    joint_wiring_map_test PRIVATE
      ti5_encos_3master::protocol
  )

  add_test(
    NAME joint_wiring_map_test
    COMMAND joint_wiring_map_test
  )

  if(ROBOT_MOTOR_BUILD_HARDWARE_RUNTIME)
    list(APPEND TI5_ENCOS_TEST_INCLUDE_DIRS "${TI5_ENCOS_VENDOR_INCLUDE_DIR}")

    add_executable(
      ti5_motor_command_adapter_test
      test/ti5_motor_command_adapter_test.cpp
      ethercat/ti5_motor_command_adapter.cpp
    )

    target_include_directories(
      ti5_motor_command_adapter_test PRIVATE
        ${TI5_ENCOS_TEST_INCLUDE_DIRS}
    )

    target_link_libraries(
      ti5_motor_command_adapter_test PRIVATE
        ti5_encos_3master::protocol
        ethercat-cpp::ethercat-cpp
    )

    add_test(
      NAME ti5_motor_command_adapter_test
      COMMAND ti5_motor_command_adapter_test
    )
  endif()
endif()
