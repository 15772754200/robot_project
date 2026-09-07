#include "hhros2_teleop/keyboard_cmd/robotServer.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include "hhros2_teleop/keyboard_cmd/keyboard.h"

namespace hhros2_teleop {


#define REMOTE_PORT 8080

RobotServer::RobotServer(KeyBoard* keyboard) 
    : server_running_(false)
    , should_exit_(false)
    , server_fd_(-1) 
    , keyboard_ptr_(keyboard) {
}

RobotServer::~RobotServer() {
    stopServer();
}

bool RobotServer::startServer() {
    if (server_running_) {
        std::cout << "服务端已在运行中" << std::endl;
        return true;
    }
    
    server_thread_ = std::thread(&RobotServer::serverThread, this);
    std::cout << "正在启动机器人远程控制服务端..." << std::endl;
    return true;
}

bool RobotServer::stopServer() {
    if (!server_running_) {
        std::cout << "服务端未在运行" << std::endl;
        return true;
    }
    
    should_exit_ = true;
    server_running_ = false;
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    std::cout << "正在停止机器人远程控制服务端..." << std::endl;
    return true;
}

bool RobotServer::isRunning() const {
    return server_running_;
}

void RobotServer::serverThread() {
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    if ((server_fd_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket创建失败");
        return;
    }
    
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt失败");
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(REMOTE_PORT);
    
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind失败");
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    
    if (listen(server_fd_, 5) < 0) {
        perror("listen失败");
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    
    server_running_ = true;
    should_exit_ = false;
    
    std::cout << "机器人服务端已启动，监听端口 " << REMOTE_PORT << std::endl;
    std::cout << "等待客户端连接和控制指令..." << std::endl;
    
    std::vector<std::thread> client_threads;
    
    while (server_running_ && !should_exit_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd_, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_fd_ + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity > 0 && FD_ISSET(server_fd_, &readfds)) {
            int new_socket = accept(server_fd_, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            if (new_socket >= 0) {
                client_threads.emplace_back(&RobotServer::handleClient, this, new_socket);
            }
        }
    }
    
    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.detach();
        }
    }
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    server_running_ = false;
    std::cout << "机器人服务端已关闭" << std::endl;
}

void RobotServer::handleClient(int client_socket) {
    char buffer[1024] = {0};
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len);
    std::cout << "客户端连接来自: " << inet_ntoa(client_addr.sin_addr) 
              << ":" << ntohs(client_addr.sin_port) << std::endl;

    while (server_running_) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(client_socket, buffer, sizeof(buffer) - 1);
        
        if (valread <= 0) {
            std::cout << "客户端断开连接" << std::endl;
            break;
        }
        
        std::string command(buffer);
        std::cout << "收到客户端指令: " << command << std::endl;
        
        // 使用智能指针的安全访问
        if (keyboard_ptr_) {
            if (!keyboard_ptr_->ServerCommandHandle(command)) {
                std::cout << "错误: 未知远程命令 '" << command << "'" << std::endl;
                std::string error_response = "错误: 未知命令 '" + command + "'";
                send(client_socket, error_response.c_str(), error_response.length(), 0);
            } else {
                std::string response = "执行: " + command;
                send(client_socket, response.c_str(), response.length(), 0);
            }
        } else {
            std::string error_response = "错误: KeyBoard对象已销毁";
            send(client_socket, error_response.c_str(), error_response.length(), 0);
        }
        
        std::string response = "执行: " + command;
        send(client_socket, response.c_str(), response.length(), 0);
        std::cout << "发送响应: " << response << std::endl;
    }
    
    close(client_socket);
}

} // namespace hhros2_teleop