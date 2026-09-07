#include "hhros2_log/log.h"
#include <iostream>
#include <memory>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>

// LogMessage 结构体实现
LogMessage::LogMessage(LogLevel lvl, LogType logType, const std::string& msg, 
                      const std::string& time, pid_t tid)
    : level(lvl), type(logType), message(msg), timestamp(time), thread_id(tid) {}

// 静态成员初始化
Logger* Logger::instance_ = nullptr;
pthread_mutex_t Logger::instance_mutex_ = PTHREAD_MUTEX_INITIALIZER;

// Logger 构造函数
Logger::Logger() : running_(false), min_log_level_(LogLevel::DEBUG), log_base_dir_("run_logs") {
    pthread_mutex_init(&queue_mutex_, nullptr);
    pthread_cond_init(&queue_cond_, nullptr);
    pthread_mutex_init(&file_map_mutex_, nullptr);
}

// Logger 析构函数
Logger::~Logger() {
    exit();
    pthread_mutex_destroy(&queue_mutex_);
    pthread_cond_destroy(&queue_cond_);
    pthread_mutex_destroy(&file_map_mutex_);
}

// 将LogType转换为目录名（用于文件路径）
std::string Logger::logTypeToDirName(LogType type) {
    switch (type) {
        case LogType::IMULOG: return "imu";
        case LogType::LIDARLOG: return "lidar";
        case LogType::TOUCHSENSORLOG: return "touchsensor";
        case LogType::GPSLOG: return "gps";
        case LogType::TOFLOG: return "tof";
        case LogType::RTKLOG: return "rtk";
        case LogType::SPEAKERLOG: return "speaker";
        case LogType::MICROPHONELOG: return "microphone";
        case LogType::DEPTHCAMERALOG: return "depthcamera";
        case LogType::RGBCAMERALOG: return "rgbcamera";
        case LogType::MOTORLOG: return "motor";
        case LogType::ULTRASONICLOG: return "ultrasonic";
        case LogType::HALLOG: return "hal";
        case LogType::CONTROLLERLOG: return "motion_control";
        case LogType::OTHER: return "other";
        default: return "other";
    }
}

// 生成按类型分目录的日志文件路径
std::string Logger::generateLogFilePath(LogType type) {
    time_t now = time(nullptr);
    char timeBuffer[40];
    struct tm* tm_info = localtime(&now);
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d", tm_info);
    
    std::string typeDir = logTypeToDirName(type);
    std::string fileName = std::string(timeBuffer) + ".log";
    
    return log_base_dir_ + "/" + typeDir + "/" + fileName;
}

// 递归创建文件夹
bool Logger::createDirectoryRecursive(const std::string& dirPath) {
    struct stat info;
    if (stat(dirPath.c_str(), &info) == 0) {
        return S_ISDIR(info.st_mode);
    }

    size_t lastSlash = dirPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string parentDir = dirPath.substr(0, lastSlash);
        if (!createDirectoryRecursive(parentDir)) {
            return false;
        }
    }

    if (mkdir(dirPath.c_str(), 0755) != 0) {
        std::cerr << "Failed to create directory: " << dirPath << std::endl;
        return false;
    }
    return true;
}

// 获取或创建日志文件句柄
LogFileHandle* Logger::getLogFileHandle(LogType type) {
    pthread_mutex_lock(&file_map_mutex_);
    
    auto it = log_files_.find(type);
    if (it != log_files_.end()) {
        pthread_mutex_unlock(&file_map_mutex_);
        return it->second.get();
    }
    
    // 创建新的文件句柄
    std::string filePath = generateLogFilePath(type);
    
    // 创建目录
    size_t last_slash = filePath.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string dir = filePath.substr(0, last_slash);
        if (!createDirectoryRecursive(dir)) {
            std::cerr << "Failed to create log directory: " << dir << std::endl;
            pthread_mutex_unlock(&file_map_mutex_);
            return nullptr;
        }
    }
    
    auto handle = std::make_unique<LogFileHandle>();
    handle->file_path = filePath;
    handle->file_stream.open(filePath, std::ios::app);
    
    if (!handle->file_stream.is_open()) {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
        pthread_mutex_unlock(&file_map_mutex_);
        return nullptr;
    }
    
    // 记录当前日期
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char dateBuffer[20];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", tm_info);
    handle->current_date = dateBuffer;
    
    LogFileHandle* result = handle.get();
    log_files_[type] = std::move(handle);
    
    pthread_mutex_unlock(&file_map_mutex_);
    return result;
}

// 检查并切换日志文件（按日期
bool Logger::checkAndRotateLogFile(LogType type) {
    pthread_mutex_lock(&file_map_mutex_);
    
    auto it = log_files_.find(type);
    if (it == log_files_.end()) {
        pthread_mutex_unlock(&file_map_mutex_);
        return true; // 文件未打开，需要创建
    }
    
    LogFileHandle* handle = it->second.get();
    
    // 检查日期是否变更
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char currentDate[20];
    strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", tm_info);
    
    if (handle->current_date != currentDate) {
        // 日期变更，需要重新打开文件
        handle->file_stream.close();
        
        std::string newFilePath = generateLogFilePath(type);
        handle->file_stream.open(newFilePath, std::ios::app);
        
        if (!handle->file_stream.is_open()) {
            std::cerr << "Failed to reopen log file: " << newFilePath << std::endl;
            pthread_mutex_unlock(&file_map_mutex_);
            return false;
        }
        
        handle->file_path = newFilePath;
        handle->current_date = currentDate;
    }
    
    pthread_mutex_unlock(&file_map_mutex_);
    return true;
}

// 获取单例实例
Logger* Logger::getInstance() {
    pthread_mutex_lock(&instance_mutex_);
    if (instance_ == nullptr) {
        instance_ = new Logger();
    }
    pthread_mutex_unlock(&instance_mutex_);
    return instance_;
}

// 初始化日志系统
bool Logger::initialize(const std::string& base_dir, LogLevel min_level) {
    min_log_level_ = min_level;
    log_base_dir_ = base_dir.empty() ? "run_logs" : base_dir;
    
    // 创建基础日志目录
    if (!createDirectoryRecursive(log_base_dir_)) {
        std::cerr << "Failed to create base log directory: " << log_base_dir_ << std::endl;
        return false;
    }
    
    running_ = true;
    if (pthread_create(&log_thread_, nullptr, &Logger::logThreadFunc, this) != 0) {
        std::cerr << "Failed to create log thread" << std::endl;
        running_ = false;
        return false;
    }
    
    pthread_setname_np(log_thread_, "LoggerThread");
    return true;
}

// 停止日志系统
void Logger::exit() {
    if (running_) {
        running_ = false;
        pthread_cond_broadcast(&queue_cond_);
        pthread_join(log_thread_, nullptr);
        
        // 关闭所有文件流
        pthread_mutex_lock(&file_map_mutex_);
        for (auto& pair : log_files_) {
            if (pair.second->file_stream.is_open()) {
                pair.second->file_stream.close();
            }
        }
        log_files_.clear();
        pthread_mutex_unlock(&file_map_mutex_);
    }
}

// 内部日志方法
void Logger::logInternal(LogLevel level, LogType logType, const char* format, va_list args) {
    if (level < min_log_level_) return;
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm);
    
    pid_t thread_id = gettid();
    
    pthread_mutex_lock(&queue_mutex_);
    log_queue_.emplace(level, logType, buffer, time_buffer, thread_id);
    pthread_cond_signal(&queue_cond_);
    pthread_mutex_unlock(&queue_mutex_);
}

// 调试日志
void Logger::debug(LogType logType, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::DEBUG, logType, format, args);
    va_end(args);
}

// 普通消息日志
void Logger::info(LogType logType, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::INFO, logType, format, args);
    va_end(args);
}

// 警告日志
void Logger::warning(LogType logType, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::WARNING, logType, format, args);
    va_end(args);
}

// 错误日志
void Logger::error(LogType logType, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::ERROR, logType, format, args);
    va_end(args);
}

// 严重错误日志
void Logger::critical(LogType logType, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::CRITICAL, logType, format, args);
    va_end(args);
}

// 日志线程函数
void* Logger::logThreadFunc(void* arg) {
    Logger* logger = static_cast<Logger*>(arg);
    logger->processLogs();
    return nullptr;
}

// 处理日志消息
void Logger::processLogs() {
    while (running_ || !log_queue_.empty()) {
        pthread_mutex_lock(&queue_mutex_);
        
        while (log_queue_.empty() && running_) {
            pthread_cond_wait(&queue_cond_, &queue_mutex_);
        }
        
        if (log_queue_.empty()) {
            pthread_mutex_unlock(&queue_mutex_);
            continue;
        }
        
        LogMessage msg = log_queue_.front();
        log_queue_.pop();
        pthread_mutex_unlock(&queue_mutex_);
        
        writeLogToFile(msg);
    }
}

// 将日志写入对应的类型文件
void Logger::writeLogToFile(const LogMessage& msg) {
    // 检查并确保日志文件就绪
    if (!checkAndRotateLogFile(msg.type)) {
        std::cerr << "Failed to rotate log file for type: " << static_cast<int>(msg.type) << std::endl;
        return;
    }
    
    LogFileHandle* handle = getLogFileHandle(msg.type);
    if (!handle || !handle->file_stream.is_open()) {
        std::cerr << "Failed to get log file handle for type: " << static_cast<int>(msg.type) << std::endl;
        return;
    }
    
    const char* level_str = "";
    switch (msg.level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARNING: level_str = "WARNING"; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
        case LogLevel::CRITICAL: level_str = "CRITICAL"; break;
    }
    
    handle->file_stream << "[" << msg.timestamp << "] "
                       << "[" << level_str << "] "
                       << "[Thread:" << msg.thread_id << "] "
                       << msg.message << std::endl;
    
    handle->file_stream.flush();
}

// 手动刷新缓冲区
void Logger::flush() {
    pthread_mutex_lock(&file_map_mutex_);
    for (auto& pair : log_files_) {
        if (pair.second->file_stream.is_open()) {
            pair.second->file_stream.flush();
        }
    }
    pthread_mutex_unlock(&file_map_mutex_);
}