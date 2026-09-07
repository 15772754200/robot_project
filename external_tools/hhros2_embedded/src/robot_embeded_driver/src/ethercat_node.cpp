#include "rclcpp/rclcpp.hpp"
#include "robot_embeded_interfaces/msg/ethercat_master.hpp"
#include "robot_embeded_interfaces/msg/ethercat_slave.hpp"
#include "robot_embeded_interfaces/srv/query_ethercat_masters.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

namespace
{
constexpr const char* ETHERCAT_MASTER_COMMAND = "ethercat master 2>&1";

struct CommandResult
{
    bool ok = false;
    int code = 0;
    std::string output;
    std::string message;
};

std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

CommandResult runCommand(const std::string& command)
{
    CommandResult result;
    FILE* pipe = ::popen(command.c_str(), "r");
    if (pipe == nullptr)
    {
        result.code = errno;
        result.message = std::string("popen failed: ") + std::strerror(errno);
        return result;
    }

    std::array<char, 256> buffer {};
    std::ostringstream output;
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
    {
        output << buffer.data();
    }

    const int status = ::pclose(pipe);
    result.output = trim(output.str());
    if (status == -1)
    {
        result.code = errno;
        result.message = std::string("pclose failed: ") + std::strerror(errno);
        return result;
    }
    if (!WIFEXITED(status))
    {
        result.code = status;
        result.message = "ethercat command did not exit normally";
        return result;
    }

    result.code = WEXITSTATUS(status);
    result.ok = result.code == 0;
    result.message = result.ok ? "EtherCAT master query ok" : "ethercat command failed";
    return result;
}

int countEthercatMasters(const std::string& output)
{
    int count = 0;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.rfind("Master", 0) == 0)
        {
            ++count;
        }
    }
    return count;
}

std::vector<robot_embeded_interfaces::msg::EthercatMaster> parseMasters(const std::string& output)
{
    std::vector<robot_embeded_interfaces::msg::EthercatMaster> masters;
    robot_embeded_interfaces::msg::EthercatMaster current;
    bool in_master = false;
    static const std::regex master_index_pattern(R"(^Master\s*(\d+).*)");

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line))
    {
        const auto text = trim(line);
        if (text.rfind("Master", 0) == 0)
        {
            if (in_master)
            {
                masters.push_back(current);
            }

            current = robot_embeded_interfaces::msg::EthercatMaster {};
            current.master_index = static_cast<int32_t>(masters.size());
            std::smatch match;
            if (std::regex_match(text, match, master_index_pattern))
            {
                current.master_index = std::stoi(match[1].str());
            }
            current.device_name = text;
            current.state = "unknown";
            current.connected = false;
            current.slave_count = 0;
            current.message = text;
            in_master = true;
            continue;
        }

        if (!in_master)
        {
            continue;
        }

        if (text.rfind("Phase:", 0) == 0)
        {
            current.state = trim(text.substr(std::string("Phase:").size()));
        }
        else if (text.rfind("Active:", 0) == 0)
        {
            const auto active = trim(text.substr(std::string("Active:").size()));
            current.connected = active == "yes" || active == "true" || active == "1";
        }
        else if (text.rfind("Slaves:", 0) == 0)
        {
            try
            {
                current.slave_count = std::stoi(trim(text.substr(std::string("Slaves:").size())));
            }
            catch (const std::exception&)
            {
                current.slave_count = 0;
            }
        }
    }

    if (in_master)
    {
        masters.push_back(current);
    }
    return masters;
}

std::string makeEthercatSlavesCommand(int master_index)
{
    return "ethercat slaves -m " + std::to_string(master_index) + " 2>&1";
}

std::string makeEthercatNameCommand(int master_index, int position)
{
    std::ostringstream command;
    command << "ethercat upload -m " << master_index << " -p " << position
            << " 0x1008 0x00 --type string 2>&1";
    return command.str();
}

bool parseSlaveLine(const std::string& line, int master_index, robot_embeded_interfaces::msg::EthercatSlave& slave)
{
    static const std::regex position_state_pattern(R"(^\s*(\d+)\s+.*?\b(INIT|PREOP|SAFEOP|OP)\b(.*)$)");
    static const std::regex bracket_state_pattern(R"(^\s*(\d+)\s+.*?\[(INIT|PREOP|SAFEOP|OP)\](.*)$)");

    std::smatch match;
    if (!std::regex_match(line, match, position_state_pattern) &&
        !std::regex_match(line, match, bracket_state_pattern))
    {
        return false;
    }

    slave.master_index = master_index;
    slave.position = std::stoi(match[1].str());
    slave.state = match[2].str();
    slave.connected = true;
    slave.operational = slave.state == "OP";
    slave.message = slave.operational ? "slave state is OP" : "slave state is " + slave.state;
    return true;
}

std::vector<robot_embeded_interfaces::msg::EthercatSlave> parseSlaves(
    int master_index,
    const std::string& output)
{
    std::vector<robot_embeded_interfaces::msg::EthercatSlave> slaves;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line))
    {
        robot_embeded_interfaces::msg::EthercatSlave slave;
        if (parseSlaveLine(line, master_index, slave))
        {
            slaves.push_back(std::move(slave));
        }
    }
    return slaves;
}

std::string parseUploadStringOutput(const std::string& output)
{
    const auto trimmed = trim(output);
    if (trimmed.empty())
    {
        return "";
    }

    std::istringstream stream(trimmed);
    std::string line;
    std::string last_line;
    while (std::getline(stream, line))
    {
        line = trim(line);
        if (!line.empty())
        {
            last_line = line;
        }
    }

    if (last_line.size() >= 2 && last_line.front() == '"' && last_line.back() == '"')
    {
        return last_line.substr(1, last_line.size() - 2);
    }
    return last_line;
}

void fillSlaveDeviceName(robot_embeded_interfaces::msg::EthercatSlave& slave)
{
    const auto result = runCommand(makeEthercatNameCommand(slave.master_index, slave.position));
    if (!result.ok)
    {
        slave.device_name = "";
        slave.message += "; device name read failed: " + result.output;
        return;
    }

    slave.device_name = parseUploadStringOutput(result.output);
    if (slave.device_name.empty())
    {
        slave.message += "; device name is empty";
    }
}
} // namespace

class RobotEmbededEthercatNode : public rclcpp::Node
{
public:
    RobotEmbededEthercatNode()
        : Node("robot_embeded_ethercat_node")
    {
        query_srv_ = create_service<robot_embeded_interfaces::srv::QueryEthercatMasters>(
            "query_ethercat_masters",
            [this](
                std::shared_ptr<robot_embeded_interfaces::srv::QueryEthercatMasters::Request> request,
                std::shared_ptr<robot_embeded_interfaces::srv::QueryEthercatMasters::Response> response) {
                handleQueryEthercatMasters(request, response);
            });

        RCLCPP_INFO(get_logger(), "robot_embeded EtherCAT node started");
    }

private:
    void handleQueryEthercatMasters(
        const std::shared_ptr<robot_embeded_interfaces::srv::QueryEthercatMasters::Request> request,
        std::shared_ptr<robot_embeded_interfaces::srv::QueryEthercatMasters::Response> response)
    {
        (void)request;
        const auto command_result = runCommand(ETHERCAT_MASTER_COMMAND);
        response->code = command_result.code;
        response->command_output = command_result.output;
        response->masters = parseMasters(command_result.output);
        response->master_count = static_cast<int32_t>(response->masters.size());
        if (response->master_count == 0)
        {
            response->master_count = countEthercatMasters(command_result.output);
        }

        response->ok = command_result.ok;
        if (!command_result.ok)
        {
            response->message = command_result.message;
            return;
        }

        std::vector<int> master_indices;
        std::set<int> seen_master_indices;
        for (const auto& master : response->masters)
        {
            if (seen_master_indices.insert(master.master_index).second)
            {
                master_indices.push_back(master.master_index);
            }
        }
        if (master_indices.empty())
        {
            for (int master_index = 0; master_index < response->master_count; ++master_index)
            {
                master_indices.push_back(master_index);
            }
        }

        for (const int master_index : master_indices)
        {
            const auto slaves_result = runCommand(makeEthercatSlavesCommand(master_index));
            if (!slaves_result.ok)
            {
                response->ok = false;
                response->code = slaves_result.code;
                response->message = "ethercat slaves query failed for master " + std::to_string(master_index) +
                                    ": " + slaves_result.output;
                return;
            }

            auto slaves = parseSlaves(master_index, slaves_result.output);
            for (auto& slave : slaves)
            {
                fillSlaveDeviceName(slave);
                response->slaves.push_back(std::move(slave));
            }
            for (auto& master : response->masters)
            {
                if (master.master_index == master_index)
                {
                    master.slave_count = static_cast<int32_t>(slaves.size());
                    break;
                }
            }
        }

        if (response->master_count == 0)
        {
            response->message = "EtherCAT connected masters=0";
        }
        else
        {
            response->message = "EtherCAT connected masters=" + std::to_string(response->master_count) +
                                ", slaves=" + std::to_string(response->slaves.size());
        }
    }

    rclcpp::Service<robot_embeded_interfaces::srv::QueryEthercatMasters>::SharedPtr query_srv_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RobotEmbededEthercatNode>());
    rclcpp::shutdown();
    return 0;
}
