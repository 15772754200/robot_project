D435i相机SDK与ros2配置手册：
1、librealsense SDK 安装：
sudo mkdir -p /etc/apt/keyrings
curl -sSf https://librealsense.intel.com/Debian/librealsense.pgp | sudo tee /etc/apt/keyrings/librealsense.pgp > /dev/null
# Make sure apt HTTPS support is installed: sudo apt-get install apt-transport-https

# Add the server to the list of repositories:

echo "deb [signed-by=/etc/apt/keyrings/librealsense.pgp] https://librealsense.intel.com/Debian/apt-repo `lsb_release -cs` main" | \
# 在下方输入下面指令
sudo tee /etc/apt/sources.list.d/librealsense.list


sudo apt-get update
# Install the libraries (see section below if upgrading packages):
sudo apt-get install librealsense2-dkms
sudo apt-get install librealsense2-utils
# The above two lines will deploy librealsense2 udev rules, build and activate kernel modules, runtime library and executable demos and tools.
# Optionally install the developer and debug packages:
sudo apt-get install librealsense2-dev
sudo apt-get install librealsense2-dbg

# 测试是否安装成功
realsense-viewer

2、realsense ros2 安装 和 使用
# 源码安装realsense ros2
mkdir -p ./ros2_ws/src
cd ./ros2_ws/src
git clone https://github.com/IntelRealSense/realsense-ros.git -b ros2-master
cd ../
# 编译（先进入本项目环境）
rosenv
sudo rosdepc init

rosdepc update
rosdepc install -i --from-path src --rosdistro $ROS_DISTRO --skip-keys=librealsense2 -y
colcon build
# 加载 RealSense 工作空间覆盖层
source install/setup.bash

# 2. 进入本项目唯一的 ROS 环境
rosenv

# 3. 测试功能
ros2 launch realsense2_camera rs_launch.py
# 之后进行ros2 topic list 如果出现了相机有关的话题说明相机成功启动
# 相机设置参数使用命令举例，这里以相机分辨率设置为640*360为例
source install/setup.bash
ros2 launch realsense2_camera rs_align_depth_launch.py depth_module.depth_profile:=640x360x30 rgb_camera.color_profile:=640x360x30

3、相机根据参数进行启动的脚本
# 根据具体需要打开的相机类型、分辨率、帧数、是否需要打开点云进行设置,并且将行为写进了日志。脚本会在选择完需要的参数之后给出相应的相机启动指令。
./start.sh

4、拍照以及录屏脚本
# 能够进行拍照或者录屏功能的选择，可以选择相机类型、分辨率以及需要的录制时间。
# 该脚本需要在启动相机节点之后进行运行，脚本会订阅相应的话题来获取数据并进行记录。
python3 record.py

5、深度相机日志读取脚本
# 能够读取日志文件夹内时间最近的相机使用日志文件，打印出相机的使用信息。
python3 camera_log_read.py

6、相机自检脚本
# 在没有启动相机节点前进行检测，检测相机设备是否连接，启动相机管道进行测试，后反馈检测结果并且写进日志。
python3 camera_test.py

7、相机的开发接口设置脚本与接口反馈界面
# 能够对深度相机的一些开发接口进行设置，并且对一些接口数据话题信息进行选择与打印，要在运行相机节点之后再运行此代码。
1）运行 `rosenv`
2）python3 camera_menu.py

8、相机统一功能代码启动脚本，并且可以选择功能，注意在开启相机时需要先启动节点，再另开终端启动统一功能脚本再开其他功能。
python realsense_toolbox_menu.py

9、注意事项：
（1）使用相机脚本前先运行 `rosenv`，不要在同一终端加载 `/opt/ros`。
（2）如果硬件连接了但是没有检测到，需要对相机进行重新连接，需要USB3.0的数据线
（3）深度相机的ros2包已在src文件夹中，需要可以直接复制。

相关话题信息（仅供参考，话题命名相似）
# rgb图像
/camera/color/image_raw
/camera/color/camera_info
/camera/color/metadata
# 深度图像
/camera/depth/image_rect_rawS
/camera/depth/camera_infoS
/camera/depth/metadata
/camera/aligned_depth_to_color/image_raw
/camera/aligned_depth_to_color/camera_info
# 红外（IR）流
/camera/infra1/image_rect_raw
/camera/infra1/camera_info
/camera/infra1/metadata
/camera/infra2/image_rect_raw
/camera/infra2/camera_info
/camera/infra2/metadata
# 点云
/camera/pointcloud
# imu
/camera/accel/sample
/camera/gyro/sample
# TF
/tf
/tf_static
