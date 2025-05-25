oid handle_pasv(int client_fd) {
    std::unique_lock<std::mutex> lock(data_mutex);

    // 创建数据套接字
    int data_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (data_fd < 0) {
        perror("socket");
        send(client_fd, "500 Failed to create data socket\r\n", 30, 0);
        return;
    }

    // 设置 SO_REUSEADDR 避免 "Address already in use" 错误
    int opt = 1;
    if (setsockopt(data_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(data_fd);
        send(client_fd, "500 Failed to set socket options\r\n", 34, 0);
        return;
    }

    // 绑定到随机端口
    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    server_addr.sin_port = 0;  // 0 表示让系统自动分配端口

    if (bind(data_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(data_fd);
        send(client_fd, "500 Failed to bind data socket\r\n", 33, 0);
        return;
    }

    // 开始监听
    if (listen(data_fd, 1) < 0) {  // 1 表示最多允许 1 个连接
        perror("listen");
        close(data_fd);
        send(client_fd, "500 Failed to listen on data socket\r\n", 40, 0);
        return;
    }

    // 获取分配的端口
    socklen_t addr_len = sizeof(server_addr);
    if (getsockname(data_fd, (struct sockaddr*)&server_addr, &addr_len) < 0) {
        perror("getsockname");
        close(data_fd);
        send(client_fd, "500 Failed to get socket name\r\n", 32, 0);
        return;
    }

    uint16_t port = ntohs(server_addr.sin_port);

    // 创建数据传输线程
    std::thread([data_fd]() {
        // 等待客户端连接
        struct sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);
        int client_data_fd = accept(data_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_data_fd < 0) {
            perror("accept");
            close(data_fd);
            return;
        }

        // 这里可以处理数据传输逻辑
        // 例如，根据控制线程的指令进行文件传输等

        // 简化示例，仅关闭连接
        close(client_data_fd);
        close(data_fd);
    }).detach();

    // 获取服务器 IP 地址
    if (server_ip_str.empty()) {
        get_server_ip(server_ip_str);
    }

    // 解析 IP 地址为 h1, h2, h3, h4
    uint8_t h1 = (server_addr.sin_addr.s_addr >> 24) & 0xFF;
    uint8_t h2 = (server_addr.sin_addr.s_addr >> 16) & 0xFF;
    uint8_t h3 = (server_addr.sin_addr.s_addr >> 8) & 0xFF;
    uint8_t h4 = server_addr.sin_addr.s_addr & 0xFF;

    // 拆分端口号为 p1 和 p2
    uint8_t p1, p2;
    split_port(port, p1, p2);

    // 构造 227 响应
    char response[100];
    snprintf(response, sizeof(response), "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             h1, h2, h3, h4, p1, p2);

    // 发送响应给客户端
    send(client_fd, response, strlen(response), 0);

    // 这里可以存储 data_fd 和 client_fd 的映射关系，以便后续处理
    // 例如：std::map<int, int> client_data_fds;  // client_fd -> data_fd

    // 等待数据传输线程完成（简化示例，实际可能需要更复杂的同步机制）
    // 由于线程已分离，这里无法等待，需根据实际需求调整

    // 关闭数据套接字（如果需要由主线程管理，可以不在此关闭）
    // close(data_fd); // 如果线程已接管 data_fd，则不需要关闭
}