



#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <locale>


namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ws   = beast::websocket;
using tcp = asio::ip::tcp;


int main (int argc,char* argv[]){
    if(argc<3){
        std::cerr <<"用法："<< argc[0] <<"<server_ip><port>\n";
        std::cerr <<"例如："<< argc[0] <<"192.168.1.100 8080\n";
        return EXIT_FAILURE;
    }

    std::string server = argc[1];
    unsigned short port = static_cast<unsigned short>(std::stoi(argc[2]));

    std::locale::global (std::locale(""));
    
    try{
        asio::io_context ioc;
        tcp::resolver resolver{ioc};
        auto endpoints = resolver.resolve(server, std::to_string(port));

        tcp::socket socket{ioc};
        asio::connect(socket,endpoints);
        ws::stream<tcp::socket> ws{std::move(socket)};
        ws.handshake(sever + ":"+std::to_string(port),"/");

        std::cout <<"已连接到服务器" <<server <<":"<< port <<std::endl;
        std::stomic<bool>done {false};
        std::thread reader([&ws,&done]() mutable{
            try {
                while(!done){
                    beast::flat_buffer buffer;
                    ws.read(buffer);
                    std::string mag =beast::buffers_to_string(buffer.data());
                    std::cout <<"/n<<服务器："<<mag<<std:endl;
                    std::cout<<"(输入要发送的消息，回车发送)"><<std::flush;
                }
            } catch(const beast::system_error& se) {
                if (se.code()!=asio::error::operation_aborted)
                 std::cerr<<"/n读取错误:"<<se.code().message()<<std::endl;
                 done = true;        
            
            }
        });
        std::thread writer([&ws,&done]() mutable{
              try{
                 std::string line;
                 std::cout << "(输入要发送的消息，回车发送）>"<<std::flush;
                 while(!done &&std::getline(std::cin,line)){
                    if(line == "/quit"|| line =="/exit"){
                        done =true;
                        ws.close(ws::close_code::normal)：
                        break;

                
                    }
                    ws.write(asio::buffer(line));
                    std::cout<<"(输入要发送的消息，回车发送)>"<<std::flush;

                 }
              }  catch(const beast::system_error& se){
                if (se.code()!=asio::error::operation_aborted)
                    std::cerr<<"/n写入错误:"<<se.code().message()<<std::endl;
                    done =true;
              }catch(const std::exception& e){
                std::cerr<<"/n发送错误:"<<e.what()<<std::endl;
                done=true;
              }

        });
        writer.join();
        if (reader.joinable() reader.join());


        std::cerr<<"客户端退出" <<std::endl;
    }catch(const std::exception& e){
        std::cerr<<"异常："<< e.what()<<std::endl;
        return EXIT_FAILURE;
    }


    return 0;




    }





}