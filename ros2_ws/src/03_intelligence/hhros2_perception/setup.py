from setuptools import find_packages, setup

package_name = "hhros2_perception"

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
    description="Perception/planning placeholder (Nav2 / occupancy interface).",
    license="Proprietary",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "perception_node = hhros2_perception.perception_node:main",
        ],
    },
)
