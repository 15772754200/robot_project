cmake_minimum_required(VERSION 3.16)
project(hhros_bringup)

find_package(ament_cmake REQUIRED)

install(
  DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(qos_contract test/test_qos_contract.py)
endif()

ament_package()
