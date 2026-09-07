from setuptools import find_packages, setup

package_name = "hhros2_behavior"

setup(
    name=package_name,
    version="0.1.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages",
            ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="robot_platform maintainers",
    maintainer_email="robotics@example.com",
    description="Behavior/task placeholder (BehaviorTree.CPP + VLM interface).",
    license="Proprietary",
    entry_points={
        "console_scripts": [
            "behavior_node = hhros2_behavior.behavior_node:main",
            "keyboard_teleop_node = hhros2_behavior.keyboard_teleop_node:main",
            "joy_teleop_node = hhros2_behavior.joy_teleop_node:main",
        ],
    },
)
