// websocket_server.cpp
// 简单 WebSocket 服务端：接受单个客户端连接（示例可扩展为多客户端广播）
// 编译: g++ -std=c++17 websocket_server.cpp -o websocket_server -lboost_system -lpthread

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
namespace http  = beast::http;
namespace ws    = beast::websocket;
using tcp = asio::ip::tcp;

int main(int argc, char* argv[]) {
    // 默认监听端口
    unsigned short port = 8080;
    if (argc >= 2) {
        port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    // 保证控制台用 UTF-8（终端需设置为 UTF-8）
    std::locale::global(std::locale(""));

    try {
        asio::io_context ioc{1};

        tcp::acceptor acceptor{ioc, {tcp::v4(), port}};
        std::cout << "WebSocket server listening on port " << port << std::endl;
        std::cout << "等待客户端连接..." << std::endl;

        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);

            std::cout << "客户端已连接： " << socket.remote_endpoint() << std::endl;

            // 将 socket 转为 websocket stream
            ws::stream<tcp::socket> ws{std::move(socket)};

            // 接受握手（阻塞）
            ws.accept();

            std::atomic<bool> done{false};

            // 读线程：从客户端读取消息并打印
            std::thread reader([&ws, &done]() mutable {
                try {
                    while (!done) {
                        beast::flat_buffer buffer;
                        ws.read(buffer); // 阻塞直到收到消息或出错
                        std::string msg = beast::buffers_to_string(buffer.data());
                        // 打印接收到的消息（假设 UTF-8）
                        std::cout << "\n<< 客户端: " << msg << std::endl;
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

            // 写线程：读取服务端 stdin，发送给客户端
            std::thread writer([&ws, &done]() mutable {
                try {
                    std::string line;
                    std::cout << "（输入要发送的消息，回车发送）> " << std::flush;
                    while (!done && std::getline(std::cin, line)) {
                        if (line == "/quit" || line == "/exit") {
                            // 关闭连接
                            done = true;
                            ws.close(ws::close_code::normal);
                            break;
                        }
                        // 发送文本（UTF-8）
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

            // 等待线程结束
            writer.join();
            // 如果 writer 主动关闭了连接，reader 会因为 read 失败而结束
            if (reader.joinable()) reader.join();

            std::cout << "客户端已断开，等待下一个连接..." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "致命错误: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
