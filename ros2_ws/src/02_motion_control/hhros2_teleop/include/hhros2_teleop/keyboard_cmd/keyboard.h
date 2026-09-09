#pragma once

#include <stdio.h>
#include <cctype>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include "hhros2_teleop/keyboard_cmd/cmdCall.h"
#include "hhros2_teleop/keyboard_cmd/robotServer.h"

namespace hhros2_teleop {

// 键盘模式
enum class KeyMode {
    KEY_F1,  // 模式F1对应多字符输入
    KEY_F2   // 模式F2对应单字符输入
};


// 机器人姿态切换按键
enum class Key {
    KEY_0 = 0,      // no controller active, joints released
    KEY_9 = 1,      // damping_controller (safe reflex)
    KEY_1 = 2,      // safe standing pose
    KEY_2 = 3,      // RL walk
    KEY_3 = 4,      // RL run
    KEY_4 = 5,      // WBC/QP whole-body task
    KEY_5 = 6,      // scripted / CSV motion playback
    KEY_W = 7,      // x方向正速度
    KEY_S = 8,      // x方向反速度
    KEY_A = 9,      // y方向正速度
    KEY_D = 10,     // y方向反速度
    KEY_Q = 11,     // 偏航角正速度
    KEY_E = 12,     // 偏航角反速度
    KEY_R = 13,     // 速度清0
    KEY_NONE = 100, // 无效输入
};

class KeyBoard {
public:
    KeyBoard();
    ~KeyBoard();
    Key last_key; // 前一次键盘输入
    Key current_key; // 当前键盘输入
    // 供 RobotServer 调用
    bool ServerCommandHandle(const std::string& inputstr);

    // 由 key_publisher 调用
    void setModeCallback(std::function<void(uint8_t)> cb);
    void setVelCallback(std::function<void(double,double,double)> cb);

private:
    void registerCommand(const std::string& cmdName,std::function<void(const std::vector<std::string>&)> handler);//通过哈系表的方式注册指令
    void registerAllCommand();
    void handleEnrcCommand(const std::vector<std::string>& args);

    void handleMultiCharInput(); // 处理多字符输入，并切换模式
    void handleSingleCharInput(); // 处理单字符输入，并切换模式

    void processSingleChar(char c); // 处理单个普通字符
    bool parseAndExecute(const std::string& inputstr);//解析并执行指令，inputstr为用户输入的指令内容
private:
    std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> commandHandlers;//创建一个哈系表，将指令与执行函数关联起来
    static void* runKeyBoard(void *arg);   //静态键盘运行函数（线程入口点）
    static void cleanupTerminal(void* arg);
    void* run(void *arg);  //实际运行函数
    pthread_t _tid;        // 线程ID
    KeyMode keymode_;      // 按键模式
    struct termios _oldSettings, _newSettings;
    unsigned int press_m_count;
    fd_set set;
    char c_; // 单键盘输入
    int input_fd_;
    RobotServer* robot_server;

    std::function<void(uint8_t)> mode_cb_;
    std::function<void(double,double,double)> vel_cb_;

    double axis_linear_x;
    double axis_linear_y;
    double axis_angular_z;
    double max_linear_x;
    double max_linear_y;
    double max_angular_z;
    double min_linear_x;
    double min_linear_y;
    double min_angular_z;
    double linear_increment;
    double angular_increment;    
};


} // namespace hhros2_teleop