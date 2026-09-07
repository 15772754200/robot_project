from setuptools import setup, find_packages
import os
from glob import glob

package_name = 'wave_control_system'

# 准备data_files列表
data_files = [
    ('share/ament_index/resource_index/packages',
        ['resource/' + package_name]),
    ('share/' + package_name, ['package.xml']),
]

# 只有当launch目录存在且有文件时才添加
launch_files = glob('launch/*.launch.py')
if launch_files:
    data_files.append(
        (os.path.join('share', package_name, 'launch'), launch_files)
    )

# 只有当config目录存在且有文件时才添加
config_files = glob('config/*.yaml')
if config_files:
    data_files.append(
        (os.path.join('share', package_name, 'config'), config_files)
    )

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=data_files,
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='your_name',
    maintainer_email='your_email@example.com',
    description='Robot wave control system',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'wave_controller = wave_control_system.wave_controller:main',
            'wave_client = wave_control_system.wave_client:main',
            'motor_driver_sim = wave_control_system.motor_driver_sim:main',
            'wave_gui = wave_control_system.wave_gui:main',
            'teach_manager = wave_control_system.teach_manager:main',
            'teach_manager_real = wave_control_system.teach_manager_real:main',
            'wave_example = wave_control_system.wave_example:main',
            'test_teach_function = wave_control_system.test_teach_function:main',
            'test_teach_real = wave_control_system.test_teach_real:main',
            'test_playback = wave_control_system.test_playback:main',
            'generate_sine_trajectory = wave_control_system.generate_sine_trajectory:main',
        ],
    },
)
