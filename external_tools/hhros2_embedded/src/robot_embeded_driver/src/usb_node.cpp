#include "robot_embeded_driver/hardware_driver.hpp"

#include "rclcpp/rclcpp.hpp"
#include "robot_embeded_interfaces/msg/usb_device.hpp"
#include "robot_embeded_interfaces/srv/configure_usb_serial.hpp"
#include "robot_embeded_interfaces/srv/configure_usb_video.hpp"
#include "robot_embeded_interfaces/srv/query_usb_devices.hpp"
#include "robot_embeded_interfaces/srv/read_usb_device.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{
robot_embeded_interfaces::msg::UsbDevice toMsg(const robot_embeded::UsbDeviceInfo& info)
{
    robot_embeded_interfaces::msg::UsbDevice msg;
    msg.sysfs_path = info.sysfs_path;
    msg.devnode = info.devnode;
    msg.subsystem = info.subsystem;
    msg.driver = info.driver;
    msg.vendor_id = info.vendor_id;
    msg.product_id = info.product_id;
    msg.manufacturer = info.manufacturer;
    msg.product = info.product;
    msg.serial = info.serial;
    msg.busnum = info.busnum;
    msg.devnum = info.devnum;
    msg.interface_name = info.interface_name;
    msg.interface_class = info.interface_class;
    msg.interface_subclass = info.interface_subclass;
    msg.interface_protocol = info.interface_protocol;
    return msg;
}
} // namespace

class RobotEmbededUsbNode : public rclcpp::Node
{
public:
    RobotEmbededUsbNode()
        : Node("robot_embeded_usb_node")
    {
        declareParameters();
        loadDefaultsFromParameters();

        query_srv_ = create_service<robot_embeded_interfaces::srv::QueryUsbDevices>(
            "query_usb_devices",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::QueryUsbDevices::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::QueryUsbDevices::Response> response) {
                handleQueryUsbDevices(request, response);
            });

        read_srv_ = create_service<robot_embeded_interfaces::srv::ReadUsbDevice>(
            "read_usb_device",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::ReadUsbDevice::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::ReadUsbDevice::Response> response) {
                handleReadUsbDevice(request, response);
            });

        serial_srv_ = create_service<robot_embeded_interfaces::srv::ConfigureUsbSerial>(
            "configure_usb_serial",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbSerial::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbSerial::Response> response) {
                handleConfigureUsbSerial(request, response);
            });

        video_srv_ = create_service<robot_embeded_interfaces::srv::ConfigureUsbVideo>(
            "configure_usb_video",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbVideo::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbVideo::Response> response) {
                handleConfigureUsbVideo(request, response);
            });

        RCLCPP_INFO(get_logger(), "robot_embeded USB node started");
    }

private:
    void declareParameters()
    {
        declare_parameter<std::string>("usb.subsystem", "");
        declare_parameter<std::string>("usb.devnode_prefix", "");
        declare_parameter<bool>("usb.include_without_devnode", true);
        declare_parameter<std::string>("usb.read_devnode", "");
        declare_parameter<int>("usb.read_max_bytes", 256);
        declare_parameter<int>("usb.read_timeout_ms", 100);

        declare_parameter<std::string>("usb_serial.device", "/dev/ttyUSB0");
        declare_parameter<int>("usb_serial.baudrate", 115200);
        declare_parameter<int>("usb_serial.databits", 8);
        declare_parameter<std::string>("usb_serial.parity", "none");
        declare_parameter<int>("usb_serial.stopbits", 1);
        declare_parameter<bool>("usb_serial.rtscts", false);
        declare_parameter<bool>("usb_serial.verify", true);

        declare_parameter<std::string>("usb_video.device", "/dev/video0");
        declare_parameter<int>("usb_video.width", 640);
        declare_parameter<int>("usb_video.height", 480);
        declare_parameter<std::string>("usb_video.pixfmt", "YUYV");
        declare_parameter<int>("usb_video.fps", 30);
        declare_parameter<bool>("usb_video.verify", true);

        declare_parameter<std::string>("logs.directory", "/var/log/robot_embeded");
    }

    void loadDefaultsFromParameters()
    {
        const auto log_dir = get_parameter("logs.directory").as_string();

        usb_query_options_.subsystem = get_parameter("usb.subsystem").as_string();
        usb_query_options_.devnode_prefix = get_parameter("usb.devnode_prefix").as_string();
        usb_query_options_.include_without_devnode = get_parameter("usb.include_without_devnode").as_bool();
        usb_query_options_.log_file = log_dir + "/usb_query.log";

        usb_read_config_.devnode = get_parameter("usb.read_devnode").as_string();
        usb_read_config_.max_bytes =
            static_cast<uint32_t>(std::max<int64_t>(1, get_parameter("usb.read_max_bytes").as_int()));
        usb_read_config_.timeout_ms = get_parameter("usb.read_timeout_ms").as_int();
        usb_read_config_.log_file = log_dir + "/usb_read.log";

        usb_serial_config_.device = get_parameter("usb_serial.device").as_string();
        usb_serial_config_.baudrate = get_parameter("usb_serial.baudrate").as_int();
        usb_serial_config_.databits = get_parameter("usb_serial.databits").as_int();
        usb_serial_config_.parity = get_parameter("usb_serial.parity").as_string();
        usb_serial_config_.stopbits = get_parameter("usb_serial.stopbits").as_int();
        usb_serial_config_.rtscts = get_parameter("usb_serial.rtscts").as_bool();
        usb_serial_config_.verify = get_parameter("usb_serial.verify").as_bool();
        usb_serial_config_.log_file = log_dir + "/usb_configure.log";

        usb_video_config_.device = get_parameter("usb_video.device").as_string();
        usb_video_config_.width = static_cast<uint32_t>(get_parameter("usb_video.width").as_int());
        usb_video_config_.height = static_cast<uint32_t>(get_parameter("usb_video.height").as_int());
        usb_video_config_.pixfmt = get_parameter("usb_video.pixfmt").as_string();
        usb_video_config_.fps = static_cast<uint32_t>(get_parameter("usb_video.fps").as_int());
        usb_video_config_.verify = get_parameter("usb_video.verify").as_bool();
        usb_video_config_.log_file = log_dir + "/usb_configure.log";
    }

    void handleQueryUsbDevices(
        const std::shared_ptr<robot_embeded_interfaces::srv::QueryUsbDevices::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::QueryUsbDevices::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        auto options = usb_query_options_;
        if (!request->use_parameter_defaults)
        {
            options.subsystem = request->subsystem;
            options.devnode_prefix = request->devnode_prefix;
            options.include_without_devnode = request->include_without_devnode;
        }

        std::vector<robot_embeded::UsbDeviceInfo> devices;
        const auto result = driver_.queryUsbDevices(options, devices);
        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
        response->devices.reserve(devices.size());
        for (const auto& device : devices)
        {
            response->devices.push_back(toMsg(device));
        }
    }

    void handleReadUsbDevice(
        const std::shared_ptr<robot_embeded_interfaces::srv::ReadUsbDevice::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::ReadUsbDevice::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        auto config = usb_read_config_;
        if (!request->use_parameter_defaults)
        {
            config.devnode = request->devnode;
            config.max_bytes = request->max_bytes;
            config.timeout_ms = request->timeout_ms;
        }

        std::vector<uint8_t> data;
        const auto result = driver_.readUsbDevice(config, data);
        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
        response->data = data;
        response->bytes_read = static_cast<uint32_t>(data.size());
    }

    void handleConfigureUsbSerial(
        const std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbSerial::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbSerial::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        auto config = usb_serial_config_;
        if (!request->use_parameter_defaults)
        {
            config.device = request->device;
            config.baudrate = request->baudrate;
            config.databits = request->databits;
            config.parity = request->parity;
            config.stopbits = request->stopbits;
            config.rtscts = request->rtscts;
            config.verify = request->verify;
        }

        const auto result = driver_.configureUsbSerial(config);
        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
    }

    void handleConfigureUsbVideo(
        const std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbVideo::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::ConfigureUsbVideo::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        auto config = usb_video_config_;
        if (!request->use_parameter_defaults)
        {
            config.device = request->device;
            config.width = request->width;
            config.height = request->height;
            config.pixfmt = request->pixfmt;
            config.fps = request->fps;
            config.verify = request->verify;
        }

        const auto result = driver_.configureUsbVideo(config);
        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
    }

    robot_embeded::HardwareDriver driver_;
    std::mutex driver_mutex_;
    robot_embeded::UsbQueryOptions usb_query_options_;
    robot_embeded::UsbReadConfig usb_read_config_;
    robot_embeded::UsbSerialConfig usb_serial_config_;
    robot_embeded::UsbVideoConfig usb_video_config_;

    rclcpp::Service<robot_embeded_interfaces::srv::QueryUsbDevices>::SharedPtr query_srv_;
    rclcpp::Service<robot_embeded_interfaces::srv::ReadUsbDevice>::SharedPtr read_srv_;
    rclcpp::Service<robot_embeded_interfaces::srv::ConfigureUsbSerial>::SharedPtr serial_srv_;
    rclcpp::Service<robot_embeded_interfaces::srv::ConfigureUsbVideo>::SharedPtr video_srv_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RobotEmbededUsbNode>());
    rclcpp::shutdown();
    return 0;
}
