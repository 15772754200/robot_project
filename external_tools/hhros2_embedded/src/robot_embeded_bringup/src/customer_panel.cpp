#include "rclcpp/rclcpp.hpp"

#include "robot_embeded_interfaces/srv/configure_spi.hpp"
#include "robot_embeded_interfaces/srv/configure_usb_serial.hpp"
#include "robot_embeded_interfaces/srv/configure_usb_video.hpp"
#include "robot_embeded_interfaces/srv/query_bms.hpp"
#include "robot_embeded_interfaces/srv/query_ethercat_masters.hpp"
#include "robot_embeded_interfaces/srv/query_spi.hpp"
#include "robot_embeded_interfaces/srv/query_usb_devices.hpp"
#include "robot_embeded_interfaces/srv/read_usb_device.hpp"
#include "robot_embeded_interfaces/srv/send_bms_control.hpp"
#include "robot_embeded_interfaces/srv/send_power_control.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace
{
constexpr auto SERVICE_WAIT = 5s;
constexpr auto SERVICE_CALL = 20s;

template <typename ServiceT>
using ClientPtr = typename rclcpp::Client<ServiceT>::SharedPtr;

std::string bytesToHex(const std::vector<uint8_t>& data)
{
    std::ostringstream out;
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (i != 0)
        {
            out << ' ';
        }
        out << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);
    }
    return out.str();
}

int readInt(const std::string& label, int default_value, int min_value, int max_value)
{
    while (true)
    {
        std::cout << label << " [" << default_value << "]: ";
        std::string text;
        std::getline(std::cin, text);
        if (text.empty())
        {
            return default_value;
        }

        try
        {
            const int value = std::stoi(text);
            if (value >= min_value && value <= max_value)
            {
                return value;
            }
        }
        catch (const std::exception&)
        {
        }
        std::cout << "Please enter an integer from " << min_value << " to " << max_value << ".\n";
    }
}

void printResult(bool ok, int32_t code, const std::string& message)
{
    std::cout << "result: " << (ok ? "success" : "failed") << "\n";
    std::cout << "code: " << code << "\n";
    std::cout << "message: " << message << "\n";
}

void printPowerRawFrames(const std::vector<uint8_t>& data)
{
    std::cout << "raw_frame: " << bytesToHex(data) << "\n";

    size_t offset = 0;
    size_t index = 1;
    while (offset + 5 <= data.size())
    {
        if (data[offset] != 0xA5 || data[offset + 1] != 0x5A)
        {
            break;
        }

        const uint16_t length =
            static_cast<uint16_t>(data[offset + 2]) | (static_cast<uint16_t>(data[offset + 3]) << 8);
        if (length < 6 || offset + length > data.size())
        {
            break;
        }

        std::vector<uint8_t> frame(data.begin() + offset, data.begin() + offset + length);
        std::cout << "raw_frame[" << index << "]: " << bytesToHex(frame) << "\n";
        offset += length;
        ++index;
    }
}

class CustomerPanel
{
public:
    explicit CustomerPanel(rclcpp::Node::SharedPtr node)
        : node_(std::move(node))
    {
        configure_spi_ = node_->create_client<robot_embeded_interfaces::srv::ConfigureSpi>("/configure_spi");
        query_spi_ = node_->create_client<robot_embeded_interfaces::srv::QuerySpi>("/query_spi");
        query_usb_devices_ =
            node_->create_client<robot_embeded_interfaces::srv::QueryUsbDevices>("/query_usb_devices");
        read_usb_device_ = node_->create_client<robot_embeded_interfaces::srv::ReadUsbDevice>("/read_usb_device");
        configure_usb_serial_ =
            node_->create_client<robot_embeded_interfaces::srv::ConfigureUsbSerial>("/configure_usb_serial");
        configure_usb_video_ =
            node_->create_client<robot_embeded_interfaces::srv::ConfigureUsbVideo>("/configure_usb_video");
        query_bms_ = node_->create_client<robot_embeded_interfaces::srv::QueryBms>("/query_bms");
        send_bms_control_ =
            node_->create_client<robot_embeded_interfaces::srv::SendBmsControl>("/send_bms_control");
        send_power_control_ =
            node_->create_client<robot_embeded_interfaces::srv::SendPowerControl>("/send_power_control");
        query_ethercat_masters_ =
            node_->create_client<robot_embeded_interfaces::srv::QueryEthercatMasters>("/query_ethercat_masters");
    }

    void run()
    {
        while (rclcpp::ok())
        {
            printMenu();
            std::cout << "Select function: ";
            std::string key;
            std::getline(std::cin, key);

            if (key == "q" || key == "quit" || key == "exit")
            {
                std::cout << "Customer panel exited.\n";
                return;
            }
            if (key == "1")
            {
                queryBms();
            }
            else if (key == "2")
            {
                bmsControlMenu();
            }
            else if (key == "3")
            {
                querySpi();
            }
            else if (key == "4")
            {
                configureSpiDefaults();
            }
            else if (key == "5")
            {
                queryUsbAll();
            }
            else if (key == "6")
            {
                queryUsbHid();
            }
            else if (key == "7")
            {
                queryUsbInput();
            }
            else if (key == "8")
            {
                readUsbDevice();
            }
            else if (key == "9")
            {
                configureUsbSerialDefaults();
            }
            else if (key == "0")
            {
                configureUsbVideoDefaults();
            }
            else if (key == "e")
            {
                queryEthercatMasters();
            }
            else if (key == "p")
            {
                powerControlMenu();
            }
            else
            {
                std::cout << "Invalid key, please try again.\n";
            }
            pause();
        }
    }

private:
    static void printMenu()
    {
        std::cout << "\n";
        std::cout << "========================================================\n";
        std::cout << "robot_embeded customer service panel\n";
        std::cout << "========================================================\n";
        std::cout << "1  Query BMS battery state\n";
        std::cout << "2  Send BMS control command\n";
        std::cout << "3  Query all SPI interfaces and configuration\n";
        std::cout << "4  Configure SPI with default parameters\n";
        std::cout << "5  Query all USB devices\n";
        std::cout << "6  Query HID USB devices\n";
        std::cout << "7  Query input-event USB devices\n";
        std::cout << "8  Read raw USB device data\n";
        std::cout << "9  Configure USB serial with default parameters\n";
        std::cout << "0  Configure USB video with default parameters\n";
        std::cout << "e  Query EtherCAT master/slave status\n";
        std::cout << "p  Send power board control command\n";
        std::cout << "q  Quit\n";
        std::cout << "--------------------------------------------------------\n";
    }

    static void pause()
    {
        std::cout << "\nPress Enter to return to main menu...";
        std::string unused;
        std::getline(std::cin, unused);
    }

    template <typename ServiceT>
    typename ServiceT::Response::SharedPtr call(
        const ClientPtr<ServiceT>& client,
        const std::string& service_name,
        const typename ServiceT::Request::SharedPtr& request)
    {
        if (!client->wait_for_service(SERVICE_WAIT))
        {
            std::cout << "Service " << service_name << " is not ready. Start robot_embeded.launch.py first.\n";
            return nullptr;
        }

        auto future = client->async_send_request(request);
        const auto status = rclcpp::spin_until_future_complete(node_, future, SERVICE_CALL);
        if (status != rclcpp::FutureReturnCode::SUCCESS)
        {
            std::cout << "Service " << service_name << " response timed out.\n";
            return nullptr;
        }
        return future.get();
    }

    void queryBms()
    {
        using Service = robot_embeded_interfaces::srv::QueryBms;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        auto response = call<Service>(query_bms_, "/query_bms", request);
        if (!response)
        {
            return;
        }

        printResult(response->ok, response->code, response->message);
        if (!response->ok)
        {
            return;
        }
        const auto& state = response->state;
        std::cout << "battery_type: " << state.battery_type << "\n";
        std::cout << "capacity_ratio: " << static_cast<int>(state.capacity_ratio) << "%\n";
        std::cout << "total_voltage: " << state.total_voltage << " V\n";
        std::cout << "current: " << state.current << " A\n";
        std::cout << "charge_discharge_state: " << state.charge_discharge_state << "\n";
        std::cout << "discharge_mos: " << state.discharge_mos << "\n";
        std::cout << "charge_mos: " << state.charge_mos << "\n";
        std::cout << "charger_connected: " << state.charger_connected << "\n";
        std::cout << "controller_connected: " << state.controller_connected << "\n";
        std::cout << "env_temp: " << state.env_temp << " C\n";
        std::cout << "mos_temp: " << state.mos_temp << " C\n";
        if (!state.cell_voltages.empty())
        {
            std::cout << "cell_voltages:";
            for (float voltage : state.cell_voltages)
            {
                std::cout << ' ' << voltage << "V";
            }
            std::cout << "\n";
        }
        if (!state.error_message.empty())
        {
            std::cout << "error_message: " << state.error_message << "\n";
        }
    }

    void bmsControlMenu()
    {
        struct Command
        {
            std::string key;
            std::string label;
            std::string command;
        };
        const Command commands[] = {
            {"1", "Close charge MOS", "close-charge-mos"},
            {"2", "Close discharge MOS", "close-discharge-mos"},
            {"3", "Open charge MOS", "open-charge-mos"},
            {"4", "Open discharge MOS", "open-discharge-mos"},
            {"5", "Enter factory mode", "enter-factory-mode"},
        };

        std::cout << "\nBMS control commands\n";
        for (const auto& command : commands)
        {
            std::cout << command.key << "  " << command.label << "\n";
        }
        std::cout << "b  Back\n";
        std::cout << "Select command: ";

        std::string key;
        std::getline(std::cin, key);
        if (key == "b" || key == "back" || key.empty())
        {
            return;
        }

        const Command* selected = nullptr;
        for (const auto& command : commands)
        {
            if (command.key == key)
            {
                selected = &command;
                break;
            }
        }
        if (selected == nullptr)
        {
            std::cout << "Invalid key, cancelled.\n";
            return;
        }

        std::cout << "About to run [" << selected->label << "], type y/n: ";
        std::string confirm;
        std::getline(std::cin, confirm);
        if (confirm == "n")
        {
            std::cout << "Cancelled.\n";
            return;
        }
        if (confirm != "y")
        {
            std::cout << "Invalid input, cancelled.\n";
            return;
        }

        using Service = robot_embeded_interfaces::srv::SendBmsControl;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        request->command = selected->command;
        auto response = call<Service>(send_bms_control_, "/send_bms_control", request);
        if (!response)
        {
            return;
        }
        printResult(response->success, response->code, response->message);
        if (!response->request_frame.empty())
        {
            std::cout << "request_frame: " << bytesToHex(response->request_frame) << "\n";
        }
        std::cout << "response_code: " << static_cast<int>(response->response_code) << "\n";
        if (!response->raw_frame.empty())
        {
            std::cout << "raw_frame: " << bytesToHex(response->raw_frame) << "\n";
        }
    }

    void powerControlMenu()
    {
        struct Command
        {
            std::string key;
            std::string label;
            std::string command;
        };
        const Command commands[] = {
            {"1", "Open limbs power", "open-limbs-power"},
            {"2", "Open upper limbs power", "open-upper-limbs-power"},
            {"3", "Open lower limbs power", "open-lower-limbs-power"},
            {"4", "Close limbs power", "close-limbs-power"},
            {"5", "Open dexterous hands power", "open-dexterous-hands-power"},
            {"6", "Close dexterous hands power", "close-dexterous-hands-power"},
            {"7", "Fan control (10/20/30/50/70/80 PWM)", "fan-control"},
        };

        std::cout << "\nPower board control commands\n";
        for (const auto& command : commands)
        {
            std::cout << command.key << "  " << command.label << "\n";
        }
        std::cout << "b  Back\n";
        std::cout << "Select command: ";

        std::string key;
        std::getline(std::cin, key);
        if (key == "b" || key == "back" || key.empty())
        {
            return;
        }

        const Command* selected = nullptr;
        for (const auto& command : commands)
        {
            if (command.key == key)
            {
                selected = &command;
                break;
            }
        }
        if (selected == nullptr)
        {
            std::cout << "Invalid key, cancelled.\n";
            return;
        }

        std::cout << "About to run [" << selected->label << "], type y/n: ";
        std::string confirm;
        std::getline(std::cin, confirm);
        if (confirm == "n")
        {
            std::cout << "Cancelled.\n";
            return;
        }
        if (confirm != "y")
        {
            std::cout << "Invalid input, cancelled.\n";
            return;
        }

        using Service = robot_embeded_interfaces::srv::SendPowerControl;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        request->command = selected->command;
        auto response = call<Service>(send_power_control_, "/send_power_control", request);
        if (!response)
        {
            return;
        }
        printResult(response->success, response->code, response->message);
        if (!response->request_frame.empty())
        {
            std::cout << "request_frame: " << bytesToHex(response->request_frame) << "\n";
        }
        std::cout << "command_code: " << static_cast<int>(response->command_code) << "\n";
        std::cout << "result: " << static_cast<int>(response->result) << "\n";
        if (!response->raw_frame.empty())
        {
            printPowerRawFrames(response->raw_frame);
        }
    }

    void querySpi()
    {
        using Service = robot_embeded_interfaces::srv::QuerySpi;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        auto response = call<Service>(query_spi_, "/query_spi", request);
        if (!response)
        {
            return;
        }
        printResult(response->ok, response->code, response->message);
        if (!response->ok)
        {
            return;
        }
        std::cout << "SPI interface count: " << response->devices.size() << "\n";
        for (size_t i = 0; i < response->devices.size(); ++i)
        {
            const auto& device = response->devices[i];
            std::cout << "\n[" << i + 1 << "] " << device.device << "\n";
            std::cout << "  status: " << (device.ok ? "normal" : "abnormal") << "\n";
            std::cout << "  code: " << device.code << "\n";
            std::cout << "  message: " << device.message << "\n";
            if (device.ok)
            {
                std::cout << "  mode: " << static_cast<int>(device.mode) << "\n";
                std::cout << "  bits_per_word: " << static_cast<int>(device.bits_per_word) << "\n";
                std::cout << "  speed_hz: " << device.speed_hz << "\n";
                std::cout << "  lsb_first: " << static_cast<int>(device.lsb_first) << "\n";
            }
        }
    }

    void configureSpiDefaults()
    {
        using Service = robot_embeded_interfaces::srv::ConfigureSpi;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        auto response = call<Service>(configure_spi_, "/configure_spi", request);
        if (response)
        {
            printResult(response->ok, response->code, response->message);
        }
    }

    void queryUsbAll()
    {
        queryUsbDevices(true, "", "", true, "Query all USB devices");
    }

    void queryUsbHid()
    {
        queryUsbDevices(false, "hidraw", "", true, "Query HID USB devices");
    }

    void queryUsbInput()
    {
        queryUsbDevices(false, "input", "/dev/input/", false, "Query input-event USB devices");
    }

    void queryUsbDevices(
        bool defaults,
        const std::string& subsystem,
        const std::string& devnode_prefix,
        bool include_without_devnode,
        const std::string& label)
    {
        using Service = robot_embeded_interfaces::srv::QueryUsbDevices;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = defaults;
        request->subsystem = subsystem;
        request->devnode_prefix = devnode_prefix;
        request->include_without_devnode = include_without_devnode;
        auto response = call<Service>(query_usb_devices_, "/query_usb_devices", request);
        (void)label;
        if (!response)
        {
            return;
        }
        printResult(response->ok, response->code, response->message);
        if (!response->ok)
        {
            return;
        }
        std::cout << "device count: " << response->devices.size() << "\n";
        for (size_t i = 0; i < response->devices.size(); ++i)
        {
            const auto& device = response->devices[i];
            const auto name =
                !device.product.empty() ? device.product :
                (!device.manufacturer.empty() ? device.manufacturer :
                 (!device.interface_name.empty() ? device.interface_name : std::string("(unknown device)")));
            std::cout << "\n[" << i + 1 << "] " << name << "\n";
            std::cout << "  devnode: " << (device.devnode.empty() ? "-" : device.devnode) << "\n";
            std::cout << "  subsystem: " << (device.subsystem.empty() ? "-" : device.subsystem) << "\n";
            std::cout << "  driver: " << (device.driver.empty() ? "-" : device.driver) << "\n";
            std::cout << "  vendor/product: " << (device.vendor_id.empty() ? "-" : device.vendor_id) << "/"
                      << (device.product_id.empty() ? "-" : device.product_id) << "\n";
            std::cout << "  sysfs: " << (device.sysfs_path.empty() ? "-" : device.sysfs_path) << "\n";
        }
    }

    void readUsbDevice()
    {
        std::cout << "USB devnode, for example /dev/hidraw0 or /dev/input/event0: ";
        std::string devnode;
        std::getline(std::cin, devnode);
        if (devnode.empty())
        {
            std::cout << "devnode is empty, cancelled.\n";
            return;
        }

        using Service = robot_embeded_interfaces::srv::ReadUsbDevice;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = false;
        request->devnode = devnode;
        request->max_bytes = static_cast<uint32_t>(readInt("bytes to read", 64, 1, 4096));
        request->timeout_ms = readInt("timeout ms", 1000, 1, 60000);
        auto response = call<Service>(read_usb_device_, "/read_usb_device", request);
        if (!response)
        {
            return;
        }
        printResult(response->ok, response->code, response->message);
        if (response->ok)
        {
            std::cout << "bytes_read: " << response->bytes_read << "\n";
            if (!response->data.empty())
            {
                std::cout << "data: " << bytesToHex(response->data) << "\n";
            }
        }
    }

    void configureUsbSerialDefaults()
    {
        using Service = robot_embeded_interfaces::srv::ConfigureUsbSerial;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        auto response = call<Service>(configure_usb_serial_, "/configure_usb_serial", request);
        if (response)
        {
            printResult(response->ok, response->code, response->message);
        }
    }

    void configureUsbVideoDefaults()
    {
        using Service = robot_embeded_interfaces::srv::ConfigureUsbVideo;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        auto response = call<Service>(configure_usb_video_, "/configure_usb_video", request);
        if (response)
        {
            printResult(response->ok, response->code, response->message);
        }
    }

    void queryEthercatMasters()
    {
        using Service = robot_embeded_interfaces::srv::QueryEthercatMasters;
        auto request = std::make_shared<Service::Request>();
        request->use_parameter_defaults = true;
        auto response = call<Service>(query_ethercat_masters_, "/query_ethercat_masters", request);
        if (!response)
        {
            return;
        }

        printResult(response->ok, response->code, response->message);
        if (!response->ok)
        {
            return;
        }

        std::cout << "master count: " << response->master_count << "\n";
        if (!response->masters.empty())
        {
            std::cout << "masters:\n";
            for (const auto& master : response->masters)
            {
                std::cout << "\n[master " << master.master_index << "]\n";
                std::cout << "  state: " << (master.state.empty() ? "-" : master.state) << "\n";
                std::cout << "  connected: " << (master.connected ? "normal" : "abnormal") << "\n";
                std::cout << "  slave_count: " << master.slave_count << "\n";
                std::cout << "  message: " << (master.message.empty() ? "-" : master.message) << "\n";
            }
        }

        std::cout << "slave count: " << response->slaves.size() << "\n";
        if (response->slaves.empty())
        {
            return;
        }

        std::cout << "\nslaves by master:\n";
        for (const auto& master : response->masters)
        {
            std::cout << "\nmaster " << master.master_index
                      << "  state=" << (master.state.empty() ? "-" : master.state)
                      << "  connected=" << (master.connected ? "normal" : "abnormal")
                      << "  slave_count=" << master.slave_count << "\n";
            std::cout << "  pos  state   op      connected  device_name\n";
            std::cout << "  --------------------------------------------\n";

            bool printed_slave = false;
            for (const auto& slave : response->slaves)
            {
                if (slave.master_index != master.master_index)
                {
                    continue;
                }

                printed_slave = true;
                std::cout << "  " << std::setw(3) << slave.position
                          << "  " << std::setw(6) << (slave.state.empty() ? "-" : slave.state)
                          << "  " << std::setw(6) << (slave.operational ? "yes" : "no")
                          << "  " << std::setw(9) << (slave.connected ? "normal" : "abnormal")
                          << "  " << (slave.device_name.empty() ? "-" : slave.device_name) << "\n";
                if (!slave.operational || !slave.connected || slave.message.find("failed") != std::string::npos)
                {
                    std::cout << "       message: " << (slave.message.empty() ? "-" : slave.message) << "\n";
                }
            }

            if (!printed_slave)
            {
                std::cout << "  no slaves found\n";
            }
        }
    }

    rclcpp::Node::SharedPtr node_;
    ClientPtr<robot_embeded_interfaces::srv::ConfigureSpi> configure_spi_;
    ClientPtr<robot_embeded_interfaces::srv::QuerySpi> query_spi_;
    ClientPtr<robot_embeded_interfaces::srv::QueryUsbDevices> query_usb_devices_;
    ClientPtr<robot_embeded_interfaces::srv::ReadUsbDevice> read_usb_device_;
    ClientPtr<robot_embeded_interfaces::srv::ConfigureUsbSerial> configure_usb_serial_;
    ClientPtr<robot_embeded_interfaces::srv::ConfigureUsbVideo> configure_usb_video_;
    ClientPtr<robot_embeded_interfaces::srv::QueryBms> query_bms_;
    ClientPtr<robot_embeded_interfaces::srv::SendBmsControl> send_bms_control_;
    ClientPtr<robot_embeded_interfaces::srv::SendPowerControl> send_power_control_;
    ClientPtr<robot_embeded_interfaces::srv::QueryEthercatMasters> query_ethercat_masters_;
};
} // namespace

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("robot_embeded_customer_panel");
    CustomerPanel panel(node);
    try
    {
        panel.run();
    }
    catch (const std::exception& exc)
    {
        std::cerr << "customer_panel failed: " << exc.what() << "\n";
    }
    rclcpp::shutdown();
    return 0;
}
