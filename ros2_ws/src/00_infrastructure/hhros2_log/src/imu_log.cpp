#include "hhros2_log/imu_log.h"

IMUSelfCheckLogger* IMUSelfCheckLogger::instance_ = nullptr;
pthread_mutex_t IMUSelfCheckLogger::instance_mutex_ = PTHREAD_MUTEX_INITIALIZER;

// 单例构造函数实现
IMUSelfCheckLogger::IMUSelfCheckLogger(const std::string& robot_id, const std::string& unit_id) :
    robot_id_(robot_id), unit_id_(unit_id) { }

// 析构函数
IMUSelfCheckLogger::~IMUSelfCheckLogger(){ }

/**
    *@brief 获取单例实例
    *@param robot_id 机器人标识（仅首次调用有效）
    *@param unit_id 检测单元ID（仅首次调用有效）
    *@return 单例实例引用
*/
IMUSelfCheckLogger* IMUSelfCheckLogger::getInstance(const std::string& robot_id, const std::string& unit_id) {
    pthread_mutex_lock(&instance_mutex_);
    if (instance_ == nullptr) {
        std::cout << "instance_" << std::endl;
        instance_ = new IMUSelfCheckLogger(robot_id, unit_id);
    }
    pthread_mutex_unlock(&instance_mutex_);
    return instance_;
}

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
void IMUSelfCheckLogger::logIMUCheckResult(const std::string& log_path, bool result,
    const std::array<double, 3>& accel_biases,
    const std::array<double, 3>& gyro_biases,
    const std::array<double, 3>& accel_mean,
    const std::array<double, 3>& gyro_mean,
    size_t sample_count,
    double sample_duration) {
    // 创建JSON对象
    nlohmann::json log_data;
    log_data["timestamp"] = Logger::getCurrentTimestamp();
    log_data["robot_id"] = robot_id_;
    log_data["unit_id"] = unit_id_;
    log_data["check_type"] = "imu_self_check";
    log_data["result"] = result ? "PASS" : "FAILED";

    // 添加详细数据
    nlohmann::json details;
    details["accel_biases"] = {
        {"x", accel_biases[0]},
        {"y", accel_biases[1]},
        {"z", accel_biases[2]}
    };
    details["gyro_biases"] = {
        {"x", gyro_biases[0]},
        {"y", gyro_biases[1]},
        {"z", gyro_biases[2]}
    };
    details["accel_mean"] = {
        {"x", accel_mean[0]},
        {"y", accel_mean[1]},
        {"z", accel_mean[2]}
    };
    details["gyro_mean"] = {
        {"x", gyro_mean[0]},
        {"y", gyro_mean[1]},
        {"z", gyro_mean[2]}
    };
    details["sample_count"] = sample_count;
    details["sample_duration"] = sample_duration;
    log_data["details"] = details;

    // 写入日志文件
    writeJsonLog(log_path, log_data);
}
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
void IMUSelfCheckLogger::logAttitudeCheckResult(const std::string& log_path, bool result, double quaternion_norm_mean, double quaternion_norm_deviation, double roll_mean,
    double pitch_mean, double yaw_mean, double roll_rms, double pitch_rms, double yaw_rms, size_t sample_count, double sample_duration)
{
    // 创建JSON对象
    nlohmann::json log_data;
    log_data["timestamp"] = Logger::getCurrentTimestamp(); 
    log_data["robot_id"] = robot_id_;
    log_data["unit_id"] = unit_id_;
    log_data["check_type"] = "attitude_self_check";
    log_data["result"] = result ? "PASS" : "FAILED";

    // 添加详细数据
    nlohmann::json details;
    details["quaternion_norm"] = {
        {"mean", quaternion_norm_mean},
        {"deviation", quaternion_norm_deviation}
    };
    details["attitude_angles_mean"] = {
        {"roll", roll_mean},
        {"pitch", pitch_mean},
        {"yaw", yaw_mean}
    };
    details["attitude_angles_rms"] = {
        {"roll", roll_rms},
        {"pitch", pitch_rms},
        {"yaw", yaw_rms}
    };
    details["sample_count"] = sample_count;
    details["sample_duration"] = sample_duration;

    log_data["details"] = details;

    // 写入日志文件
    writeJsonLog(log_path, log_data);
}
/**
    *@brief 写入JSON日志到文件
    *@param filepath 文件路径名
    *@param json_data JSON数据
*/
void IMUSelfCheckLogger::writeJsonLog(const std::string& filepath, const nlohmann::json& json_data) {
    // 写入JSON数据到文件（覆盖模式）
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << json_data.dump(4) << std::endl;  // 缩进4个空格，美化输出
        file.close();
        LOG_WARNING(LogType::IMULOG, "日志已写入: %s" , filepath.c_str());
    } else {
        LOG_WARNING(LogType::IMULOG, "无法打开日志文件: %s" , filepath.c_str());
    }
}