#ifndef ROBOT_EMBEDED_DRIVER_HARDWARE_DRIVER_HPP
#define ROBOT_EMBEDED_DRIVER_HARDWARE_DRIVER_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace robot_embeded
{

struct Result
{
    bool ok = false;
    int code = 0;
    std::string message;
};

struct SpiConfig
{
    std::string device = "/dev/spidev0.0";
    uint8_t mode = 0;
    uint8_t bits_per_word = 8;
    uint32_t speed_hz = 1000000;
    uint8_t lsb_first = 0;
    bool verify = true;
    std::string log_file = "/var/log/robot_embeded/spi_configure.log";
};

struct SpiState
{
    std::string device = "/dev/spidev0.0";
    bool ok = false;
    int code = 0;
    std::string message;
    uint8_t mode = 0;
    uint8_t bits_per_word = 0;
    uint32_t speed_hz = 0;
    uint8_t lsb_first = 0;
};

struct UsbSerialConfig
{
    std::string device = "/dev/ttyUSB0";
    int baudrate = 115200;
    int databits = 8;
    std::string parity = "none";
    int stopbits = 1;
    bool rtscts = false;
    bool verify = true;
    std::string log_file = "/var/log/robot_embeded/usb_configure.log";
};

struct UsbQueryOptions
{
    std::string subsystem;
    std::string devnode_prefix;
    bool include_without_devnode = true;
    std::string log_file = "/var/log/robot_embeded/usb_query.log";
};

struct UsbDeviceInfo
{
    std::string sysfs_path;
    std::string devnode;
    std::string subsystem;
    std::string driver;
    std::string vendor_id;
    std::string product_id;
    std::string manufacturer;
    std::string product;
    std::string serial;
    std::string busnum;
    std::string devnum;
    std::string interface_name;
    std::string interface_class;
    std::string interface_subclass;
    std::string interface_protocol;
};

struct UsbReadConfig
{
    std::string devnode;
    uint32_t max_bytes = 256;
    int timeout_ms = 100;
    std::string log_file = "/var/log/robot_embeded/usb_read.log";
};

struct UsbVideoConfig
{
    std::string device = "/dev/video0";
    uint32_t width = 640;
    uint32_t height = 480;
    std::string pixfmt = "YUYV";
    uint32_t fps = 30;
    bool verify = true;
    std::string log_file = "/var/log/robot_embeded/usb_configure.log";
};

struct BmsInfo
{
    uint8_t battery_version = 0;
    std::string battery_type;
    uint8_t capacity_ratio = 0;
    float total_voltage = 0.0f;
    float current = 0.0f;
    std::string charge_discharge_state;
    std::vector<float> cell_voltages;
    int cell_temp1 = 0;
    int cell_temp2 = 0;
    int cell_temp3 = 0;
    int cell_temp4 = 0;
    int env_temp = 0;
    int mos_temp = 0;
    std::string discharge_mos;
    std::string charge_mos;
    std::string charger_connected;
    std::string battery_switch;
    std::string battery_work_state;
    std::string controller_connected;
    std::string error_message;

    BmsInfo();
};

struct BmsConfig
{
    std::string device = "/dev/ttyTHS0";
    int baudrate = 9600;
    int response_timeout_ms = 1500;
    int rs485_tx_value = 0;
    int rs485_rx_value = 1;
    std::string log_file = "/var/log/robot_embeded/bms.log";
};

enum class BmsControlCommand
{
    CloseChargeMos,
    CloseDischargeMos,
    OpenChargeMos,
    OpenDischargeMos,
    EnterFactoryMode
};

struct BmsControlResponse
{
    bool success = false;
    uint8_t response_code = 0;
    std::string message;
    std::vector<uint8_t> request_frame;
    std::vector<uint8_t> raw_frame;
};

struct PowerConfig
{
    std::string device = "/dev/ttyTHS2";
    int baudrate = 115200;
    int response_timeout_ms = 1500;
    int rs485_tx_value = 0;
    int rs485_rx_value = 1;
    uint8_t rs485_id = 1;
    std::array<uint8_t, 6> fan_pwm_percent = {10, 20, 30, 50, 70, 80};
    std::string log_file = "/var/log/robot_embeded/power.log";
};

enum class PowerControlCommand
{
    OpenLimbs,
    OpenUpperLimbs,
    OpenLowerLimbs,
    CloseLimbs,
    OpenDexterousHands,
    CloseDexterousHands,
    FanControl
};

struct PowerControlResponse
{
    bool success = false;
    uint8_t command = 0;
    uint8_t result = 0;
    std::string message;
    std::vector<uint8_t> request_frame;
    std::vector<uint8_t> raw_frame;
};

class HardwareDriver
{
public:
    HardwareDriver();
    ~HardwareDriver();

    Result init(const std::string& config_path);

    Result configureSpi(const SpiConfig& config);
    Result querySpi(const std::string& device, SpiState& state);
    Result querySpiDevices(std::vector<SpiState>& devices);
    Result queryUsbDevices(const UsbQueryOptions& options, std::vector<UsbDeviceInfo>& devices);
    Result readUsbDevice(const UsbReadConfig& config, std::vector<uint8_t>& data);
    Result configureUsbSerial(const UsbSerialConfig& config);
    Result configureUsbVideo(const UsbVideoConfig& config);
    Result queryBmsInfo(const BmsConfig& config, BmsInfo& info);
    Result sendBmsControl(const BmsConfig& config, BmsControlCommand command, BmsControlResponse& response);
    Result sendPowerControl(const PowerConfig& config, PowerControlCommand command, PowerControlResponse& response);
};

std::string driverVersion();

} // namespace robot_embeded

#endif // ROBOT_EMBEDED_DRIVER_HARDWARE_DRIVER_HPP
