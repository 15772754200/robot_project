#include "robot_embeded_driver/hardware_driver.hpp"

#include "rclcpp/rclcpp.hpp"
#include "robot_embeded_interfaces/msg/spi_device.hpp"
#include "robot_embeded_interfaces/srv/configure_spi.hpp"
#include "robot_embeded_interfaces/srv/query_spi.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{
robot_embeded_interfaces::msg::SpiDevice toMsg(const robot_embeded::SpiState& state)
{
    robot_embeded_interfaces::msg::SpiDevice msg;
    msg.device = state.device;
    msg.ok = state.ok;
    msg.code = state.code;
    msg.message = state.message;
    msg.mode = state.mode;
    msg.bits_per_word = state.bits_per_word;
    msg.speed_hz = state.speed_hz;
    msg.lsb_first = state.lsb_first;
    return msg;
}
} // namespace

class RobotEmbededSpiNode : public rclcpp::Node
{
public:
    RobotEmbededSpiNode()
        : Node("robot_embeded_spi_node")
    {
        declareParameters();
        loadDefaultsFromParameters();

        configure_srv_ = create_service<robot_embeded_interfaces::srv::ConfigureSpi>(
            "configure_spi",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::ConfigureSpi::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::ConfigureSpi::Response> response) {
                handleConfigureSpi(request, response);
            });

        query_srv_ = create_service<robot_embeded_interfaces::srv::QuerySpi>(
            "query_spi",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::QuerySpi::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::QuerySpi::Response> response) {
                handleQuerySpi(request, response);
            });

        RCLCPP_INFO(get_logger(), "robot_embeded SPI node started");
    }

private:
    void declareParameters()
    {
        declare_parameter<std::string>("spi.device", "/dev/spidev0.0");
        declare_parameter<int>("spi.mode", 0);
        declare_parameter<int>("spi.speed_hz", 1000000);
        declare_parameter<int>("spi.bits_per_word", 8);
        declare_parameter<int>("spi.lsb_first", 0);
        declare_parameter<bool>("spi.verify", true);
        declare_parameter<std::string>("logs.directory", "/var/log/robot_embeded");
    }

    void loadDefaultsFromParameters()
    {
        const auto log_dir = get_parameter("logs.directory").as_string();
        spi_config_.device = get_parameter("spi.device").as_string();
        spi_config_.mode = static_cast<uint8_t>(std::clamp<int>(get_parameter("spi.mode").as_int(), 0, 255));
        spi_config_.speed_hz = static_cast<uint32_t>(std::max<int64_t>(0, get_parameter("spi.speed_hz").as_int()));
        spi_config_.bits_per_word =
            static_cast<uint8_t>(std::clamp<int>(get_parameter("spi.bits_per_word").as_int(), 0, 255));
        spi_config_.lsb_first = static_cast<uint8_t>(std::clamp<int>(get_parameter("spi.lsb_first").as_int(), 0, 255));
        spi_config_.verify = get_parameter("spi.verify").as_bool();
        spi_config_.log_file = log_dir + "/spi_configure.log";
    }

    void handleConfigureSpi(
        const std::shared_ptr<robot_embeded_interfaces::srv::ConfigureSpi::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::ConfigureSpi::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        auto config = spi_config_;
        if (!request->use_parameter_defaults)
        {
            config.device = request->device;
            config.mode = request->mode;
            config.bits_per_word = request->bits_per_word;
            config.speed_hz = request->speed_hz;
            config.lsb_first = request->lsb_first;
            config.verify = request->verify;
        }

        const auto result = driver_.configureSpi(config);
        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
    }

    void handleQuerySpi(
        const std::shared_ptr<robot_embeded_interfaces::srv::QuerySpi::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::QuerySpi::Response> response)
    {
        (void)request;
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        std::vector<robot_embeded::SpiState> devices;
        const auto result = driver_.querySpiDevices(devices);

        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
        response->devices.reserve(devices.size());
        for (const auto& device : devices)
        {
            response->devices.push_back(toMsg(device));
        }
    }

    robot_embeded::HardwareDriver driver_;
    std::mutex driver_mutex_;
    robot_embeded::SpiConfig spi_config_;

    rclcpp::Service<robot_embeded_interfaces::srv::ConfigureSpi>::SharedPtr configure_srv_;
    rclcpp::Service<robot_embeded_interfaces::srv::QuerySpi>::SharedPtr query_srv_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RobotEmbededSpiNode>());
    rclcpp::shutdown();
    return 0;
}
