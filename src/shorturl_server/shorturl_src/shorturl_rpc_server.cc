#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>  // 添加 epoll 头文件
#include <fcntl.h>      // 添加非阻塞设置头文件
#include <thread>
#include "shorturl_rpc.h"
#include "shorturl_rpc.pb.h"


// 启动 RPC 服务
void RunRpcServer() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(50051);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return;
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        return;
    }

     // 创建 epoll 实例（Reactor 核心）
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_fd);
        return;
    }

    // 将监听套接字设置为非阻塞（避免 accept 阻塞事件循环）
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    // 注册监听套接字的可读事件（新连接事件）
    struct epoll_event ev;
    ev.events = EPOLLIN;  // 监听读事件（新连接到来）
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        close(epoll_fd);
        close(server_fd);
        return;
    }


    std::cout << "RPC Server listening on port 50051" << std::endl;

    // Reactor 事件循环
    struct epoll_event events[1024];  // 存储触发的事件
    while (true) {
        // 等待事件触发（超时时间 -1 表示永久等待）
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            // 处理新连接事件（监听套接字可读）
            if (fd == server_fd) {
                while (true) {  // 非阻塞 accept，可能有多个新连接
                    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                    if (new_socket == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 无更多新连接，退出循环
                            break;
                        }
                        perror("accept");
                        continue;
                    }

                    // 将客户端套接字设置为非阻塞
                    flags = fcntl(new_socket, F_GETFL, 0);
                    fcntl(new_socket, F_SETFL, flags | O_NONBLOCK);

                    // 注册客户端套接字的可读事件（等待数据）
                    ev.events = EPOLLIN;  // 监听客户端数据可读
                    ev.data.fd = new_socket;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &ev) == -1) {
                        perror("epoll_ctl: new_socket");
                        close(new_socket);
                    }
                }
            } 
            // 处理客户端数据可读事件
            else {
                char buffer[1024] = {0};
                ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
                if (bytesRead == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 数据未就绪，继续等待
                        continue;
                    }
                    perror("read");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);  // 移除事件监听
                    close(fd);
                    continue;
                } else if (bytesRead == 0) {
                    // 客户端关闭连接
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);  // 移除事件监听
                    close(fd);
                    continue;
                }

                // 读取数据并处理
                std::string requestStr(buffer, bytesRead);
                std::string responseStr = handleRequest(requestStr);

                // 发送响应（可优化为监听 EPOLLOUT 事件确保发送完成）
                send(fd, responseStr.c_str(), responseStr.length(), 0);
                close(fd);  // 关闭连接（根据需求决定是否保持长连接）
            }
        }
    }

    // 清理资源
    close(epoll_fd);
    close(server_fd);
}


void startRpcServer() {
    srand(time(NULL));
    RunRpcServer();
}



