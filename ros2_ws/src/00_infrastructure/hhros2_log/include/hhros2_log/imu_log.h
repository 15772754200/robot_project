#ifndef IMU_LOG_H
#define IMU_LOG_H

#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <pthread.h>
#include <nlohmann/json.hpp>
#include "hhros2_log/log.h"

/**
 *@brief 自检日志记录器类（单例模式）
 *负责将IMU自检和姿态自检结果记录到日志文件
 */
class IMUSelfCheckLogger
{
public:
    // 禁止拷贝构造和赋值运算
    IMUSelfCheckLogger(const IMUSelfCheckLogger&) = delete;
    IMUSelfCheckLogger& operator=(const IMUSelfCheckLogger&) = delete;

    /**
     *@brief 获取单例实例
     *@param robot_id 机器人标识（仅首次调用有效）
     *@param unit_id 检测单元ID（仅首次调用有效）
     *@return 单例实例引用
     */
    static IMUSelfCheckLogger* getInstance(
        const std::string& robot_id = "Robot_01", 
        const std::string& unit_id = "IMU_01");

    /**
     *@brief 记录IMU自检结果
     *@param log_path 日志路径
     *@param result 自检是否通过
     *@param accel_biases 加速度计零偏 [x, y, z]
     *@param gyro_biases 陀螺仪零偏 [x, y, z]
     *@param accel_mean 加速度计平均值 [x, y, z]
     *@param gyro_mean 陀螺仪平均值 [x, y, z]
     *@param sample_count 采样数量
     *@param sample_duration 采样时间
     */
    void logIMUCheckResult(
        const std::string& log_path,
        bool result,
        const std::array<double, 3>& accel_biases,
        const std::array<double, 3>& gyro_biases,
        const std::array<double, 3>& accel_mean,
        const std::array<double, 3>& gyro_mean,
        size_t sample_count,
        double sample_duration);

    /**
     *@brief 记录姿态自检结果
     *@param log_path 日志路径
     *@param result 自检是否通过
     *@param quaternion_norm_mean 四元数模长平均值
     *@param quaternion_norm_deviation 四元数模长偏差
     *@param roll_mean 横滚角平均值
     *@param pitch_mean 俯仰角平均值
     *@param yaw_mean 航向角平均值
     *@param roll_rms 横滚角RMS
     *@param pitch_rms 俯仰角RMS
     *@param yaw_rms 航向角RMS
     *@param sample_count 采样数量
     *@param sample_duration 采样时间
     */
    void logAttitudeCheckResult(
        const std::string& log_path,
        bool result,
        double quaternion_norm_mean,
        double quaternion_norm_deviation,
        double roll_mean,
        double pitch_mean,
        double yaw_mean,
        double roll_rms,
        double pitch_rms,
        double yaw_rms,
        size_t sample_count,
        double sample_duration);

private:
    /**
     *@brief 构造函数（私有，仅内部调用）
     *@param robot_id 机器人标识
     *@param unit_id 检测单元ID
     */
    explicit IMUSelfCheckLogger(const std::string& robot_id = "Robot_01", const std::string& unit_id = "IMU_01");

    /**
     *@brief 析构函数（私有）
     */
    ~IMUSelfCheckLogger();

    /**
     *@brief 写入JSON日志到文件
     *@param filepath 文件路径名
     *@param json_data JSON数据
     */
    void writeJsonLog(const std::string& filepath, const nlohmann::json& json_data);

    std::string robot_id_;   // 机器人标识
    std::string unit_id_;    // 检测单元ID

    static IMUSelfCheckLogger* instance_;
    static pthread_mutex_t instance_mutex_;
};

#endif // !IMU_LOG_H