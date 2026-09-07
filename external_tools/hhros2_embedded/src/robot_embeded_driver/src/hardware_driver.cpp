#include "robot_embeded_driver/hardware_driver.hpp"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace robot_embeded
{
    namespace
    {
        std::string trim(const std::string &text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return "";
            }
            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        std::string nowIso8601()
        {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_r(&t, &tm);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
            return oss.str();
        }

        std::string jsonEscape(const std::string &text)
        {
            std::ostringstream oss;
            for (char ch : text)
            {
                switch (ch)
                {
                case '\\':
                    oss << "\\\\";
                    break;
                case '"':
                    oss << "\\\"";
                    break;
                case '\n':
                    oss << "\\n";
                    break;
                case '\r':
                    oss << "\\r";
                    break;
                case '\t':
                    oss << "\\t";
                    break;
                default:
                    oss << ch;
                    break;
                }
            }
            return oss.str();
        }

        bool ensureLogParent(const std::string &log_file)
        {
            fs::path path(log_file);
            if (path.parent_path().empty())
            {
                return true;
            }
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            return !ec;
        }

        bool appendLogLine(const std::string &log_file, const std::string &line)
        {
            if (!ensureLogParent(log_file))
            {
                return false;
            }
            std::ofstream log(log_file, std::ios::app);
            if (!log)
            {
                return false;
            }
            log << line << "\n";
            return static_cast<bool>(log);
        }

        bool readFile(const fs::path &path, std::string &out)
        {
            std::ifstream in(path);
            if (!in)
            {
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            out = trim(ss.str());
            return true;
        }

        bool pathExists(const fs::path &path)
        {
            std::error_code ec;
            return fs::exists(path, ec);
        }

        std::map<std::string, std::string> listDevNodesByMajorMinor()
        {
            std::map<std::string, std::string> out;
            std::error_code ec;
            const fs::path dev_root("/dev");
            if (!fs::exists(dev_root, ec))
            {
                return out;
            }

            fs::recursive_directory_iterator it(dev_root, fs::directory_options::skip_permission_denied, ec);
            const fs::recursive_directory_iterator end;
            while (it != end)
            {
                const auto path = it->path();
                struct stat st{};
                if (::stat(path.c_str(), &st) == 0 && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)))
                {
                    const std::string key = std::to_string(major(st.st_rdev)) + ":" + std::to_string(minor(st.st_rdev));
                    const std::string value = path.string();
                    auto existing = out.find(key);
                    if (existing == out.end() || value.size() < existing->second.size())
                    {
                        out[key] = value;
                    }
                }
                it.increment(ec);
                if (ec)
                {
                    ec.clear();
                }
            }
            return out;
        }

        std::string lookupDevnode(
            const std::map<std::string, std::string> &dev_nodes_by_major_minor,
            const fs::path &sysfs_path)
        {
            std::string major_minor;
            if (!readFile(sysfs_path / "dev", major_minor))
            {
                return "";
            }
            const auto it = dev_nodes_by_major_minor.find(major_minor);
            return it == dev_nodes_by_major_minor.end() ? "" : it->second;
        }

        bool startsWith(const std::string &text, const std::string &prefix)
        {
            return prefix.empty() || text.rfind(prefix, 0) == 0;
        }

        bool isSpiDevName(const std::string &name)
        {
            if (!startsWith(name, "spidev"))
            {
                return false;
            }
            const auto bus_chip = name.substr(std::string("spidev").size());
            const auto dot = bus_chip.find('.');
            if (dot == std::string::npos || dot == 0 || dot + 1 >= bus_chip.size())
            {
                return false;
            }
            return std::all_of(bus_chip.begin(), bus_chip.end(), [](char ch)
                               { return std::isdigit(static_cast<unsigned char>(ch)) || ch == '.'; });
        }

        void addSpiDevPath(std::set<std::string> &paths, const std::string &name)
        {
            if (isSpiDevName(name))
            {
                paths.insert((fs::path("/dev") / name).string());
            }
        }

        std::vector<std::string> listSpiDevicePaths()
        {
            std::set<std::string> paths;

            std::error_code sys_ec;
            const fs::path sys_spidev("/sys/class/spidev");
            if (fs::exists(sys_spidev, sys_ec))
            {
                for (const auto &entry : fs::directory_iterator(sys_spidev, sys_ec))
                {
                    if (sys_ec)
                    {
                        break;
                    }
                    const std::string name = entry.path().filename().string();
                    addSpiDevPath(paths, name);
                }
            }

            sys_ec.clear();
            const fs::path spi_bus("/sys/bus/spi/devices");
            if (fs::exists(spi_bus, sys_ec))
            {
                for (const auto &entry : fs::directory_iterator(spi_bus, sys_ec))
                {
                    if (sys_ec)
                    {
                        break;
                    }

                    const std::string spi_name = entry.path().filename().string();
                    if (startsWith(spi_name, "spi"))
                    {
                        addSpiDevPath(paths, "spidev" + spi_name.substr(std::string("spi").size()));
                    }

                    std::error_code child_ec;
                    const fs::path spidev_dir = entry.path() / "spidev";
                    if (!fs::exists(spidev_dir, child_ec))
                    {
                        continue;
                    }
                    for (const auto &child : fs::directory_iterator(spidev_dir, child_ec))
                    {
                        if (child_ec)
                        {
                            break;
                        }
                        addSpiDevPath(paths, child.path().filename().string());
                    }
                }
            }

            std::error_code dev_ec;
            const fs::path dev_root("/dev");
            if (fs::exists(dev_root, dev_ec))
            {
                for (const auto &entry : fs::directory_iterator(dev_root, dev_ec))
                {
                    if (dev_ec)
                    {
                        break;
                    }
                    const std::string name = entry.path().filename().string();
                    if (isSpiDevName(name))
                    {
                        paths.insert(entry.path().string());
                    }
                }
            }

            return std::vector<std::string>(paths.begin(), paths.end());
        }

        std::string readLinkName(const fs::path &path)
        {
            std::error_code ec;
            const auto target = fs::read_symlink(path, ec);
            if (ec)
            {
                return "";
            }
            return target.filename().string();
        }

        std::string devnodeForClassDevice(const std::string &subsystem, const std::string &name)
        {
            if (subsystem == "input")
            {
                return "/dev/input/" + name;
            }
            if (subsystem == "sound")
            {
                return "/dev/snd/" + name;
            }
            return "/dev/" + name;
        }

        bool findUsbDevicePath(const fs::path &start, fs::path &out)
        {
            fs::path current = start;
            while (!current.empty() && current != current.root_path())
            {
                if (pathExists(current / "idVendor") && pathExists(current / "idProduct"))
                {
                    out = current;
                    return true;
                }
                current = current.parent_path();
            }
            return false;
        }

        bool findUsbInterfacePath(const fs::path &start, fs::path &out)
        {
            fs::path current = start;
            while (!current.empty() && current != current.root_path())
            {
                if (pathExists(current / "bInterfaceClass"))
                {
                    out = current;
                    return true;
                }
                current = current.parent_path();
            }
            return false;
        }

        void fillUsbDeviceFields(const fs::path &usb_device_path, UsbDeviceInfo &info)
        {
            readFile(usb_device_path / "idVendor", info.vendor_id);
            readFile(usb_device_path / "idProduct", info.product_id);
            readFile(usb_device_path / "manufacturer", info.manufacturer);
            readFile(usb_device_path / "product", info.product);
            readFile(usb_device_path / "serial", info.serial);
            readFile(usb_device_path / "busnum", info.busnum);
            readFile(usb_device_path / "devnum", info.devnum);
        }

        void fillUsbInterfaceFields(const fs::path &interface_path, UsbDeviceInfo &info)
        {
            if (interface_path.empty())
            {
                return;
            }
            readFile(interface_path / "interface", info.interface_name);
            readFile(interface_path / "bInterfaceClass", info.interface_class);
            readFile(interface_path / "bInterfaceSubClass", info.interface_subclass);
            readFile(interface_path / "bInterfaceProtocol", info.interface_protocol);
        }

        bool usbInfoMatches(const UsbQueryOptions &options, const UsbDeviceInfo &info)
        {
            if (!options.subsystem.empty() && info.subsystem != options.subsystem)
            {
                return false;
            }
            if (!startsWith(info.devnode, options.devnode_prefix))
            {
                return false;
            }
            if (!options.include_without_devnode && info.devnode.empty())
            {
                return false;
            }
            return true;
        }

        std::string usbDeviceToJson(const UsbDeviceInfo &info)
        {
            std::ostringstream oss;
            oss << "{\"sysfs_path\":\"" << jsonEscape(info.sysfs_path) << "\",";
            oss << "\"devnode\":\"" << jsonEscape(info.devnode) << "\",";
            oss << "\"subsystem\":\"" << jsonEscape(info.subsystem) << "\",";
            oss << "\"vendor_id\":\"" << jsonEscape(info.vendor_id) << "\",";
            oss << "\"product_id\":\"" << jsonEscape(info.product_id) << "\",";
            oss << "\"product\":\"" << jsonEscape(info.product) << "\"}";
            return oss.str();
        }

        uint32_t fourccFromString(const std::string &text)
        {
            return v4l2_fourcc(text[0], text[1], text[2], text[3]);
        }

        std::string fourccToString(uint32_t pixfmt)
        {
            std::string s(4, ' ');
            s[0] = static_cast<char>(pixfmt & 0xFF);
            s[1] = static_cast<char>((pixfmt >> 8) & 0xFF);
            s[2] = static_cast<char>((pixfmt >> 16) & 0xFF);
            s[3] = static_cast<char>((pixfmt >> 24) & 0xFF);
            return s;
        }

        speed_t baudToConstant(int baudrate)
        {
            switch (baudrate)
            {
            case 9600:
                return B9600;
            case 19200:
                return B19200;
            case 38400:
                return B38400;
            case 57600:
                return B57600;
            case 115200:
                return B115200;
            case 230400:
                return B230400;
#ifdef B460800
            case 460800:
                return B460800;
#endif
#ifdef B921600
            case 921600:
                return B921600;
#endif
            default:
                return 0;
            }
        }

        int constantToBaud(speed_t speed)
        {
            switch (speed)
            {
            case B9600:
                return 9600;
            case B19200:
                return 19200;
            case B38400:
                return 38400;
            case B57600:
                return 57600;
            case B115200:
                return 115200;
            case B230400:
                return 230400;
#ifdef B460800
            case B460800:
                return 460800;
#endif
#ifdef B921600
            case B921600:
                return 921600;
#endif
            default:
                return -1;
            }
        }

        Result makeResult(bool ok, int code, const std::string &message)
        {
            return Result{ok, code, message};
        }

        bool parseBmsFrame(const std::vector<uint8_t> &data, BmsInfo &out, std::string &error)
        {
            if (data.size() < 61)
            {
                error = "BMS frame length is less than 61 bytes";
                return false;
            }
            if (data.front() != 0x4B || data.back() != 0x0D)
            {
                error = "BMS frame header or tail is invalid";
                return false;
            }

            auto it = data.begin() + 5;
            const uint8_t battery_type = *(it + 4);
            const uint8_t battery_state = *(it + 26);

            out.battery_version = *(it - 4);
            switch (battery_type)
            {
            case 0x01:
                out.battery_type = "lead-acid";
                break;
            case 0x02:
                out.battery_type = "ternary-lithium";
                break;
            case 0x03:
                out.battery_type = "lithium-iron-phosphate";
                break;
            case 0x04:
                out.battery_type = "manganese-lithium";
                break;
            default:
                out.battery_type = "unknown";
                break;
            }

            out.capacity_ratio = *(it + 10);
            const uint16_t total_voltage_raw = static_cast<uint16_t>((*(it + 7) << 8) | *(it + 6));
            out.total_voltage = total_voltage_raw * 0.01f;

            out.cell_temp1 = *(it + 11) - 30;
            out.cell_temp2 = *(it + 12) - 30;
            out.cell_temp3 = *(it + 13) - 30;
            out.cell_temp4 = *(it + 14) - 30;
            out.env_temp = *(it + 15) - 30;
            out.mos_temp = *(it + 16) - 30;

            const uint16_t current_raw = static_cast<uint16_t>((*(it + 9) << 8) | *(it + 8));
            int16_t current_val = static_cast<int16_t>(current_raw & 0x7FFF);
            if (current_raw & 0x8000)
            {
                current_val = -current_val;
                out.charge_discharge_state = "discharging";
            }
            else
            {
                out.charge_discharge_state = current_val == 0 ? "idle" : "charging";
            }
            out.current = current_val * 0.01f;

            out.cell_voltages.assign(13, 0.0f);
            for (size_t i = 0; i < 13; ++i)
            {
                auto idx = it + 28 + static_cast<std::ptrdiff_t>(i * 2);
                const uint16_t raw = static_cast<uint16_t>((*(idx + 1) << 8) | *(idx));
                out.cell_voltages[i] = raw * 0.01f;
            }

            out.discharge_mos = (battery_state & 0x80) ? "on" : "off";
            out.charge_mos = (battery_state & 0x40) ? "on" : "off";
            out.charger_connected = (battery_state & 0x20) ? "connected" : "disconnected";
            out.battery_switch = (battery_state & 0x10) ? "on" : "off";
            switch (battery_state & 0x0C)
            {
            case 0x00:
                out.battery_work_state = "standby";
                break;
            case 0x04:
                out.battery_work_state = "charging";
                break;
            case 0x08:
                out.battery_work_state = "discharging";
                break;
            default:
                out.battery_work_state = "unknown";
                break;
            }
            out.controller_connected = (battery_state & 0x02) ? "connected" : "disconnected";

            std::string err_msg;
            const uint8_t err1 = *(it + 23);
            const uint8_t err2 = *(it + 24);
            const uint8_t err3 = *(it + 25);
            if (err1 & 0x80)
                err_msg += "external-circuit-open;";
            if (err1 & 0x40)
                err_msg += "over-current;";
            if (err1 & 0x20)
                err_msg += "low-voltage-protection;";
            if (err1 & 0x10)
                err_msg += "high-voltage-protection;";
            if (err1 & 0x08)
                err_msg += "cell-over-voltage;";
            if (err1 & 0x04)
                err_msg += "charge-over-current;";
            if (err1 & 0x01)
                err_msg += "cell-under-voltage;";
            if (err2 & 0x80)
                err_msg += "charge-high-temperature;";
            if (err2 & 0x40)
                err_msg += "discharge-high-temperature;";
            if (err2 & 0x20)
                err_msg += "charge-low-temperature;";
            if (err2 & 0x10)
                err_msg += "discharge-low-temperature;";
            if (err2 & 0x08)
                err_msg += "mos-high-temperature;";
            if (err3 & 0x80)
                err_msg += "temperature-sampling-failure;";
            if (err3 & 0x40)
                err_msg += "voltage-sampling-failure;";
            if (err3 & 0x20)
                err_msg += "mos-communication-failure;";
            if (err3 & 0x10)
                err_msg += "bms-failure;";
            out.error_message = err_msg.empty() ? "none" : err_msg;
            return true;
        }

        std::string bmsInfoToJson(const BmsInfo &info)
        {
            std::ostringstream oss;
            oss << "{\"timestamp\":\"" << nowIso8601() << "\",";
            oss << "\"capacity_ratio\":" << static_cast<int>(info.capacity_ratio) << ",";
            oss << "\"total_voltage\":" << info.total_voltage << ",";
            oss << "\"current\":" << info.current << ",";
            oss << "\"work_state\":\"" << jsonEscape(info.battery_work_state) << "\",";
            oss << "\"error\":\"" << jsonEscape(info.error_message) << "\"}";
            return oss.str();
        }

        std::string bytesToHex(const std::vector<uint8_t> &bytes)
        {
            std::ostringstream oss;
            for (size_t i = 0; i < bytes.size(); ++i)
            {
                if (i)
                    oss << " ";
                oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(bytes[i]);
            }
            return oss.str();
        }

        std::string bmsControlName(BmsControlCommand command)
        {
            switch (command)
            {
            case BmsControlCommand::CloseChargeMos:
                return "close-charge-mos";
            case BmsControlCommand::CloseDischargeMos:
                return "close-discharge-mos";
            case BmsControlCommand::OpenChargeMos:
                return "open-charge-mos";
            case BmsControlCommand::OpenDischargeMos:
                return "open-discharge-mos";
            case BmsControlCommand::EnterFactoryMode:
                return "enter-factory-mode";
            }
            return "unknown";
        }

        std::vector<uint8_t> bmsControlBytes(BmsControlCommand command)
        {
            switch (command)
            {
            case BmsControlCommand::CloseChargeMos:
                return {0x4A, 0x01, 0x02, 0x11, 0x01, 0x00, 0x13, 0x0E};
            case BmsControlCommand::CloseDischargeMos:
                return {0x4A, 0x01, 0x02, 0x12, 0x01, 0x00, 0x10, 0x0E};
            case BmsControlCommand::OpenChargeMos:
                return {0x4A, 0x01, 0x02, 0x13, 0x01, 0x00, 0x11, 0x0E};
            case BmsControlCommand::OpenDischargeMos:
                return {0x4A, 0x01, 0x02, 0x14, 0x01, 0x00, 0x16, 0x0E};
            case BmsControlCommand::EnterFactoryMode:
                return {0x4A, 0x01, 0x02, 0x15, 0x01, 0x00, 0x17, 0x0E};
            }
            return {};
        }

        bool parseBmsControlFrame(const std::vector<uint8_t> &data, BmsControlResponse &out, std::string &error)
        {
            if (data.size() < 8)
            {
                error = "BMS control response length is less than 8 bytes";
                return false;
            }
            if (data.front() != 0x5A || data[7] != 0x0D)
            {
                error = "BMS control response header or tail is invalid";
                return false;
            }
            out.raw_frame.assign(data.begin(), data.begin() + 8);
            out.response_code = data[4];
            out.success = out.response_code == 0x01;
            out.message = out.success ? "BMS control command succeeded"
                                      : "BMS control command failed, response_code=" + std::to_string(out.response_code);
            return true;
        }

        struct Rs485SerialConfig
        {
            std::string label;
            std::string device;
            int baudrate;
            int response_timeout_ms;
        };

        Rs485SerialConfig makeBmsRs485Config(const BmsConfig &config)
        {
            return Rs485SerialConfig{
                "BMS",
                config.device,
                config.baudrate,
                config.response_timeout_ms,
            };
        }

        Rs485SerialConfig makePowerRs485Config(const PowerConfig &config)
        {
            return Rs485SerialConfig{
                "power",
                config.device,
                config.baudrate,
                config.response_timeout_ms,
            };
        }

        Result writeRs485Bytes(
            int fd, const Rs485SerialConfig &config, const uint8_t *data, size_t size, const std::string &what)
        {
            if (fd < 0 || data == nullptr || size == 0)
            {
                return makeResult(false, 1, "invalid " + what + " write request");
            }

            size_t written = 0;
            const auto write_deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(100, config.response_timeout_ms));
            while (written < size)
            {
                if (std::chrono::steady_clock::now() >= write_deadline)
                {
                    return makeResult(false, 2, "write " + what + " timeout");
                }

                const ssize_t n = ::write(fd, data + written, size - written);
                if (n > 0)
                {
                    written += static_cast<size_t>(n);
                    continue;
                }
                if (n == 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
                {
                    ::usleep(100);
                    continue;
                }

                const int code = errno;
                return makeResult(false, code, "write " + what + " failed: " + std::strerror(code));
            }

            if (tcdrain(fd) != 0)
            {
                const int code = errno;
                return makeResult(false, code, "tcdrain " + what + " failed: " + std::strerror(code));
            }

            return makeResult(true, 0, config.label + " write ok");
        }

        uint8_t crc8(const std::vector<uint8_t> &data)
        {
            uint8_t crc = 0x00;
            for (uint8_t byte : data)
            {
                crc ^= byte;
                for (int i = 0; i < 8; ++i)
                {
                    crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
                }
            }
            return crc;
        }

        std::vector<uint8_t> makePowerFrame(uint8_t rs485_id, const std::vector<uint8_t> &payload)
        {
            std::vector<uint8_t> frame;
            const uint16_t length = static_cast<uint16_t>(2 + 2 + 1 + payload.size() + 1);
            frame.reserve(length);
            frame.push_back(0xA5);
            frame.push_back(0x5A);
            frame.push_back(static_cast<uint8_t>(length & 0xFF));
            frame.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
            frame.push_back(rs485_id);
            frame.insert(frame.end(), payload.begin(), payload.end());
            frame.push_back(crc8(frame));
            return frame;
        }

        std::vector<uint8_t> powerControlBytes(const PowerConfig &config, PowerControlCommand command)
        {
            switch (command)
            {
            case PowerControlCommand::OpenUpperLimbs:
                return makePowerFrame(config.rs485_id, {0x01, 0x01, 0x01, 0x00, 0x00, 0x64, 0x00,
                                                        0x64, 0x00, 0x64, 0x00, 0x64, 0x00});
            case PowerControlCommand::OpenLowerLimbs:
                return makePowerFrame(config.rs485_id, {0x01, 0x00, 0x00, 0x01, 0x01, 0x64, 0x00,
                                                        0x64, 0x00, 0x64, 0x00, 0x64, 0x00});
            case PowerControlCommand::OpenLimbs:
                return makePowerFrame(config.rs485_id, {0x01, 0x01, 0x01, 0x01, 0x01, 0x64, 0x00,
                                                        0x64, 0x00, 0x64, 0x00, 0x64, 0x00});
            case PowerControlCommand::CloseLimbs:
                return makePowerFrame(config.rs485_id, {0x01, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00,
                                                        0x64, 0x00, 0x64, 0x00, 0x64, 0x00});
            case PowerControlCommand::OpenDexterousHands:
                return makePowerFrame(config.rs485_id, {0x02, 0x01, 0x01, 0x01, 0x01});
            case PowerControlCommand::CloseDexterousHands:
                return makePowerFrame(config.rs485_id, {0x02, 0x00, 0x00, 0x00, 0x00});
            case PowerControlCommand::FanControl:
                return makePowerFrame(config.rs485_id, {0x05,
                                                        config.fan_pwm_percent[0],
                                                        config.fan_pwm_percent[1],
                                                        config.fan_pwm_percent[2],
                                                        config.fan_pwm_percent[3],
                                                        config.fan_pwm_percent[4],
                                                        config.fan_pwm_percent[5]});
            }
            return {};
        }

        std::vector<std::vector<uint8_t>> powerControlFrames(const PowerConfig &config, PowerControlCommand command)
        {
            if (command == PowerControlCommand::OpenLimbs)
            {
                return {
                    powerControlBytes(config, PowerControlCommand::OpenUpperLimbs),
                    powerControlBytes(config, PowerControlCommand::OpenLimbs),
                };
            }

            auto bytes = powerControlBytes(config, command);
            if (bytes.empty())
            {
                return {};
            }
            return {bytes};
        }

        std::vector<uint8_t> flattenFrames(const std::vector<std::vector<uint8_t>> &frames)
        {
            std::vector<uint8_t> out;
            for (const auto &frame : frames)
            {
                out.insert(out.end(), frame.begin(), frame.end());
            }
            return out;
        }

        std::string powerControlName(PowerControlCommand command)
        {
            switch (command)
            {
            case PowerControlCommand::OpenLimbs:
                return "open-limbs-power";
            case PowerControlCommand::OpenUpperLimbs:
                return "open-upper-limbs-power";
            case PowerControlCommand::OpenLowerLimbs:
                return "open-lower-limbs-power";
            case PowerControlCommand::CloseLimbs:
                return "close-limbs-power";
            case PowerControlCommand::OpenDexterousHands:
                return "open-dexterous-hands-power";
            case PowerControlCommand::CloseDexterousHands:
                return "close-dexterous-hands-power";
            case PowerControlCommand::FanControl:
                return "fan-control";
            }
            return "unknown";
        }

        bool parsePowerControlFrame(
            const std::vector<uint8_t> &data, PowerControlResponse &out, std::string &error)
        {
            if (data.size() < 6)
            {
                error = "power control response length is less than 6 bytes";
                return false;
            }
            if (data[0] != 0xA5 || data[1] != 0x5A)
            {
                error = "power control response header is invalid";
                return false;
            }

            const uint16_t length = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
            const size_t total_size = static_cast<size_t>(length);
            if (length < 6)
            {
                error = "power control response frame length is invalid";
                return false;
            }
            if (data.size() < total_size)
            {
                error = "power control response frame is incomplete";
                return false;
            }

            std::vector<uint8_t> crc_data(data.begin(), data.begin() + total_size - 1);
            const uint8_t expected_crc = crc8(crc_data);
            const uint8_t actual_crc = data[total_size - 1];
            if (actual_crc != expected_crc)
            {
                std::ostringstream oss;
                oss << "power control response CRC mismatch, expected=0x" << std::uppercase << std::hex
                    << std::setw(2) << std::setfill('0') << static_cast<int>(expected_crc) << ", actual=0x"
                    << std::setw(2) << static_cast<int>(actual_crc);
                error = oss.str();
                return false;
            }

            out.raw_frame.assign(data.begin(), data.begin() + total_size);
            out.command = data.size() > 6 ? data[6] : 0;
            out.result = data.size() > 7 ? data[7] : 0;
            out.success = out.result == 0x01;
            out.message = out.success ? "power control command succeeded"
                                      : "power control command failed, result=" + std::to_string(out.result);
            return true;
        }

        Result readPowerControlResponse(int fd, const PowerConfig &config, PowerControlResponse &response)
        {
            std::vector<uint8_t> buffer;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(config.response_timeout_ms);
            while (std::chrono::steady_clock::now() < deadline)
            {
                uint8_t temp[128];
                const ssize_t n = ::read(fd, temp, sizeof(temp));
                if (n > 0)
                {
                    buffer.insert(buffer.end(), temp, temp + n);
                    auto it = std::find(buffer.begin(), buffer.end(), 0xA5);
                    while (it != buffer.end() && std::distance(it, buffer.end()) >= 2 && *(it + 1) != 0x5A)
                    {
                        it = std::find(it + 1, buffer.end(), 0xA5);
                    }
                    if (it != buffer.end())
                    {
                        if (it != buffer.begin())
                            buffer.erase(buffer.begin(), it);

                        if (buffer.size() >= 5)
                        {
                            const uint16_t length =
                                static_cast<uint16_t>(buffer[2]) | (static_cast<uint16_t>(buffer[3]) << 8);
                            const size_t total_size = static_cast<size_t>(length);
                            if (buffer.size() >= total_size)
                            {
                                std::vector<uint8_t> frame(buffer.begin(), buffer.begin() + total_size);
                                std::string error;
                                if (!parsePowerControlFrame(frame, response, error))
                                {
                                    response.raw_frame = buffer;
                                    return makeResult(false, 1, error + ", rx_buffer=" + bytesToHex(buffer));
                                }
                                return makeResult(response.success, response.success ? 0 : 1, response.message);
                            }
                        }
                    }
                }
                else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                {
                    const int code = errno;
                    return makeResult(false, code, std::string("read power control response failed: ") + std::strerror(code));
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }

            if (!buffer.empty())
            {
                response.raw_frame = buffer;
                return makeResult(false, 2, "power control response timeout, rx_buffer=" + bytesToHex(buffer));
            }
            return makeResult(false, 2, "power control response timeout");
        }

        int openRs485Serial(const Rs485SerialConfig &config, Result &error_result)
        {
            int fd = ::open(config.device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
            if (fd < 0)
            {
                error_result =
                    makeResult(false, errno, "open " + config.label + " serial failed: " + std::strerror(errno));
                return -1;
            }

            termios tio{};
            if (tcgetattr(fd, &tio) != 0)
            {
                const int code = errno;
                ::close(fd);
                error_result = makeResult(false, code, std::string("tcgetattr failed: ") + std::strerror(code));
                return -1;
            }
            const speed_t baud = baudToConstant(config.baudrate);
            if (baud == 0)
            {
                ::close(fd);
                error_result = makeResult(false, 1, "unsupported " + config.label + " baudrate");
                return -1;
            }
            cfmakeraw(&tio);
            cfsetispeed(&tio, baud);
            cfsetospeed(&tio, baud);
            tio.c_cflag &= ~CSIZE;
            tio.c_cflag |= CS8 | CLOCAL | CREAD;
            tio.c_cflag &= ~PARENB;
            tio.c_cflag &= ~CSTOPB;
#ifdef CRTSCTS
            tio.c_cflag &= ~CRTSCTS;
#endif
            tio.c_cc[VTIME] = 0;
            tio.c_cc[VMIN] = 0;
            tcflush(fd, TCIOFLUSH);
            if (tcsetattr(fd, TCSANOW, &tio) != 0)
            {
                const int code = errno;
                ::close(fd);
                error_result = makeResult(false, code, std::string("tcsetattr failed: ") + std::strerror(code));
                return -1;
            }

            error_result = makeResult(true, 0, config.label + " serial opened");
            return fd;
        }

        int openBmsSerial(const BmsConfig &config, Result &error_result)
        {
            return openRs485Serial(makeBmsRs485Config(config), error_result);
        }

        int openPowerSerial(const PowerConfig &config, Result &error_result)
        {
            return openRs485Serial(makePowerRs485Config(config), error_result);
        }

        Result writeBmsBytes(
            int fd, const BmsConfig &config, const uint8_t *data, size_t size, const std::string &what)
        {
            return writeRs485Bytes(fd, makeBmsRs485Config(config), data, size, what);
        }

        Result writePowerBytes(
            int fd, const PowerConfig &config, const uint8_t *data, size_t size, const std::string &what)
        {
            return writeRs485Bytes(fd, makePowerRs485Config(config), data, size, what);
        }

    } // namespace

    BmsInfo::BmsInfo()
    {
        cell_voltages.resize(13, 0.0f);
    }

    HardwareDriver::HardwareDriver() = default;
    HardwareDriver::~HardwareDriver() = default;

    Result HardwareDriver::init(const std::string &config_path)
    {
        if (!config_path.empty() && !pathExists(config_path))
        {
            return makeResult(false, 1, "config file does not exist: " + config_path);
        }
        return makeResult(true, 0, config_path.empty() ? "initialized with defaults" : "initialized: " + config_path);
    }

    Result HardwareDriver::configureSpi(const SpiConfig &config)
    {
        int fd = ::open(config.device.c_str(), O_RDWR);
        if (fd < 0)
        {
            return makeResult(false, errno, std::string("open SPI failed: ") + std::strerror(errno));
        }

        uint8_t mode = config.mode;
        uint8_t bits = config.bits_per_word;
        uint32_t speed = config.speed_hz;
        uint8_t lsb = config.lsb_first;
        auto fail = [&](const std::string &what)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, what + ": " + std::strerror(code));
        };

        if (::ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
            return fail("SPI_IOC_WR_MODE");
        if (::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
            return fail("SPI_IOC_WR_BITS_PER_WORD");
        if (::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
            return fail("SPI_IOC_WR_MAX_SPEED_HZ");
        if (::ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb) < 0)
            return fail("SPI_IOC_WR_LSB_FIRST");

        if (config.verify)
        {
            uint8_t rd_mode = 0;
            uint8_t rd_bits = 0;
            uint32_t rd_speed = 0;
            uint8_t rd_lsb = 0;
            if (::ioctl(fd, SPI_IOC_RD_MODE, &rd_mode) < 0)
                return fail("SPI_IOC_RD_MODE");
            if (::ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &rd_bits) < 0)
                return fail("SPI_IOC_RD_BITS_PER_WORD");
            if (::ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &rd_speed) < 0)
                return fail("SPI_IOC_RD_MAX_SPEED_HZ");
            if (::ioctl(fd, SPI_IOC_RD_LSB_FIRST, &rd_lsb) < 0)
                return fail("SPI_IOC_RD_LSB_FIRST");
            if (rd_mode != mode || rd_bits != bits || rd_speed != speed || rd_lsb != lsb)
            {
                ::close(fd);
                return makeResult(false, 1, "SPI verification mismatch");
            }
        }

        ::close(fd);
        appendLogLine(config.log_file, "{\"timestamp\":\"" + nowIso8601() + "\",\"device\":\"" +
                                           jsonEscape(config.device) + "\",\"ok\":true}");
        return makeResult(true, 0, "SPI configured: " + config.device);
    }

    Result HardwareDriver::querySpi(const std::string &device, SpiState &state)
    {
        state = SpiState{};
        state.device = device;

        int fd = ::open(device.c_str(), O_RDONLY);
        if (fd < 0)
        {
            const auto result = makeResult(false, errno, std::string("open SPI failed: ") + std::strerror(errno));
            state.ok = result.ok;
            state.code = result.code;
            state.message = result.message;
            return result;
        }

        uint8_t mode = 0;
        uint8_t bits = 0;
        uint32_t speed = 0;
        uint8_t lsb = 0;
        auto fail = [&](const std::string &what)
        {
            const int code = errno;
            ::close(fd);
            const auto result = makeResult(false, code, what + ": " + std::strerror(code));
            state.ok = result.ok;
            state.code = result.code;
            state.message = result.message;
            return result;
        };

        if (::ioctl(fd, SPI_IOC_RD_MODE, &mode) < 0)
            return fail("SPI_IOC_RD_MODE");
        if (::ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0)
            return fail("SPI_IOC_RD_BITS_PER_WORD");
        if (::ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0)
            return fail("SPI_IOC_RD_MAX_SPEED_HZ");
        if (::ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb) < 0)
            return fail("SPI_IOC_RD_LSB_FIRST");

        ::close(fd);
        state.device = device;
        state.mode = mode;
        state.bits_per_word = bits;
        state.speed_hz = speed;
        state.lsb_first = lsb;
        const auto result = makeResult(true, 0, "SPI query ok: " + device);
        state.ok = result.ok;
        state.code = result.code;
        state.message = result.message;
        return result;
    }

    Result HardwareDriver::querySpiDevices(std::vector<SpiState> &devices)
    {
        devices.clear();
        const auto paths = listSpiDevicePaths();
        devices.reserve(paths.size());

        for (const auto &path : paths)
        {
            SpiState state;
            const auto result = querySpi(path, state);
            state.ok = result.ok;
            state.code = result.code;
            state.message = result.message;
            devices.push_back(std::move(state));
        }

        return makeResult(true, 0, "SPI query ok, devices=" + std::to_string(devices.size()));
    }

    Result HardwareDriver::queryUsbDevices(const UsbQueryOptions &options, std::vector<UsbDeviceInfo> &devices)
    {
        devices.clear();
        std::error_code ec;
        std::set<std::string> seen;
        const auto dev_nodes_by_major_minor = listDevNodesByMajorMinor();

        const fs::path usb_bus("/sys/bus/usb/devices");
        if (fs::exists(usb_bus, ec))
        {
            for (const auto &usb_entry : fs::directory_iterator(usb_bus, ec))
            {
                if (ec)
                {
                    break;
                }
                const fs::path usb_path = fs::weakly_canonical(usb_entry.path(), ec);
                const fs::path scan_path = ec ? usb_entry.path() : usb_path;
                if (!pathExists(scan_path / "idVendor") || !pathExists(scan_path / "idProduct"))
                {
                    continue;
                }

                UsbDeviceInfo info;
                info.sysfs_path = scan_path.string();
                info.subsystem = "usb";
                info.driver = readLinkName(scan_path / "driver");
                fillUsbDeviceFields(scan_path, info);

                if (!usbInfoMatches(options, info))
                {
                    continue;
                }

                const std::string key = info.subsystem + "|" + info.devnode + "|" + info.sysfs_path;
                if (!seen.insert(key).second)
                {
                    continue;
                }
                devices.push_back(std::move(info));
            }
        }

        const fs::path sys_class("/sys/class");
        if (!fs::exists(sys_class, ec))
        {
            return makeResult(false, 1, "/sys/class does not exist");
        }

        for (const auto &subsystem_entry : fs::directory_iterator(sys_class, ec))
        {
            if (ec)
            {
                break;
            }
            if (!subsystem_entry.is_directory(ec))
            {
                continue;
            }
            const std::string subsystem = subsystem_entry.path().filename().string();
            if (!options.subsystem.empty() && subsystem != options.subsystem)
            {
                continue;
            }

            std::error_code dev_ec;
            for (const auto &device_entry : fs::directory_iterator(subsystem_entry.path(), dev_ec))
            {
                if (dev_ec)
                {
                    break;
                }

                const std::string name = device_entry.path().filename().string();
                const fs::path physical_path = fs::weakly_canonical(device_entry.path(), ec);
                const fs::path scan_path = ec ? device_entry.path() : physical_path;

                fs::path usb_device_path;
                if (!findUsbDevicePath(scan_path, usb_device_path))
                {
                    continue;
                }

                UsbDeviceInfo info;
                info.sysfs_path = scan_path.string();
                info.devnode = lookupDevnode(dev_nodes_by_major_minor, scan_path);
                if (info.devnode.empty())
                {
                    info.devnode = devnodeForClassDevice(subsystem, name);
                }
                if (!info.devnode.empty() && !pathExists(info.devnode))
                {
                    info.devnode.clear();
                }
                info.subsystem = subsystem;
                info.driver = readLinkName(scan_path / "driver");
                fillUsbDeviceFields(usb_device_path, info);

                fs::path interface_path;
                findUsbInterfacePath(scan_path, interface_path);
                fillUsbInterfaceFields(interface_path, info);

                if (!usbInfoMatches(options, info))
                {
                    continue;
                }

                const std::string key = info.subsystem + "|" + info.devnode + "|" + info.sysfs_path;
                if (!seen.insert(key).second)
                {
                    continue;
                }
                devices.push_back(std::move(info));
            }
        }

        std::sort(devices.begin(), devices.end(), [](const UsbDeviceInfo &lhs, const UsbDeviceInfo &rhs)
                  {
        if (lhs.devnode != rhs.devnode)
        {
            return lhs.devnode < rhs.devnode;
        }
        return lhs.sysfs_path < rhs.sysfs_path; });

        std::ostringstream log;
        log << "{\"timestamp\":\"" << nowIso8601() << "\",\"count\":" << devices.size() << ",\"devices\":[";
        for (size_t i = 0; i < devices.size(); ++i)
        {
            if (i)
            {
                log << ",";
            }
            log << usbDeviceToJson(devices[i]);
        }
        log << "]}";
        appendLogLine(options.log_file, log.str());

        return makeResult(true, 0, "USB query ok, devices=" + std::to_string(devices.size()));
    }

    Result HardwareDriver::readUsbDevice(const UsbReadConfig &config, std::vector<uint8_t> &data)
    {
        data.clear();
        if (config.devnode.empty())
        {
            return makeResult(false, 1, "USB devnode is empty");
        }

        const uint32_t max_bytes = std::clamp<uint32_t>(config.max_bytes, 1, 4096);
        const int timeout_ms = std::max(0, config.timeout_ms);
        int fd = ::open(config.devnode.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
        {
            return makeResult(false, errno, std::string("open USB device failed: ") + std::strerror(errno));
        }

        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int ready = ::poll(&pfd, 1, timeout_ms);
        if (ready < 0)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, std::string("poll USB device failed: ") + std::strerror(code));
        }
        if (ready == 0)
        {
            ::close(fd);
            return makeResult(false, 2, "USB read timeout");
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        {
            ::close(fd);
            return makeResult(false, 3, "USB device is not readable");
        }

        data.resize(max_bytes);
        const ssize_t n = ::read(fd, data.data(), data.size());
        if (n < 0)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, std::string("read USB device failed: ") + std::strerror(code));
        }
        data.resize(static_cast<size_t>(n));
        ::close(fd);

        appendLogLine(config.log_file, "{\"timestamp\":\"" + nowIso8601() + "\",\"devnode\":\"" +
                                           jsonEscape(config.devnode) + "\",\"bytes_read\":" +
                                           std::to_string(data.size()) + "}");
        return makeResult(true, 0, "USB read ok, bytes=" + std::to_string(data.size()));
    }

    Result HardwareDriver::configureUsbSerial(const UsbSerialConfig &config)
    {
        int fd = ::open(config.device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0)
        {
            return makeResult(false, errno, std::string("open serial failed: ") + std::strerror(errno));
        }
        termios tio{};
        if (tcgetattr(fd, &tio) != 0)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, std::string("tcgetattr failed: ") + std::strerror(code));
        }

        const speed_t baud = baudToConstant(config.baudrate);
        if (baud == 0)
        {
            ::close(fd);
            return makeResult(false, 1, "unsupported baudrate");
        }
        cfmakeraw(&tio);
        cfsetispeed(&tio, baud);
        cfsetospeed(&tio, baud);
        tio.c_cflag &= ~CSIZE;
        switch (config.databits)
        {
        case 5:
            tio.c_cflag |= CS5;
            break;
        case 6:
            tio.c_cflag |= CS6;
            break;
        case 7:
            tio.c_cflag |= CS7;
            break;
        case 8:
            tio.c_cflag |= CS8;
            break;
        default:
            ::close(fd);
            return makeResult(false, 1, "databits must be 5, 6, 7, or 8");
        }
        if (config.parity == "none")
        {
            tio.c_cflag &= ~PARENB;
        }
        else if (config.parity == "even")
        {
            tio.c_cflag |= PARENB;
            tio.c_cflag &= ~PARODD;
        }
        else if (config.parity == "odd")
        {
            tio.c_cflag |= PARENB;
            tio.c_cflag |= PARODD;
        }
        else
        {
            ::close(fd);
            return makeResult(false, 1, "parity must be none, even, or odd");
        }
        if (config.stopbits == 1)
            tio.c_cflag &= ~CSTOPB;
        else if (config.stopbits == 2)
            tio.c_cflag |= CSTOPB;
        else
        {
            ::close(fd);
            return makeResult(false, 1, "stopbits must be 1 or 2");
        }
#ifdef CRTSCTS
        if (config.rtscts)
            tio.c_cflag |= CRTSCTS;
        else
            tio.c_cflag &= ~CRTSCTS;
#endif
        tio.c_cflag |= CLOCAL | CREAD;
        if (tcsetattr(fd, TCSANOW, &tio) != 0)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, std::string("tcsetattr failed: ") + std::strerror(code));
        }
        if (config.verify)
        {
            termios actual{};
            if (tcgetattr(fd, &actual) != 0 || constantToBaud(cfgetispeed(&actual)) != config.baudrate)
            {
                ::close(fd);
                return makeResult(false, 1, "serial verification mismatch");
            }
        }
        ::close(fd);
        appendLogLine(config.log_file, "{\"timestamp\":\"" + nowIso8601() + "\",\"device\":\"" +
                                           jsonEscape(config.device) + "\",\"type\":\"serial\",\"ok\":true}");
        return makeResult(true, 0, "USB serial configured: " + config.device);
    }

    Result HardwareDriver::configureUsbVideo(const UsbVideoConfig &config)
    {
        if (config.pixfmt.size() != 4)
        {
            return makeResult(false, 1, "pixfmt must be a 4-character FOURCC");
        }
        int fd = ::open(config.device.c_str(), O_RDWR | O_NONBLOCK);
        if (fd < 0)
        {
            return makeResult(false, errno, std::string("open video failed: ") + std::strerror(errno));
        }
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = config.width;
        fmt.fmt.pix.height = config.height;
        fmt.fmt.pix.pixelformat = fourccFromString(config.pixfmt);
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        if (::ioctl(fd, VIDIOC_S_FMT, &fmt) != 0)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, std::string("VIDIOC_S_FMT failed: ") + std::strerror(code));
        }

        v4l2_streamparm parm{};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = config.fps;
        if (::ioctl(fd, VIDIOC_S_PARM, &parm) != 0)
        {
            const int code = errno;
            ::close(fd);
            return makeResult(false, code, std::string("VIDIOC_S_PARM failed: ") + std::strerror(code));
        }
        if (config.verify)
        {
            v4l2_format actual{};
            actual.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (::ioctl(fd, VIDIOC_G_FMT, &actual) != 0 || actual.fmt.pix.width != config.width ||
                actual.fmt.pix.height != config.height || fourccToString(actual.fmt.pix.pixelformat) != config.pixfmt)
            {
                ::close(fd);
                return makeResult(false, 1, "video verification mismatch");
            }
        }
        ::close(fd);
        appendLogLine(config.log_file, "{\"timestamp\":\"" + nowIso8601() + "\",\"device\":\"" +
                                           jsonEscape(config.device) + "\",\"type\":\"video\",\"ok\":true}");
        return makeResult(true, 0, "USB video configured: " + config.device);
    }

    Result HardwareDriver::queryBmsInfo(const BmsConfig &config, BmsInfo &info)
    {
        Result open_result;
        int fd = openBmsSerial(config, open_result);
        if (fd < 0)
            return open_result;

        const uint8_t query[] = {0x4A, 0x01, 0x02, 0x01, 0x01, 0x00, 0x03, 0x0E};
        const auto write_result = writeBmsBytes(fd, config, query, sizeof(query), "BMS query");
        if (!write_result.ok)
        {
            ::close(fd);
            return write_result;
        }

        std::vector<uint8_t> buffer;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(config.response_timeout_ms);
        while (std::chrono::steady_clock::now() < deadline)
        {
            uint8_t temp[128];
            const ssize_t n = ::read(fd, temp, sizeof(temp));
            if (n > 0)
            {
                buffer.insert(buffer.end(), temp, temp + n);
                auto it = std::find(buffer.begin(), buffer.end(), 0x4B);
                if (it != buffer.end())
                {
                    if (it != buffer.begin())
                    {
                        buffer.erase(buffer.begin(), it);
                    }
                    if (buffer.size() >= 61 && buffer[60] == 0x0D)
                    {
                        std::vector<uint8_t> frame(buffer.begin(), buffer.begin() + 61);
                        std::string error;
                        if (!parseBmsFrame(frame, info, error))
                        {
                            ::close(fd);
                            return makeResult(false, 1, error);
                        }
                        ::close(fd);
                        appendLogLine(config.log_file, bmsInfoToJson(info));
                        return makeResult(true, 0, "BMS query ok");
                    }
                }
            }
            else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            {
                const int code = errno;
                ::close(fd);
                return makeResult(false, code, std::string("read BMS response failed: ") + std::strerror(code));
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        ::close(fd);
        return makeResult(false, 2, "BMS response timeout");
    }

    Result HardwareDriver::sendBmsControl(const BmsConfig &config, BmsControlCommand command, BmsControlResponse &response)
    {
        const auto bytes = bmsControlBytes(command);
        response.request_frame = bytes;
        if (bytes.empty())
        {
            return makeResult(false, 1, "unsupported BMS control command");
        }

        Result open_result;
        int fd = openBmsSerial(config, open_result);
        if (fd < 0)
            return open_result;

        const auto write_result = writeBmsBytes(fd, config, bytes.data(), bytes.size(), "BMS control command");
        if (!write_result.ok)
        {
            ::close(fd);
            return write_result;
        }

        std::vector<uint8_t> buffer;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(config.response_timeout_ms);
        while (std::chrono::steady_clock::now() < deadline)
        {
            uint8_t temp[128];
            const ssize_t n = ::read(fd, temp, sizeof(temp));
            if (n > 0)
            {
                buffer.insert(buffer.end(), temp, temp + n);
                auto it = std::find(buffer.begin(), buffer.end(), 0x5A);
                if (it != buffer.end())
                {
                    if (it != buffer.begin())
                        buffer.erase(buffer.begin(), it);
                    if (buffer.size() >= 8 && buffer[7] == 0x0D)
                    {
                        std::vector<uint8_t> frame(buffer.begin(), buffer.begin() + 8);
                        std::string error;
                        if (!parseBmsControlFrame(frame, response, error))
                        {
                            ::close(fd);
                            return makeResult(false, 1, error);
                        }
                        ::close(fd);
                        std::ostringstream log;
                        log << "{\"timestamp\":\"" << nowIso8601() << "\",";
                        log << "\"command\":\"" << bmsControlName(command) << "\",";
                        log << "\"success\":" << (response.success ? "true" : "false") << ",";
                        log << "\"response_code\":" << static_cast<int>(response.response_code) << ",";
                        log << "\"raw_frame\":\"" << bytesToHex(response.raw_frame) << "\"}";
                        appendLogLine(config.log_file, log.str());
                        return makeResult(response.success, response.success ? 0 : 1, response.message);
                    }
                }
            }
            else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            {
                const int code = errno;
                ::close(fd);
                return makeResult(false, code, std::string("read BMS control response failed: ") + std::strerror(code));
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        ::close(fd);
        return makeResult(false, 2, "BMS control response timeout");
    }

    Result HardwareDriver::sendPowerControl(
        const PowerConfig &config, PowerControlCommand command, PowerControlResponse &response)
    {
        const auto frames = powerControlFrames(config, command);
        response.request_frame = flattenFrames(frames);
        if (frames.empty())
        {
            return makeResult(false, 1, "unsupported power control command");
        }

        Result open_result;
        int fd = openPowerSerial(config, open_result);
        if (fd < 0)
            return open_result;

        if (frames.size() > 1)
        {
            response.success = true;
        }

        for (size_t i = 0; i < frames.size(); ++i)
        {
            const auto &bytes = frames[i];
            const auto write_result = writePowerBytes(fd, config, bytes.data(), bytes.size(), "power control command");
            const auto frame_sent_at = std::chrono::steady_clock::now();
            if (!write_result.ok)
            {
                ::close(fd);
                return write_result;
            }

            if (frames.size() > 1)
            {
                PowerControlResponse frame_response;
                const auto frame_result = readPowerControlResponse(fd, config, frame_response);
                if (!frame_response.raw_frame.empty())
                {
                    response.raw_frame.insert(
                        response.raw_frame.end(), frame_response.raw_frame.begin(), frame_response.raw_frame.end());
                }
                response.command = frame_response.command;
                response.result = frame_response.result;

                if (i != 0)
                {
                    response.message += "; ";
                }
                response.message += "frame " + std::to_string(i + 1) + "/" + std::to_string(frames.size()) + ": " +
                                    frame_result.message;

                if (!frame_result.ok)
                {
                    response.success = false;
                }

                if (i + 1 < frames.size())
                {
                    const auto elapsed = std::chrono::steady_clock::now() - frame_sent_at;
                    const auto command_interval = std::chrono::milliseconds(30);
                    if (elapsed < command_interval)
                    {
                        std::this_thread::sleep_for(command_interval - elapsed);
                    }
                }
                continue;
            }

            if (i + 1 < frames.size())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        }

        if (frames.size() > 1)
        {
            ::close(fd);
            const auto final_result =
                makeResult(response.success, response.success ? 0 : 1,
                           response.message.empty() ? "power control command failed" : response.message);
            if (!final_result.ok)
            {
                return final_result;
            }

            std::ostringstream log;
            log << "{\"timestamp\":\"" << nowIso8601() << "\",";
            log << "\"command\":\"" << powerControlName(command) << "\",";
            log << "\"success\":" << (response.success ? "true" : "false") << ",";
            log << "\"result\":" << static_cast<int>(response.result) << ",";
            log << "\"request_frame\":\"" << bytesToHex(response.request_frame) << "\",";
            log << "\"raw_frame\":\"" << bytesToHex(response.raw_frame) << "\"}";
            appendLogLine(config.log_file, log.str());
            return final_result;
        }

        const auto final_result = readPowerControlResponse(fd, config, response);
        ::close(fd);
        if (!final_result.ok)
        {
            return final_result;
        }

        std::ostringstream log;
        log << "{\"timestamp\":\"" << nowIso8601() << "\",";
        log << "\"command\":\"" << powerControlName(command) << "\",";
        log << "\"success\":" << (response.success ? "true" : "false") << ",";
        log << "\"result\":" << static_cast<int>(response.result) << ",";
        log << "\"request_frame\":\"" << bytesToHex(response.request_frame) << "\",";
        log << "\"raw_frame\":\"" << bytesToHex(response.raw_frame) << "\"}";
        appendLogLine(config.log_file, log.str());
        return final_result;
    }

    std::string driverVersion()
    {
        return "1.0.0";
    }

} // namespace robot_embeded
