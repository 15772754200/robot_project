// websocket_client.cpp
// 简单 WebSocket 客户端：连接到服务器并进行双向通信
// 编译: g++ -std=c++17 websocket_client.cpp -o websocket_client -lboost_system -lpthread

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <locale>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using tcp = asio::ip::tcp;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <server_ip> <port>\n";
        std::cerr << "例如: " << argv[0] << " 192.168.1.100 8080\n";
        return EXIT_FAILURE;
    }

    std::string server = argv[1];
    unsigned short port = static_cast<unsigned short>(std::stoi(argv[2]));

    // 保证控制台用 UTF-8（终端需设置为 UTF-8）
    std::locale::global(std::locale(""));

    try {
        asio::io_context ioc;
        tcp::resolver resolver{ioc};
        auto endpoints = resolver.resolve(server, std::to_string(port)); //resolver.resolve 是一个非常重要的函数，用于将主机名和端口号解析为具体的 TCP 端点

        tcp::socket socket{ioc};//套接字可以被看作是一个通信端点
        //它允许一个进程（如客户端或服务器）通过网络与其他进程进行通信。
        //套接字封装了网络通信的复杂性，使得开发者可以使用简单的API来发送和接收数据
        asio::connect(socket, endpoints);

        ws::stream<tcp::socket> ws{std::move(socket)};

        // 握手（指定目标路径 "/"）
        ws.handshake(server + ":" + std::to_string(port), "/");

        std::cout << "已连接到服务器 " << server << ":" << port << std::endl;

        std::atomic<bool> done{false};

        // 读线程：从服务器接收并打印
        std::thread reader([&ws, &done]() mutable {
            try {
                while (!done) {
                    beast::flat_buffer buffer;
                    ws.read(buffer);//ws.read(buffer)：从 WebSocket 连接中读取数据，
                    //并将数据存储到 buffer 中。
                    std::string msg = beast::buffers_to_string(buffer.data());
                    std::cout << "\n<< 服务器: " << msg << std::endl;
                    std::cout << "（输入要发送的消息，回车发送）> " << std::flush;
                }
            } catch (const beast::system_error& se) {
                if (se.code() != asio::error::operation_aborted)
                    std::cerr << "\n读取错误: " << se.code().message() << std::endl;
                done = true;
            } catch (const std::exception& e) {
                std::cerr << "\n读取异常: " << e.what() << std::endl;
                done = true;
            }
        });

        // 写线程：从 stdin 读取并发送
        std::thread writer([&ws, &done]() mutable {
            try {
                std::string line;
                std::cout << "（输入要发送的消息，回车发送）> " << std::flush;
                while (!done && std::getline(std::cin, line)) {
                    if (line == "/quit" || line == "/exit") {
                        done = true;
                        ws.close(ws::close_code::normal);
                        break;
                    }
                    ws.write(asio::buffer(line));
                    std::cout << "已发送: " << line << std::endl;
                    if (!done) std::cout << "（输入要发送的消息，回车发送）> " << std::flush;
                }
            } catch (const beast::system_error& se) {
                if (se.code() != asio::error::operation_aborted)
                    std::cerr << "\n发送错误: " << se.code().message() << std::endl;
                done = true;
            } catch (const std::exception& e) {
                std::cerr << "\n发送异常: " << e.what() << std::endl;
                done = true;
            }
        });

        writer.join();
        if (reader.joinable()) reader.join();

        std::cout << "客户端退出" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
