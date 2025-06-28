#include "shorturl_web_service.h"
#include "shorturl_rpc.h" 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <json/json.h>
#include <iostream>
#include "shorturl_rpc.pb.h"
#include <thread>
#include <string>
#include <sstream>
#include <stdexcept>
#include "shorturl_rpc.h"

#define PROTOBUF_ENABLE 0
#define JSON_ENABLE 1

// 处理单个客户端请求的函数
void handleClient(int new_socket) {
    try {
        // 动态缓冲区读取 HTTP 请求
        std::string request;
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(new_socket, buffer, sizeof(buffer))) > 0) {
            request.append(buffer, bytesRead);
        }
        if (bytesRead == -1) {
            throw std::runtime_error("read error");
        }

        // 解析短链标识
        std::string short_key;
        size_t path_start = request.find("GET /p/");
        if (path_start != std::string::npos) {
            path_start += 7;
            size_t path_end = request.find(" ", path_start);
            if (path_end != std::string::npos) {
                short_key = request.substr(path_start, path_end - path_start);
            }
        }

        if (short_key.empty()) {
            std::string http_response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
            if (send(new_socket, http_response.c_str(), http_response.length(), 0) == -1) {
                throw std::runtime_error("send error");
            }
            close(new_socket);
            return;
        }

        // 调用 RPC 解析短链
        std::string rpc_response;
        #if PROTOBUF_ENABLE
        shorturl_rpc::ResolveShortUrlRequest rpc_request;
        rpc_request.set_short_url("http://192.168.31.43/p/" + short_key);
        std::string rpc_request_str;
        rpc_request.SerializeToString(&rpc_request_str);
        rpc_response = handleRequest(rpc_request_str);

        shorturl_rpc::ResolveShortUrlResponse rpc_pb_response;
        if (rpc_pb_response.ParseFromString(rpc_response) && rpc_pb_response.code() == 0) {
            std::string full_url = rpc_pb_response.full_url();
            std::string http_response = "HTTP/1.1 302 Found\r\nLocation: " + full_url + "\r\nContent-Length: 0\r\n\r\n";
            if (send(new_socket, http_response.c_str(), http_response.length(), 0) == -1) {
                throw std::runtime_error("send error");
            }
        } else {
            std::string http_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            if (send(new_socket, http_response.c_str(), http_response.length(), 0) == -1) {
                throw std::runtime_error("send error");
            }
        }
        #elif JSON_ENABLE
        
        std::string shorturl = "http://192.168.31.43/p/" + short_key;
        rpc_response = resolveShortUrl(shorturl);

        Json::Value rpc_json;
        Json::Reader reader;
        if (reader.parse(rpc_response, rpc_json) && rpc_json["code"].asInt() == 0) {
            std::string full_url = rpc_json["full_url"].asString();
            std::string http_response = "HTTP/1.1 302 Found\r\nLocation: " + full_url + "\r\nContent-Length: 0\r\n\r\n";
            if (send(new_socket, http_response.c_str(), http_response.length(), 0) == -1) {
                throw std::runtime_error("send error");
            }
        } else {
            std::string http_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            if (send(new_socket, http_response.c_str(), http_response.length(), 0) == -1) {
                throw std::runtime_error("send error");
            }
        }
        #endif
    } catch (const std::exception& e) {
        std::cerr << "Error handling client: " << e.what() << std::endl;
    }
    close(new_socket);
}

// 短链服务主函数
void RunShortUrlWebService() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 创建 socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        return;
    }

    // 设置 socket 选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        return;
    }

    // 绑定端口（假设短链服务监听 8082 端口）
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8082);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind failed");
        close(server_fd);
        return;
    }

    // 监听连接
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        return;
    }

    std::cout << "Short URL Web Service listening on port 8082" << std::endl;

    // 处理客户端请求
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) == -1) {
            perror("accept");
            continue;
        }

        // 使用线程处理每个客户端连接
        std::thread(handleClient, new_socket).detach();
    }
}
