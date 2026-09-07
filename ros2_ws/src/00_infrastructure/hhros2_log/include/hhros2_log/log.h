#ifndef LOG_H_
#define LOG_H_

#include <string>
#include <queue>
#include <pthread.h>
#include <fstream>
#include <map>
#include <memory>
#include <chrono>      // for std::chrono::system_clock
#include <sstream>     // for std::stringstream
#include <iomanip>     // for std::put_time, std::setfill, std::setw
#include <ctime>       // for std::localtime, std::time_t
#include <iostream>

// 日志级别枚举
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

// 日志类型
enum class LogType {
    IMULOG = 0,
    LIDARLOG = 1,
    TOUCHSENSORLOG = 2,
    GPSLOG = 3,
    TOFLOG = 4,
    RTKLOG = 5,
    SPEAKERLOG = 6,
    MICROPHONELOG = 7,
    DEPTHCAMERALOG = 8,
    RGBCAMERALOG = 9,
    MOTORLOG = 10,
    ULTRASONICLOG = 11,
    HALLOG = 12,
    CONTROLLERLOG = 13,
    OTHER
};

// 日志消息结构
struct LogMessage {
    LogLevel level;
    LogType type;
    std::string message;
    std::string timestamp;
    pid_t thread_id;
    
    LogMessage(LogLevel lvl, LogType logType, const std::string& msg, const std::string& time, pid_t tid);
};

// 日志文件句柄
struct LogFileHandle {
    std::ofstream file_stream;
    std::string file_path;
    std::string current_date; // 用于日期变更检测
};

class Logger {
private:
    std::queue<LogMessage> log_queue_;
    pthread_mutex_t queue_mutex_;
    pthread_cond_t queue_cond_;
    pthread_t log_thread_;
    bool running_;
    
    // 按日志类型存储的文件句柄映射表 [2,3](@ref)
    std::map<LogType, std::unique_ptr<LogFileHandle>> log_files_;
    pthread_mutex_t file_map_mutex_;
    
    LogLevel min_log_level_;
    std::string log_base_dir_;
    
    // 单例模式
    static Logger* instance_;
    static pthread_mutex_t instance_mutex_;
    
    Logger();
    ~Logger();
    
    // 删除拷贝构造函数和赋值运算符
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 内部日志方法
    void logInternal(LogLevel level, LogType logType, const char* format, va_list args);
    
    // 日志线程函数
    static void* logThreadFunc(void* arg);
    
    // 处理日志消息
    void processLogs();
    
    // 将日志写入文件（修改为按类型写入）
    void writeLogToFile(const LogMessage& msg);
    
    // 递归创建文件夹
    bool createDirectoryRecursive(const std::string& dirPath);
    
    // 将LogType转换为目录名
    std::string logTypeToDirName(LogType type);
    
    // 获取或创建日志文件句柄
    LogFileHandle* getLogFileHandle(LogType type);
    
    // 生成按类型分目录的日志文件路径
    std::string generateLogFilePath(LogType type);
    
    // 检查并切换日志文件（按日期）
    bool checkAndRotateLogFile(LogType type);

public:
    // 获取单例实例
    static Logger* getInstance();
    
    // 初始化日志系统
    bool initialize(const std::string& base_dir = "run_logs", LogLevel min_level = LogLevel::DEBUG);
    
    // 停止日志系统
    void exit();
    
    // 记录日志（格式化字符串）
    void log(LogLevel level, LogType logType, const char* format, ...);
    
    // 便捷日志方法
    void debug(LogType logType, const char* format, ...);
    void info(LogType logType, const char* format, ...);
    void warning(LogType logType, const char* format, ...);
    void error(LogType logType, const char* format, ...);
    void critical(LogType logType, const char* format, ...);
    
    // 手动刷新缓冲区
    void flush();

    // 获取当前时间
    static std::string getCurrentTimestamp(){
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        // 格式化时间戳
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

};


// 便捷宏定义
#define LOG_DEBUG(log_type, ...) Logger::getInstance()->debug(log_type, __VA_ARGS__)
#define LOG_INFO(log_type, ...) Logger::getInstance()->info(log_type, __VA_ARGS__)
#define LOG_WARNING(log_type, ...) Logger::getInstance()->warning(log_type, __VA_ARGS__)
#define LOG_ERROR(log_type, ...) Logger::getInstance()->error(log_type, __VA_ARGS__)
#define LOG_CRITICAL(log_type, ...) Logger::getInstance()->critical(log_type, __VA_ARGS__)

#endif // LOG_H_