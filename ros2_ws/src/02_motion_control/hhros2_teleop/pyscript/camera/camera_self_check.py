import pyrealsense2 as rs
import numpy as np
import cv2
import sys

# 初次使用时，会提示错误，手动安装下面两个模块
# pip3 install --upgrade pip
# pip3 install pyrealsense2

def test_realsense_device():
    print("初次使用时，会提示错误，手动安装下面两个模块")
    print("pip3 install --upgrade pip")
    print("pip3 install pyrealsense2")
    print("🔍 检查 RealSense 设备...")
    ctx = rs.context()
    devices = ctx.query_devices()

    if len(devices) == 0:
        print("❌ 未检测到任何 RealSense 设备，请检查连接。")
        sys.exit(1)

    dev = devices[0]
    print(f"✅ 检测到设备: {dev.get_info(rs.camera_info.name)}")
    print(f"   序列号: {dev.get_info(rs.camera_info.serial_number)}")

    # ---- 检查传感器 ----
    sensors = dev.query_sensors()
    rgb_sensor = None
    depth_sensor = None
    infrared_sensor = None

    for s in sensors:
        if s.is_depth_sensor():
            depth_sensor = s
        elif s.is_color_sensor():
            rgb_sensor = s
        else:
            # 部分型号红外传感器不会明确标为 color/depth
            name = s.get_info(rs.camera_info.name)
            if "Infrared" in name or "Stereo" in name:
                infrared_sensor = s

    # 打印检测到的传感器
    if depth_sensor:
        print("🌊 检测到深度相机传感器。")
    else:
        print("⚠️ 未检测到深度相机传感器。")

    if rgb_sensor:
        print("📸 检测到 RGB 相机传感器。")
    else:
        print("⚠️ 未检测到 RGB 相机传感器。")


    # ---- 配置并启动管线 ----
    pipeline = rs.pipeline()
    config = rs.config()
    config.enable_device(dev.get_info(rs.camera_info.serial_number))

    if rgb_sensor:
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
    if depth_sensor:
        config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
    if depth_sensor:
        try:
            config.enable_stream(rs.stream.infrared, 1, 640, 480, rs.format.y8, 30)
            config.enable_stream(rs.stream.infrared, 2, 640, 480, rs.format.y8, 30)
            print("🌌 尝试启用红外相机流 (由深度模块提供)...")
        except Exception as e:
            print(f"⚠️ 无法启用红外流: {e}")

    try:
        print("\n▶ 启动相机流测试...")
        pipeline.start(config)

        # 预热几帧
        for _ in range(30):
            pipeline.wait_for_frames()

        frames = pipeline.wait_for_frames()
        color_frame = frames.get_color_frame() if rgb_sensor else None
        depth_frame = frames.get_depth_frame() if depth_sensor else None
        infrared_frame1 = frames.get_infrared_frame(1)
        infrared_frame2 = frames.get_infrared_frame(2)


        print("\n📋 测试结果:")

        if color_frame:
            color_image = np.asanyarray(color_frame.get_data())
            print(f"✅ RGB 相机工作正常，分辨率: {color_image.shape[1]}x{color_image.shape[0]}")
        else:
            print("❌ 未获取到 RGB 帧。")

        if depth_frame:
            depth_image = np.asanyarray(depth_frame.get_data())
            if np.mean(depth_image) > 0:
                print(f"✅ 深度相机工作正常，分辨率: {depth_image.shape[1]}x{depth_image.shape[0]}")
            else:
                print("⚠️ 深度帧有效但数据全为0。")
        else:
            print("❌ 未获取到深度帧。")

        if infrared_frame1 is not None:
            ir1 = np.asanyarray(infrared_frame1.get_data())
            print(f"✅ 红外相机 1 工作正常，分辨率: {ir1.shape[1]}x{ir1.shape[0]}")
        else:
            print("❌ 未获取到红外相机 1 帧。")

        if infrared_frame2 is not None:
            ir2 = np.asanyarray(infrared_frame2.get_data())
            print(f"✅ 红外相机 2 工作正常，分辨率: {ir2.shape[1]}x{ir2.shape[0]}")
        else:
            print("❌ 未获取到红外相机 2 帧。")

    except Exception as e:
        print(f"❌ 测试过程中出错: {e}")
    finally:
        pipeline.stop()
        print("\n✅ 测试结束。")

if __name__ == "__main__":
    test_realsense_device()
