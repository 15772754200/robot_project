#include "robot_embeded_driver/hardware_driver.hpp"

#include "rclcpp/rclcpp.hpp"
#include "robot_embeded_interfaces/msg/bms_state.hpp"
#include "robot_embeded_interfaces/srv/query_bms.hpp"
#include "robot_embeded_interfaces/srv/send_bms_control.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

namespace
{
robot_embeded_interfaces::msg::BmsState toMsg(const robot_embeded::BmsInfo& info)
{
    robot_embeded_interfaces::msg::BmsState msg;
    msg.battery_version = info.battery_version;
    msg.battery_type = info.battery_type;
    msg.capacity_ratio = info.capacity_ratio;
    msg.total_voltage = info.total_voltage;
    msg.current = info.current;
    msg.charge_discharge_state = info.charge_discharge_state;
    msg.cell_voltages = info.cell_voltages;
    msg.cell_temp1 = info.cell_temp1;
    msg.cell_temp2 = info.cell_temp2;
    msg.cell_temp3 = info.cell_temp3;
    msg.cell_temp4 = info.cell_temp4;
    msg.env_temp = info.env_temp;
    msg.mos_temp = info.mos_temp;
    msg.discharge_mos = info.discharge_mos;
    msg.charge_mos = info.charge_mos;
    msg.charger_connected = info.charger_connected;
    msg.battery_switch = info.battery_switch;
    msg.battery_work_state = info.battery_work_state;
    msg.controller_connected = info.controller_connected;
    msg.error_message = info.error_message;
    return msg;
}

bool parseBmsCommand(const std::string& text, robot_embeded::BmsControlCommand& command)
{
    if (text == "close-charge-mos")
    {
        command = robot_embeded::BmsControlCommand::CloseChargeMos;
        return true;
    }
    if (text == "close-discharge-mos")
    {
        command = robot_embeded::BmsControlCommand::CloseDischargeMos;
        return true;
    }
    if (text == "open-charge-mos")
    {
        command = robot_embeded::BmsControlCommand::OpenChargeMos;
        return true;
    }
    if (text == "open-discharge-mos")
    {
        command = robot_embeded::BmsControlCommand::OpenDischargeMos;
        return true;
    }
    if (text == "enter-factory-mode")
    {
        command = robot_embeded::BmsControlCommand::EnterFactoryMode;
        return true;
    }
    return false;
}
} // namespace

class RobotEmbededBmsNode : public rclcpp::Node
{
public:
    RobotEmbededBmsNode()
        : Node("robot_embeded_bms_node")
    {
        declareParameters();
        loadDefaultsFromParameters();

        bms_pub_ = create_publisher<robot_embeded_interfaces::msg::BmsState>("bms_state", 10);

        query_srv_ = create_service<robot_embeded_interfaces::srv::QueryBms>(
            "query_bms",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::QueryBms::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::QueryBms::Response> response) {
                handleQueryBms(request, response);
            });

        control_srv_ = create_service<robot_embeded_interfaces::srv::SendBmsControl>(
            "send_bms_control",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::SendBmsControl::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::SendBmsControl::Response> response) {
                handleSendBmsControl(request, response);
            });

        if (bms_publish_enabled_)
        {
            bms_timer_ = create_wall_timer(
                std::chrono::milliseconds(bms_poll_interval_ms_),
                [this]() {
                    publishBmsState();
                });
        }

        RCLCPP_INFO(get_logger(), "robot_embeded BMS node started");
    }

private:
    void declareParameters()
    {
        declare_parameter<std::string>("bms.device", "/dev/ttyTHS0");
        declare_parameter<int>("bms.baudrate", 9600);
        declare_parameter<int>("bms.response_timeout_ms", 1500);
        declare_parameter<int>("bms.rs485_tx_value", 0);
        declare_parameter<int>("bms.rs485_rx_value", 1);
        declare_parameter<int>("bms.poll_interval_ms", 1000);
        declare_parameter<bool>("bms.publish_enabled", false);
        declare_parameter<std::string>("logs.directory", "/var/log/robot_embeded");
    }

    void loadDefaultsFromParameters()
    {
        const auto log_dir = get_parameter("logs.directory").as_string();
        bms_config_.device = get_parameter("bms.device").as_string();
        bms_config_.baudrate = get_parameter("bms.baudrate").as_int();
        bms_config_.response_timeout_ms = get_parameter("bms.response_timeout_ms").as_int();
        bms_config_.rs485_tx_value = get_parameter("bms.rs485_tx_value").as_int();
        bms_config_.rs485_rx_value = get_parameter("bms.rs485_rx_value").as_int();
        bms_config_.log_file = log_dir + "/bms.log";

        bms_poll_interval_ms_ = static_cast<int>(std::max<int64_t>(100, get_parameter("bms.poll_interval_ms").as_int()));
        bms_publish_enabled_ = get_parameter("bms.publish_enabled").as_bool();
    }

    void handleQueryBms(
        const std::shared_ptr<robot_embeded_interfaces::srv::QueryBms::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::QueryBms::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        auto config = bms_config_;
        if (!request->use_parameter_defaults)
        {
            config.device = request->device;
            config.baudrate = request->baudrate;
            config.response_timeout_ms = request->response_timeout_ms;
        }

        robot_embeded::BmsInfo info;
        const auto result = driver_.queryBmsInfo(config, info);
        response->ok = result.ok;
        response->code = result.code;
        response->message = result.message;
        if (result.ok)
        {
            response->state = toMsg(info);
            bms_pub_->publish(response->state);
        }
    }

    void handleSendBmsControl(
        const std::shared_ptr<robot_embeded_interfaces::srv::SendBmsControl::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::SendBmsControl::Response> response)
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        robot_embeded::BmsControlCommand command;
        if (!parseBmsCommand(request->command, command))
        {
            response->success = false;
            response->response_code = 0;
            response->code = 2;
            response->message = "unknown BMS control command: " + request->command;
            return;
        }

        auto config = bms_config_;
        if (!request->use_parameter_defaults)
        {
            config.device = request->device;
            config.baudrate = request->baudrate;
            config.response_timeout_ms = request->response_timeout_ms;
        }

        robot_embeded::BmsControlResponse control_response;
        const auto result = driver_.sendBmsControl(config, command, control_response);
        response->success = control_response.success;
        response->response_code = control_response.response_code;
        response->message = result.message;
        response->request_frame = control_response.request_frame;
        response->raw_frame = control_response.raw_frame;
        response->code = result.code;
    }

    void publishBmsState()
    {
        std::lock_guard<std::mutex> lock(driver_mutex_);
        loadDefaultsFromParameters();

        robot_embeded::BmsInfo info;
        const auto result = driver_.queryBmsInfo(bms_config_, info);
        if (!result.ok)
        {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), 5000, "BMS polling failed: %s", result.message.c_str());
            return;
        }
        bms_pub_->publish(toMsg(info));
    }

    robot_embeded::HardwareDriver driver_;
    std::mutex driver_mutex_;
    robot_embeded::BmsConfig bms_config_;
    int bms_poll_interval_ms_ = 1000;
    bool bms_publish_enabled_ = false;

    rclcpp::Publisher<robot_embeded_interfaces::msg::BmsState>::SharedPtr bms_pub_;
    rclcpp::TimerBase::SharedPtr bms_timer_;
    rclcpp::Service<robot_embeded_interfaces::srv::QueryBms>::SharedPtr query_srv_;
    rclcpp::Service<robot_embeded_interfaces::srv::SendBmsControl>::SharedPtr control_srv_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RobotEmbededBmsNode>());
    rclcpp::shutdown();
    return 0;
}
