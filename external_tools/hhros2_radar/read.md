

服务端执行
```sh
colcon build
source install/setup.bash
ros2 launch rslidar_sdk start.py
```


需要通过有线连接客户机
服务端  ./bashrc
export ROS_DOMAIN_ID=0
export ROS_LOCALHOST_ONLY=0
export ROS_IP=192.168.1.3  服务端网络接口

客户端
export ROS_DOMAIN_ID=0
export ROS_LOCALHOST_ONLY=0
export ROS_IP=192.168.1.30  客户端网络接口
在客户端直接启动rviz2

