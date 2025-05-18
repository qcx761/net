#include <iostream>
#include <threadpool.hpp> // 线程池的头文件
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

// 向客户端发送响应
void send_response(int client_fd, const std::string& message) {
    std::string response = message + "\r\n"; // FTP协议的CRLF结尾
    send(client_fd, response.c_str(), response.size(), 0); // 发送响应
}

// 从客户端接收命令
std::string receive_command(int control_fd) {
    char buffer[1024]; // 缓冲区
    ssize_t bytes_received = recv(control_fd, buffer, sizeof(buffer) - 1, 0); // 读取数据

    if (bytes_received < 0) {
        return ""; // 读取错误
    }
    
    buffer[bytes_received] = '\0'; // 确保字符串结束
    return std::string(buffer); // 返回接收到的命令
}

// 处理控制命令
void handle_client(int control_fd, threadpool& data_pool) {
    while (true) {
        std::string command = receive_command(control_fd);
        
        if (command == "PASV") {
            handle_pasv_command(control_fd, data_pool);
        } else if (command == "LIST") {
            handle_list_command(control_fd, data_pool);
        } else if (command == "RETR") {
            handle_retr_command(control_fd, data_pool);
        } else if (command == "STOR") {
            handle_stor_command(control_fd, data_pool);
        } else if (command == "QUIT") {
            send_response(control_fd, "221 Goodbye.");
            break; // 退出循环
        } else {
            send_response(control_fd, "500 Unknown command.");
        }
    }
    close(control_fd);
}

// PASV命令处理
void handle_pasv_command(int control_fd, threadpool& data_pool) {
    int data_port = choose_random_port();
    listen_on_port(data_port);
    send_response(control_fd, "227 Entering Passive Mode.");

    int data_fd = accept_connection(data_port);
    data_pool.enqueue(handle_data_transfer, data_fd);
}

// LIST命令处理
void handle_list_command(int control_fd, threadpool& data_pool) {
    int data_port = choose_random_port();
    listen_on_port(data_port);
    send_response(control_fd, "227 Entering Passive Mode.");

    int data_fd = accept_connection(data_port);
    data_pool.enqueue(handle_list_transfer, data_fd);
}

// RETR命令处理
void handle_retr_command(int control_fd, threadpool& data_pool) {
    std::string filename = receive_filename(control_fd);
    int data_port = choose_random_port();
    listen_on_port(data_port);
    send_response(control_fd, "227 Entering Passive Mode.");

    int data_fd = accept_connection(data_port);
    data_pool.enqueue(send_file, data_fd, filename);
}

// STOR命令处理
void handle_stor_command(int control_fd, threadpool& data_pool) {
    std::string filename = receive_filename(control_fd);
    int data_port = choose_random_port();
    listen_on_port(data_port);
    send_response(control_fd, "227 Entering Passive Mode.");

    int data_fd = accept_connection(data_port);
    data_pool.enqueue(receive_file, data_fd, filename);
}

// 数据传输处理
void handle_data_transfer(int data_fd) {
    // 处理数据传输逻辑
}

void handle_list_transfer(int data_fd) {
    // 处理列表传输逻辑
}

void send_file(int data_fd, const std::string& filename) {
    // 发送文件逻辑
}

void receive_file(int data_fd, const std::string& filename) {
    // 接收文件逻辑
}

int main() {
    int server_fd = setup_server();
    
    threadpool control_pool(10); // 控制连接线程池
    threadpool data_pool(10);     // 数据连接线程池

    while (true) {
        int client_fd = accept(server_fd);
        control_pool.enqueue(handle_client, client_fd, std::ref(data_pool));
    }
    
    close(server_fd);
    return 0;
}




int choose_random_port() {
    std::srand(static_cast<unsigned int>(std::time(nullptr))); // 初始化随机数种子
    return std::rand() % (65535 - 1024 + 1) + 1024; // 返回1024到65535之间的随机端口
}

void listen_on_port(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr)); // 清空结构体
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 接受任何地址
    server_addr.sin_port = htons(port); // 转换端口为网络字节序

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding to port" << std::endl;
        close(sockfd);
        return;
    }

    if (listen(sockfd, 5) < 0) {
        std::cerr << "Error listening on port" << std::endl;
        close(sockfd);
        return;
    }

    // 可以返回sockfd，以便后续使用
}

int accept_connection(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_fd < 0) {
        std::cerr << "Error accepting connection" << std::endl;
        return -1; // 返回-1表示出错
    }
    
    return client_fd; // 返回客户端的文件描述符
}

std::string receive_filename(int control_fd) {
    char buffer[1024]; // 缓冲区
    ssize_t bytes_received = recv(control_fd, buffer, sizeof(buffer) - 1, 0); // 读取数据

    if (bytes_received < 0) {
        return ""; // 读取错误
    }
    
    buffer[bytes_received] = '\0'; // 确保字符串结束
    return std::string(buffer); // 返回接收到的文件名
}