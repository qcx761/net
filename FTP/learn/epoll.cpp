#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

#define PORT 8080
#define MAX_EVENTS 10

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return 1;
    }

    // 设置非阻塞模式（可选）
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    // 绑定并监听
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen");
        return 1;
    }

    // 创建 epoll 实例
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        return 1;
    }

    // 注册监听套接字到 epoll
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN | EPOLLET;  // 边缘触发模式
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl");
        return 1;
    }

    while (true) {
        // 等待事件
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            break;
        }

        // 直接处理事件（不封装成 handle 函数）
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_fd) {
                // 新连接到来
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
                if (conn_fd == -1) {
                    perror("accept");
                    continue;
                }

                // 设置非阻塞模式
                fcntl(conn_fd, F_SETFL, fcntl(conn_fd, F_GETFL, 0) | O_NONBLOCK);

                // 注册新连接到 epoll
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
                    perror("epoll_ctl");
                    close(conn_fd);
                }
            } else if (events[i].events & EPOLLIN) {
                // 可读事件（客户端发送数据）
                char buf[1024];
                int conn_fd = events[i].data.fd;
                ssize_t len = read(conn_fd, buf, sizeof(buf));
                if (len <= 0) {
                    // 客户端关闭连接或出错
                    if (len == 0) {
                        std::cout << "Client disconnected: " << conn_fd << std::endl;
                    } else {
                        perror("read");
                    }
                    close(conn_fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, conn_fd, nullptr);
                } else {
                    // 处理数据（这里简单回显）
                    write(conn_fd, buf, len);
                }
            } else if (events[i].events & EPOLLOUT) {
                // 可写事件（通常用于非阻塞写）
                // 这里可以处理需要写入数据的场景
                std::cout << "EPOLLOUT event on fd: " << events[i].data.fd << std::endl;
            }
        }
    }

    close(epfd);
    return 0;
}