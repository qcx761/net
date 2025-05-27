void handle_pasv(int client_fd) {
    // 生成随机端口
    int port;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        port = 1024 + rand() % (65535 - 1024); // 1024-65535
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        // 尝试绑定端口
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == -1) {
            perror("socket failed");
            continue;
        }

        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(listen_fd);
            continue; // 端口被占用，重试
        }

        // 成功绑定，退出循环
        break;
    } while (1);

    // 监听数据端口
    if (listen(listen_fd, 5) == -1) {
        perror("listen failed");
        close(listen_fd);
        return;
    }

    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    // 注册到 epoll
    struct epoll_event ev;
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        close(listen_fd);
        return;
    }

    // 获取实际绑定的 IP 和端口
    if (getsockname(listen_fd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getsockname failed");
        close(listen_fd);
        return;
    }

    // 转换 IP 地址为字符串
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));

    // 解析 IP 地址为 h1.h2.h3.h4 格式
    char str[4][4];
    sscanf(ip_str, "%3[^.].%3[^.].%3[^.].%3[^.]", str[0], str[1], str[2], str[3]);

    // 计算 p1 和 p2
    int p1 = port / 256;
    int p2 = port % 256;

    // 构造响应消息
    char arr[100];
    sprintf(arr, "227 entering passive mode (%s,%s,%s,%s,%d,%d)", str[0], str[1], str[2], str[3], p1, p2);
    send(client_fd, arr, strlen(arr), 0);

    // 等待客户端连接数据端口
    while (1) {
        int data_fd = accept(listen_fd, NULL, NULL);
        if (data_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有新连接，继续等待
                continue;
            } else {
                perror("accept failed");
                break;
            }
        }

        // 处理数据传输（这里可以创建一个新线程）
        fcntl(data_fd, F_SETFL, O_NONBLOCK);
        // TODO: 处理数据传输（如文件上传/下载）
        close(data_fd); // 示例中直接关闭，实际应处理数据
    }

    // 清理
    close(listen_fd);
}