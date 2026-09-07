#include "hhros2_hal/exipc_system_hardware.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "hardware_interface/lexical_casts.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "rclcpp/rclcpp.hpp"
#include "hhros2_log/log.h"

/***debug log start***/
#include "probe/single_shot_probe.h"
/***debug log end***/

namespace hhros2_hal
{

    namespace
    {
        //constexpr char kLogger[] = "hhros2_hal.ExipcSystemHardware";
        constexpr int kJointCount = hhros2::shm::kJointCount;

        double imu_value_for(
            const std::string &iface, const hhros2::shm::ImuSample &s)
        {
            if (iface == "orientation.x")
                return s.orientation[0];
            if (iface == "orientation.y")
                return s.orientation[1];
            if (iface == "orientation.z")
                return s.orientation[2];
            if (iface == "orientation.w")
                return s.orientation[3];
            if (iface == "angular_velocity.x")
                return s.angular_velocity[0];
            if (iface == "angular_velocity.y")
                return s.angular_velocity[1];
            if (iface == "angular_velocity.z")
                return s.angular_velocity[2];
            if (iface == "linear_acceleration.x")
                return s.linear_acceleration[0];
            if (iface == "linear_acceleration.y")
                return s.linear_acceleration[1];
            if (iface == "linear_acceleration.z")
                return s.linear_acceleration[2];
            return 0.0;
        }
    } // namespace

    hardware_interface::CallbackReturn ExipcSystemHardware::on_init(
        const hardware_interface::HardwareInfo &info)
    {
        if (hardware_interface::SystemInterface::on_init(info) !=
            hardware_interface::CallbackReturn::SUCCESS)
        {
            return hardware_interface::CallbackReturn::ERROR;
        }

        if (static_cast<int>(info_.joints.size()) != kJointCount)
        {
            // RCLCPP_ERROR(
            //     rclcpp::get_logger(kLogger),
            //     "Expected %d joints, got %zu", kJointCount, info_.joints.size());
            LOG_ERROR(LogType::HALLOG,"Expected %d joints, got %zu", kJointCount, info_.joints.size());
            return hardware_interface::CallbackReturn::ERROR;
        }

        // hardware_parameters from the <ros2_control><hardware> tag
        auto it = info_.hardware_parameters.find("shm_name");
        if (it != info_.hardware_parameters.end() && !it->second.empty())
        {
            shm_name_ = it->second;
        }
        it = info_.hardware_parameters.find("exipc_port");
        if (it != info_.hardware_parameters.end() && !it->second.empty())
        {
            exipc_port_ = std::stoi(it->second);
        }

        const auto n = info_.joints.size();
        hw_pos_.assign(n, std::numeric_limits<double>::quiet_NaN());
        hw_vel_.assign(n, 0.0);
        hw_eff_.assign(n, 0.0);
        cmd_pos_.assign(n, std::numeric_limits<double>::quiet_NaN());
        cmd_vel_.assign(n, 0.0);
        cmd_eff_.assign(n, 0.0);
        cmd_kp_.assign(n, 0.0);
        cmd_kd_.assign(n, 0.0);
        fb_.assign(n, hhros2::shm::JointFeedback{});
        cmd_frame_.assign(n, hhros2::shm::JointCommand{});

        // Validate each joint exposes the hybrid impedance command set.
        for (const auto &joint : info_.joints)
        {
            if (joint.command_interfaces.size() < 5)
            {
                // RCLCPP_WARN(
                //     rclcpp::get_logger(kLogger),
                //     "Joint '%s' should export position/velocity/effort/kp/kd",
                //     joint.name.c_str());
                    LOG_WARNING(LogType::HALLOG,
                        "Joint '%s' should export position/velocity/effort/kp/kd",
                        joint.name.c_str());
            }
        }

        // Discover IMU sensor state interfaces declared in the URDF.
        for (const auto &sensor : info_.sensors)
        {
            for (const auto &si : sensor.state_interfaces)
            {
                imu_iface_names_.push_back(si.name);
            }
        }
        imu_states_.assign(imu_iface_names_.size(), 0.0);

        // RCLCPP_INFO(
        //     rclcpp::get_logger(kLogger),
        //     "Initialized: %zu joints, shm='%s', port=%d, imu_ifaces=%zu",
        //     n, shm_name_.c_str(), exipc_port_, imu_iface_names_.size());
        LOG_INFO(LogType::HALLOG,
            "Initialized: %zu joints, shm='%s', port=%d, imu_ifaces=%zu",
            n, shm_name_.c_str(), exipc_port_, imu_iface_names_.size());
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ExipcSystemHardware::on_configure(
        const rclcpp_lifecycle::State & /*previous_state*/)
    {
        // Attach to the L0 segment but do NOT enable torque: at Inactive the joints
        // stay passive while sensors begin reporting (commercial safe-startup rule).
        if (!client_.open(shm_name_, /*create=*/false))
        {
            // RCLCPP_ERROR(
            //     rclcpp::get_logger(kLogger),
            //     "Failed to attach shared memory '%s'. Is the L0 runtime up?",
            //     shm_name_.c_str());

            LOG_ERROR(LogType::HALLOG,
                "Failed to attach shared memory '%s'. Is the L0 runtime up?",
                shm_name_.c_str());

            return hardware_interface::CallbackReturn::ERROR;
        }
        read_miss_count_ = 0;
        // RCLCPP_INFO(rclcpp::get_loger(kLogger), "Configured: shm attached.");
        LOG_INFO(LogType::HALLOG, "Configured: shm attached.");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ExipcSystemHardware::on_cleanup(
        const rclcpp_lifecycle::State & /*previous_state*/)
    {
        client_.close();
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    std::vector<hardware_interface::StateInterface>
    ExipcSystemHardware::export_state_interfaces()
    {
        std::vector<hardware_interface::StateInterface> ifaces;
        std::cout << "Exporting state interfaces :" << std::endl;
        for (std::size_t i = 0; i < info_.joints.size(); ++i)
        {
            const auto &name = info_.joints[i].name;
            ifaces.emplace_back(
                name, hardware_interface::HW_IF_POSITION, &hw_pos_[i]);
            ifaces.emplace_back(
                name, hardware_interface::HW_IF_VELOCITY, &hw_vel_[i]);
            ifaces.emplace_back(
                name, hardware_interface::HW_IF_EFFORT, &hw_eff_[i]);
            std::cout << "joint[" << i << "]=" << name << std::endl;
        }
        // IMU sensor
        std::size_t k = 0;
        for (const auto &sensor : info_.sensors)
        {
            for (const auto &si : sensor.state_interfaces)
            {
                ifaces.emplace_back(sensor.name, si.name, &imu_states_[k]);
                ++k;
            }
        }
        return ifaces;
    }

    std::vector<hardware_interface::CommandInterface>
    ExipcSystemHardware::export_command_interfaces()
    {
        std::vector<hardware_interface::CommandInterface> ifaces;
        for (std::size_t i = 0; i < info_.joints.size(); ++i)
        {
            const auto &name = info_.joints[i].name;
            ifaces.emplace_back(
                name, hardware_interface::HW_IF_POSITION, &cmd_pos_[i]);
            ifaces.emplace_back(
                name, hardware_interface::HW_IF_VELOCITY, &cmd_vel_[i]);
            ifaces.emplace_back(
                name, hardware_interface::HW_IF_EFFORT, &cmd_eff_[i]);
            ifaces.emplace_back(name, "kp", &cmd_kp_[i]);
            ifaces.emplace_back(name, "kd", &cmd_kd_[i]);
        }
        return ifaces;
    }

    hardware_interface::CallbackReturn ExipcSystemHardware::on_activate(
        const rclcpp_lifecycle::State & /*previous_state*/)
    {
        // Seed command from the latest measured state so activation never produces
        // a jump. Stiffness starts at zero; the active controller raises it.
        if (client_.read_feedback(fb_.data(), kJointCount, &imu_sample_))
        {
            for (std::size_t i = 0; i < info_.joints.size(); ++i)
            {
                hw_pos_[i] = fb_[i].position;
                cmd_pos_[i] = fb_[i].position;
            }
        }
        hold_current_position();
        active_ = true;
        // RCLCPP_INFO(rclcpp::get_logger(kLogger), "Activated (zero-stiffness hold).");
        LOG_INFO(LogType::HALLOG, "Activated (zero-stiffness hold).");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ExipcSystemHardware::on_deactivate(
        const rclcpp_lifecycle::State & /*previous_state*/)
    {
        active_ = false;
        hold_current_position();
        client_.write_command(cmd_frame_.data(), kJointCount);
        // RCLCPP_INFO(rclcpp::get_logger(kLogger), "Deactivated (released).");
        LOG_INFO(LogType::HALLOG, "Deactivated (released).");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    void ExipcSystemHardware::hold_current_position()
    {
        for (std::size_t i = 0; i < info_.joints.size(); ++i)
        {
            cmd_vel_[i] = 0.0;
            cmd_eff_[i] = 0.0;
            cmd_kp_[i] = 0.0;
            cmd_kd_[i] = 0.0;
            cmd_frame_[i] = hhros2::shm::JointCommand{cmd_pos_[i], 0.0, 0.0, 0.0, 0.0};
        }
    }

    hardware_interface::return_type ExipcSystemHardware::read(
        const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
    {
        if (!client_.read_feedback(fb_.data(), kJointCount, &imu_sample_))
        {
            // Tolerate transient torn reads; escalate after a sustained gap so
            // hhros2_core sees a stale-feedback fault.
            if (++read_miss_count_ > 5)
            {
                return hardware_interface::return_type::ERROR;
            }
            return hardware_interface::return_type::OK;
        }
        read_miss_count_ = 0;

        auto *trace = single_shot_probe::map_trace();
        if (trace && single_shot_probe::ready(trace) &&
            trace->ec_feedback_change_ns != 0U &&
            trace->ipc_feedback_rx_ns == 0U)
        {
            trace->ipc_feedback_rx_ns = single_shot_probe::now_ns();
        }

        for (std::size_t i = 0; i < info_.joints.size(); ++i)
        {
            hw_pos_[i] = fb_[i].position;
            hw_vel_[i] = fb_[i].velocity;
            hw_eff_[i] = fb_[i].effort;
            // std::cout << "joint[" << i << "]=" << info_.joints[i].name
            //           << ", pos=" << hw_pos_[i]
            //           << std::endl;
        }
        for (std::size_t k = 0; k < imu_iface_names_.size(); ++k)
        {
            imu_states_[k] = imu_value_for(imu_iface_names_[k], imu_sample_);
        }
        return hardware_interface::return_type::OK;
    }

    hardware_interface::return_type ExipcSystemHardware::write(
        const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
    {
        if (!active_)
        {
            return hardware_interface::return_type::OK;
        }

        /***debug log start***/
        // 记录 HAL 写入共享内存的时刻
        auto *trace = single_shot_probe::map_trace();
        if (trace && single_shot_probe::ready(trace) &&
            trace->hal_write_shm_ns == 0U)
        {
            trace->hal_write_shm_ns = single_shot_probe::now_ns();
        }
        /***debug log end***/

        for (std::size_t i = 0; i < info_.joints.size(); ++i)
        {
            const double pos = std::isnan(cmd_pos_[i]) ? hw_pos_[i] : cmd_pos_[i];
            cmd_frame_[i] = hhros2::shm::JointCommand{
                pos, cmd_vel_[i], cmd_eff_[i], cmd_kp_[i], cmd_kd_[i]};
        }
        client_.write_command(cmd_frame_.data(), kJointCount);
        return hardware_interface::return_type::OK;
    }

} // namespace hhros2_hal

PLUGINLIB_EXPORT_CLASS(
    hhros2_hal::ExipcSystemHardware, hardware_interface::SystemInterface)
