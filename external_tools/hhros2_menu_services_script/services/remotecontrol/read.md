

在两台机器上都需要安装 Boost（包含 Asio/Beast）：

sudo apt update
sudo apt install -y g++ make libboost-all-dev

编译命令（示例）

下面的编译命令适用于 C++17：

g++ -std=c++17 -O2 websocket_server.cpp -o websocket_server -lboost_system -lpthread
g++ -std=c++17 -O2 websocket_client.cpp -o websocket_client -lboost_system -lpthread


运行与通信流程（步骤）

在服务器（机器 A）上：

编译服务端：

g++ -std=c++17 -O2 websocket_server.cpp -o websocket_server -lboost_system -lpthread


运行服务端（监听 8080，可改端口）：

./websocket_server 8080


服务器会输出类似：

WebSocket server listening on port 8080
等待客户端连接...


在客户端（机器 B）上：

编译客户端：

g++ -std=c++17 -O2 websocket_client.cpp -o websocket_client -lboost_system -lpthread


运行客户端，连接到服务器 IP（假设服务器 IP 为 192.168.1.100）：

./websocket_client 192.168.1.100 8080
