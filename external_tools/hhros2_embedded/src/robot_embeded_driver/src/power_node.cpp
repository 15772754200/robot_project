#include "robot_embeded_driver/hardware_driver.hpp"

#include "rclcpp/rclcpp.hpp"
#include "robot_embeded_interfaces/srv/send_power_control.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

namespace
{
bool parseFanPwmValues(const std::string& text, std::array<uint8_t, 6>& pwm_values)
{
    if (text == "fan-control")
    {
        return true;
    }

    const auto delimiter = text.find_first_of(":=");
    if (delimiter == std::string::npos || text.substr(0, delimiter) != "fan-control")
    {
        return false;
    }

    std::array<uint8_t, 6> parsed{};
    std::stringstream stream(text.substr(delimiter + 1));
    std::string item;
    size_t index = 0;
    while (std::getline(stream, item, ','))
    {
        if (index >= parsed.size() || item.empty())
        {
            return false;
        }

        try
        {
            size_t consumed = 0;
            const int value = std::stoi(item, &consumed, 10);
            if (consumed != item.size() || value < 0 || value > 100)
            {
                return false;
            }
            parsed[index++] = static_cast<uint8_t>(value);
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    if (index != parsed.size())
    {
        return false;
    }

    pwm_values = parsed;
    return true;
}

bool parsePowerCommand(
    const std::string& text,
    robot_embeded::PowerControlCommand& command,
    std::array<uint8_t, 6>& fan_pwm_values)
{
    const std::pair<const char*, robot_embeded::PowerControlCommand> commands[] = {
        {"open-limbs-power", robot_embeded::PowerControlCommand::OpenLimbs},
        {"open-upper-limbs-power", robot_embeded::PowerControlCommand::OpenUpperLimbs},
        {"open-lower-limbs-power", robot_embeded::PowerControlCommand::OpenLowerLimbs},
        {"close-limbs-power", robot_embeded::PowerControlCommand::CloseLimbs},
        {"open-dexterous-hands-power", robot_embeded::PowerControlCommand::OpenDexterousHands},
        {"close-dexterous-hands-power", robot_embeded::PowerControlCommand::CloseDexterousHands},
    };

    for (const auto& item : commands)
    {
        if (text == item.first)
        {
            command = item.second;
            return true;
        }
    }

    if (parseFanPwmValues(text, fan_pwm_values))
    {
        command = robot_embeded::PowerControlCommand::FanControl;
        return true;
    }

    return false;
}
} // namespace

class RobotEmbededPowerNode : public rclcpp::Node
{
public:
    RobotEmbededPowerNode()
        : Node("robot_embeded_power_node")
    {
        declareParameters();
        loadDefaultsFromParameters();

        control_srv_ = create_service<robot_embeded_interfaces::srv::SendPowerControl>(
            "send_power_control",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::SendPowerControl::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::SendPowerControl::Response> response) {
                handleSendPowerControl(request, response);
            });

        RCLCPP_INFO(get_logger(), "robot_embeded power node started");
    }

private:
    void declareParameters()
    {
        declare_parameter<std::string>("power.device", "/dev/ttyTHS2");
        declare_parameter<int>("power.baudrate", 115200);
        declare_parameter<int>("power.response_timeout_ms", 1500);
        declare_parameter<int>("power.rs485_tx_value", 0);
        declare_parameter<int>("power.rs485_rx_value", 1);
        declare_parameter<int>("power.rs485_id", 1);
        declare_parameter<std::string>("logs.directory", "/var/log/robot_embeded");
    }

    void loadDefaultsFromParameters()
    {
        const auto log_dir = get_parameter("logs.directory").as_string();
        power_config_.device = get_parameter("power.device").as_string();
        power_config_.baudrate = get_parameter("power.baudrate").as_int();
        power_config_.response_timeout_ms = get_parameter("power.response_timeout_ms").as_int();
        power_config_.rs485_tx_value = get_parameter("power.rs485_tx_value").as_int();
        power_config_.rs485_rx_value = get_parameter("power.rs485_rx_value").as_int();
        power_config_.rs485_id = static_cast<uint8_t>(
            std::clamp<int64_t>(get_parameter("power.rs485_id").as_int(), 0, 255));
        power_config_.log_file = log_dir + "/power.log";
    }

    void handleSendPowerControl(
        const std::shared_ptr<robot_embeded_interfaces::srv::SendPowerControl::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::SendPowerControl::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        robot_embeded::PowerControlCommand command;
        auto fan_pwm_values = power_config_.fan_pwm_percent;
        if (!parsePowerCommand(request->command, command, fan_pwm_values))
        {
            response->success = false;
            response->command_code = 0;
            response->result = 0;
            response->code = 2;
            response->message = "unknown power control command: " + request->command +
                "; fan command format: fan-control or fan-control:10,20,30,50,70,80";
            return;
        }

        auto config = power_config_;
        config.fan_pwm_percent = fan_pwm_values;
        if (!request->use_parameter_defaults)
        {
            config.device = request->device;
            config.baudrate = request->baudrate;
            config.response_timeout_ms = request->response_timeout_ms;
            config.rs485_id = request->rs485_id;
        }

        robot_embeded::PowerControlResponse control_response;
        const auto result = driver_.sendPowerControl(config, command, control_response);
        response->success = control_response.success;
        response->command_code = control_response.command;
        response->result = control_response.result;
        response->message = result.message;
        response->request_frame = control_response.request_frame;
        response->raw_frame = control_response.raw_frame;
        response->code = result.code;
    }

    robot_embeded::HardwareDriver driver_;
    std::mutex driver_mutex_;
    robot_embeded::PowerConfig power_config_;
    rclcpp::Service<robot_embeded_interfaces::srv::SendPowerControl>::SharedPtr control_srv_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RobotEmbededPowerNode>());
    rclcpp::shutdown();
    return 0;
}
