实现rgb相机分辨率/帧数设置启动，进行拍照/视频录制等操作，确认采集参数发送给系统，进行正常视频流是否正常工作的检测，以及校验参数的合法性等工作
同时将记录写进日志供读取。
需要先安装一些依赖
pip install opencv-python
sudo apt-get install -y cheese
sudo apt-get install -y v4l-utils
sudo apt-get install -y ffmpeg

系统启动脚本：
python RGB_sys.py