#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>

#define PORT 2100
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int server_fd, epfd;
struct epoll_event ev, events[MAX_EVENTS];
std::map<int, int> control_to_data; // 控制连接 -> 数据连接
std::mutex mtx;

void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

void handle_accept_and_epoll() {
    // 初始化 epoll
    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create failed");
        return;
    }

    // 监听 server_fd
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
    ev.data.fd = server_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl (server_fd)");
        close(epfd);
        return;
    }

    while (true) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {
                // 新连接
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int connect_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
                if (connect_fd == -1) {
                    perror("accept");
                    continue;
                }

                // 设置非阻塞模式
                set_nonblocking(connect_fd);

                // 注册到 epoll
                ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
                ev.data.fd = connect_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connect_fd, &ev) == -1) {
                    perror("epoll_ctl (client_fd)");
                    close(connect_fd);
                }
            } else {
                // 处理客户端数据
                int client_fd = events[i].data.fd;
                if (events[i].events & (EPOLLERR | EPOLLRDHUP)) {
                    printf("Client error or disconnected: %d\n", client_fd);
                    close(client_fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    // 读取数据
                    char buffer[BUFFER_SIZE];
                    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
                    if (bytes_read <= 0) {
                        if (bytes_read == 0) {
                            printf("Client disconnected: %d\n", client_fd);
                        } else {
                            perror("read failed");
                        }
                        close(client_fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                    } else {
                        buffer[bytes_read] = '\0';
                        std::cout << "Received: " << buffer << std::endl;

                        // 解析 FTP 命令（简化版）
                        if (strncmp(buffer, "PASV", 4) == 0) {
                            // 创建数据连接（简化版）
                            int data_fd = socket(AF_INET, SOCK_STREAM, 0);
                            if (data_fd == -1) {
                                perror("socket");
                                continue;
                            }

                            set_nonblocking(data_fd);

                            struct sockaddr_in data_addr{};
                            data_addr.sin_family = AF_INET;
                            data_addr.sin_port = htons(50000 + rand() % 10000); // 随机端口
                            data_addr.sin_addr.s_addr = INADDR_ANY;

                            if (bind(data_fd, (struct sockaddr*)&data_addr, sizeof(data_addr)) == -1) {
                                perror("bind");
                                close(data_fd);
                                continue;
                            }

                            if (listen(data_fd, 5) == -1) {
                                perror("listen");
                                close(data_fd);
                                continue;
                            }

                            // 发送 PASV 响应
                            char response[100];
                            snprintf(response, sizeof(response), "227 entering passive mode (%d,%d,%d,%d,%d,%d)\r\n",
                                     127, 0, 0, 1, (data_addr.sin_port >> 8) & 0xFF, data_addr.sin_port & 0xFF);
                            write(client_fd, response, strlen(response));

                            // 记录控制连接 -> 数据连接
                            {
                                std::lock_guard<std::mutex> lock(mtx);
                                control_to_data[client_fd] = data_fd;
                            }

                            // 等待客户端连接数据端口（简化版）
                            struct sockaddr_in client_data_addr{};
                            socklen_t client_data_len = sizeof(client_data_addr);
                            int client_data_fd = accept(data_fd, (struct sockaddr*)&client_data_addr, &client_data_len);
                            if (client_data_fd == -1) {
                                perror("accept (data)");
                                close(data_fd);
                                {
                                    std::lock_guard<std::mutex> lock(mtx);
                                    control_to_data.erase(client_fd);
                                }
                                continue;
                            }

                            // 处理数据传输（简化版）
                            char data_buffer[BUFFER_SIZE];
                            ssize_t data_bytes_read;
                            while ((data_bytes_read = read(client_data_fd, data_buffer, BUFFER_SIZE)) > 0) {
                                write(client_data_fd, data_buffer, data_bytes_read); // 回显
                            }

                            close(client_data_fd);
                            close(data_fd);
                            {
                                std::lock_guard<std::mutex> lock(mtx);
                                control_to_data.erase(client_fd);
                            }
                        }
                    }
                }
            }
        }
    }

    close(epfd);
}

void FTP_start() {
    // 初始化服务器
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return;
    }

    set_nonblocking(server_fd);

    struct sockaddr_in ser_addr{};
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) == -1) {
        perror("Listen failed");
        close(server_fd);
        return;
    }

    // 启动 accept + epoll 线程
    std::thread accept_epoll_thread(handle_accept_and_epoll);
    accept_epoll_thread.detach(); // 分离线程

    // 主线程可以继续做其他事情（如监控）
    while (true) {
        sleep(1);
    }
}

int main() {
    FTP_start();
    return 0;
}