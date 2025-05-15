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