#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace hhros2_teleop {

class KeyBoard;  //前向声明

class RobotServer {
public:
    RobotServer(KeyBoard* keyboard);
    ~RobotServer();
    
    bool startServer();
    bool stopServer();
    bool isRunning() const;
private:
    void serverThread();
    void handleClient(int client_socket);
    
    std::atomic<bool> server_running_;
    std::atomic<bool> should_exit_;
    std::thread server_thread_;
    int server_fd_;
    KeyBoard* keyboard_ptr_; 
};

} // namespace hhros2_teleop