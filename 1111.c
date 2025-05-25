#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

// 假设这是你的线程池类（简化版）
class ThreadPool {
    // 简化实现，实际应包含线程管理和任务队列
};

// 全局或类成员变量，用于存储数据连接的套接字信息
std::mutex data_mutex;
std::condition_variable data_cv;
bool data_ready = false;
int data_port = -1;
std::string server_ip_str;

// 数据传输线程函数
void data_transfer_thread(int data_fd, int client_fd) {
    // 这里实现数据传输逻辑，例如文件列表、文件传输等
    // 简化示例，仅接受连接并关闭
    char buffer[1024];
    ssize_t bytes_received = recv(data_fd, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        // 处理接收到的数据
        std::cout << "Received data: " << std::string(buffer, bytes_received) << std::endl;
    }
    close(data_fd);
    close(client_fd);
}

// 解析服务器 IP 地址并转换为字符串
void get_server_ip(std::string& ip_str) {
    char host[256];
    if (gethostname(host, sizeof(host)) == 0) {
        struct hostent* host_entry = gethostbyname(host);
        if (host_entry) {
            struct in_addr** addr_list = (struct in_addr**)host_entry->h_addr_list;
            for(int i = 0; addr_list[i] != nullptr; i++) {
                strcpy(host, inet_ntoa(*addr_list[i]));
                // 假设我们取第一个非回环地址
                if (strcmp(host, "127.0.0.1") != 0) {
                    ip_str = host;
                    return;
                }
            }
        }
    }
    // 默认 IP（假设）
    ip_str = "192.168.1.1";
}

// 将端口号拆分为 p1 和 p2
void split_port(uint16_t port, uint8_t& p1, uint8_t& p2) {
    p1 = (port >> 8) & 0xFF;
    p2 = port & 0xFF;
}

// 处理 PASV 命令
void handle_pasv(int client_fd) {
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

// 辅助函数：发送文件列表（简化示例）
void send_file_list(int data_fd) {
    DIR* dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 获取文件信息
        struct stat statbuf;
        std::string path = entry->d_name;
        if (stat(path.c_str(), &statbuf) < 0) {
            perror("stat");
            continue;
        }

        // 格式化输出（类似 ls -l）
        char time_str[80];
        strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&statbuf.st_mtime));

        std::string file_type = (S_ISDIR(statbuf.st_mode)) ? "d" : "-";
        std::string permissions = 
            (statbuf.st_mode & S_IRUSR) ? "r" : "-" +
            (statbuf.st_mode & S_IWUSR) ? "w" : "-" +
            (statbuf.st_mode & S_IXUSR) ? "x" : "-" +
            (statbuf.st_mode & S_IRGRP) ? "r" : "-" +
            (statbuf.st_mode & S_IWGRP) ? "w" : "-" +
            (statbuf.st_mode & S_IXGRP) ? "x" : "-" +
            (statbuf.st_mode & S_IROTH) ? "r" : "-" +
            (statbuf.st_mode & S_IWOTH) ? "w" : "-" +
            (statbuf.st_mode & S_IXOTH) ? "x" : "-";

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s%s %ld %s %s %8ld %s %s\r\n",
                 file_type.c_str(),
                 permissions.c_str(),
                 (long)statbuf.st_nlink,
                 "user",  // 简化示例
                 "group", // 简化示例
                 (long)statbuf.st_size,
                 time_str,
                 entry->d_name);

        send(data_fd, buffer, strlen(buffer), 0);
    }

    closedir(dir);
}

// 辅助函数：处理 LIST 命令（简化示例）
void handle_list(int client_fd) {
    // 这里应该进入被动模式并获取 data_fd
    // 为了简化，假设已经有一个 data_fd
    int data_fd = 4; // 示例，实际应从 map 获取

    // 发送 150 响应
    send(client_fd, "150 Here comes the directory listing.\r\n", 40, 0);

    // 发送文件列表
    send_file_list(data_fd);

    // 发送 226 响应
    send(client_fd, "226 Directory send OK.\r\n", 25, 0);
}

// 辅助函数：处理 RETR 命令（简化示例）
void handle_retr(int client_fd, const std::string& filename) {
    int data_fd = 4; // 示例，实际应从 map 获取

    // 发送 150 响应
    send(client_fd, "150 Opening BINARY mode data connection.\r\n", 45, 0);

    // 打开文件
    int file_fd = open(filename.c_str(), O_RDONLY);
    if (file_fd < 0) {
        perror("open");
        send(client_fd, "550 Failed to open file.\r\n", 25, 0);
        return;
    }

    // 发送文件内容
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(data_fd, buffer, bytes_read, 0);
    }

    if (bytes_read < 0) {
        perror("read");
    }

    close(file_fd);
    close(data_fd);

    // 发送 226 响应
    send(client_fd, "226 Transfer complete.\r\n", 25, 0);
}

// 辅助函数：处理 STOR 命令（简化示例）
void handle_stor(int client_fd, const std::string& filename) {
    int data_fd = 4; // 示例，实际应从 map 获取

    // 发送 150 响应
    send(client_fd, "150 Ok to send data.\r\n", 22, 0);

    // 打开文件（写入模式）
    int file_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("open");
        send(client_fd, "550 Failed to open file for writing.\r\n", 40, 0);
        return;
    }

    // 接收数据并写入文件
    char buffer[4096];
    ssize_t bytes_received;
    while ((bytes_received = recv(data_fd, buffer, sizeof(buffer), 0)) > 0) {
        write(file_fd, buffer, bytes_received);
    }

    if (bytes_received < 0) {
        perror("recv");
    }

    close(file_fd);
    close(data_fd);

    // 发送 226 响应
    send(client_fd, "226 Transfer complete.\r\n", 25, 0);
}

// 辅助函数：改进的 handle_pasv，使用共享变量存储 data_fd
std::map<int, int> client_data_fds; // client_fd -> data_fd

void handle_pasv_improved(int client_fd) {
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
    uint8_t p1 = (port >> 8) & 0xFF;
    uint8_t p2 = port & 0xFF;

    // 构造 227 响应
    char response[100];
    snprintf(response, sizeof(response), "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             h1, h2, h3, h4, p1, p2);

    // 发送响应给客户端
    send(client_fd, response, strlen(response), 0);

    // 存储 data_fd 到 map 中（注意：这里 data_fd 已经被传递给新线程，可能需要其他同步机制）
    // 由于 data_fd 被新线程接管，这里存储可能不合适，需根据实际设计调整
    // client_data_fds[client_fd] = data_fd; // 可能不正确

    // 更好的做法是在新线程中管理 data_fd，并通过其他方式通知控制线程
    // 由于示例简化，这里不再深入
}

// 更合理的 handle_pasv 实现，使用回调或事件通知
// 由于 C++ 标准库不直接支持线程间高级通信，这里简化处理

// 假设我们有一个函数来获取 client_data_fds 的映射
// 为了简化，这里不实现完整的线程安全 map 操作

// 以下是一个改进的 handle_pasv 实现，假设我们能够安全地存储 data_fd

std::mutex client_mutex;
std::map<int, int> client_data_fds; // client_fd -> data_fd

void handle_pasv(int client_fd) {
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

    // 创建数据传输线程，并传递 data_fd
    std::thread data_thread([data_fd]() {
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
    });

    // 分离线程（假设数据传输完成后线程会自动结束）
    data_thread.detach();

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
    uint8_t p1 = (port >> 8) & 0xFF;
    uint8_t p2 = port & 0xFF;

    // 构造 227 响应
    char response[100];
    snprintf(response, sizeof(response), "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             h1, h2, h3, h4, p1, p2);

    // 发送响应给客户端
    send(client_fd, response, strlen(response), 0);

    // 存储 data_fd 到 map 中
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_data_fds[client_fd] = data_fd; // 注意：data_fd 已经被传递给新线程，这里存储可能不合适
    }

    // 更好的做法是在新线程中管理 data_fd，并通过其他方式通知控制线程
    // 由于示例简化，这里不再深入
}

// 辅助函数：发送文件列表（简化示例）
void send_file_list(int data_fd) {
    DIR* dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 获取文件信息
        struct stat statbuf;
        std::string path = entry->d_name;
        if (stat(path.c_str(), &statbuf) < 0) {
            perror("stat");
            continue;
        }

        // 格式化输出（类似 ls -l）
        char time_str[80];
        strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&statbuf.st_mtime));

        std::string file_type = (S_ISDIR(statbuf.st_mode)) ? "d" : "-";
        std::string permissions = 
            (statbuf.st_mode & S_IRUSR) ? "r" : "-" +
            (statbuf.st_mode & S_IWUSR) ? "w" : "-" +
            (statbuf.st_mode & S_IXUSR) ? "x" : "-" +
            (statbuf.st_mode & S_IRGRP) ? "r" : "-" +
            (statbuf.st_mode & S_IWGRP) ? "w" : "-" +
            (statbuf.st_mode & S_IXGRP) ? "x" : "-" +
            (statbuf.st_mode & S_IROTH) ? "r" : "-" +
            (statbuf.st_mode & S_IWOTH) ? "w" : "-" +
            (statbuf.st_mode & S_IXOTH) ? "x" : "-";

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s%s %ld %s %s %8ld %s %s\r\n",
                 file_type.c_str(),
                 permissions.c_str(),
                 (long)statbuf.st_nlink,
                 "user",  // 简化示例
                 "group", // 简化示例
                 (long)statbuf.st_size,
                 time_str,
                 entry->d_name);

        send(data_fd, buffer, strlen(buffer), 0);
    }

    closedir(dir);
}

// 辅助函数：处理 LIST 命令（简化示例）
void handle_list(int client_fd) {
    // 这里应该进入被动模式并获取 data_fd
    // 为了简化，假设已经有一个 data_fd
    int data_fd = 4; // 示例，实际应从 map 获取

    // 发送 150 响应
    send(client_fd, "150 Here comes the directory listing.\r\n", 40, 0);

    // 发送文件列表
    send_file_list(data_fd);

    // 发送 226 响应
    send(client_fd, "226 Directory send OK.\r\n", 25, 0);
}

// 辅助函数：处理 RETR 命令（简化示例）
void handle_retr(int client_fd, const std::string& filename) {
    int data_fd = 4; // 示例，实际应从 map 获取

    // 发送 150 响应
    send(client_fd, "150 Opening BINARY mode data connection.\r\n", 45, 0);

    // 打开文件
    int file_fd = open(filename.c_str(), O_RDONLY);
    if (file_fd < 0) {
        perror("open");
        send(client_fd, "550 Failed to open file.\r\n", 25, 0);
        return;
    }

    // 发送文件内容
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(data_fd, buffer, bytes_read, 0);
    }

    if (bytes_read < 0) {
        perror("read");
    }

    close(file_fd);
    close(data_fd);

    // 发送 226 响应
    send(client_fd, "226 Transfer complete.\r\n", 25, 0);
}

// 辅助函数：处理 STOR 命令（简化示例）
void handle_stor(int client_fd, const std::string& filename) {
    int data_fd = 4; // 示例，实际应从 map 获取

    // 发送 150 响应
    send(client_fd, "150 Ok to send data.\r\n", 22, 0);

    // 打开文件（写入模式）
    int file_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("open");
        send(client_fd, "550 Failed to open file for writing.\r\n", 40, 0);
        return;
    }

    // 接收数据并写入文件
    char buffer[4096];
    ssize_t bytes_received;
    while ((bytes_received = recv(data_fd, buffer, sizeof(buffer), 0)) > 0) {
        write(file_fd, buffer, bytes_received);
    }

    if (bytes_received < 0) {
        perror("recv");
    }

    close(file_fd);
    close(data_fd);

    // 发送 226 响应
    send(client_fd, "226 Transfer complete.\r\n", 25, 0);
}

// 辅助函数：改进的 handle_pasv，使用共享变量存储 data_fd
std::map<int, int> client_data_fds; // client_fd -> data_fd

void handle_pasv_improved(int client_fd) {
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

    // 创建数据传输线程，并传递 data_fd
    std::thread data_thread([data_fd]() {
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
    });

    // 分离线程（假设数据传输完成后线程会自动结束）
    data_thread.detach();

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
    uint8_t p1 = (port >> 8) & 0xFF;
    uint8_t p2 = port & 0xFF;

    // 构造 227 响应
    char response[100];
    snprintf(response, sizeof(response), "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             h1, h2, h3, h4, p1, p2);

    // 发送响应给客户端
    send(client_fd, response, strlen(response), 0);

    // 存储 data_fd 到 map 中
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_data_fds[client_fd] = data_fd; // 注意：data_fd 已经被传递给新线程，这里存储可能不合适
    }

    // 更好的做法是在新线程中管理 data_fd，并通过其他方式通知控制线程
    // 由于示例简化，这里不再深入
}

// 辅助函数：发送文件列表（简化示例）
void send_file_list(int data_fd) {
    DIR* dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 获取文件信息
        struct stat statbuf;
        std::string path = entry->d_name;
        if (stat