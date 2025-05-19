#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        return 1;
    }

    int listen_fd = /* 创建并绑定监听套接字 */;
    set_nonblocking(listen_fd);
    listen(listen_fd, SOMAXCONN);

    // 水平触发
    struct epoll_event event;
    event.events = EPOLLIN; // 水平触发
    event.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &event);

    // 边缘触发示例
    // event.events = EPOLLIN | EPOLLET; // 边缘触发
    // epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &event);

    while (true) {
        struct epoll_event events[10];
        int n = epoll_wait(epfd, events, 10, -1);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == listen_fd) {
                // 接受连接
                int conn_fd = accept(listen_fd, nullptr, nullptr);
                set_nonblocking(conn_fd);
                
                // 添加连接到 epoll
                struct epoll_event conn_event;
                conn_event.events = EPOLLIN; // 或 EPOLLIN | EPOLLET; 选择模式
                conn_event.data.fd = conn_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &conn_event);
            } else {
                // 读取数据
                char buf[1024];
                ssize_t len = read(events[i].data.fd, buf, sizeof(buf));
                if (len == 0) {
                    // 处理关闭连接
                    close(events[i].data.fd);
                } else if (len > 0) {
                    // 处理接收到的数据
                    std::cout << "Received data" << std::endl;
                }
            }
        }
    }

    close(epfd);
    return 0;
}