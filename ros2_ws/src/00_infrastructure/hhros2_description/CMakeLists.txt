cmake_minimum_required(VERSION 3.16)
project(hhros2_description)

find_package(ament_cmake REQUIRED)

install(
  DIRECTORY urdf config launch meshes models
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(
    joint_contract
    test/test_joint_contract.py
  )
endif()

ament_package()
