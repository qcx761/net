#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

#define CONTROL_PORT 21
#define DATA_PORT 20

class FTPServer {
public:
    FTPServer() {
        // 创建控制连接
        control_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (control_socket < 0) {
            perror("Failed to create control socket");
            exit(1);
        }
        
        sockaddr_in control_addr;
        memset(&control_addr, 0, sizeof(control_addr));
        control_addr.sin_family = AF_INET;
        control_addr.sin_addr.s_addr = INADDR_ANY;
        control_addr.sin_port = htons(CONTROL_PORT);

        if (bind(control_socket, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0) {
            perror("Control socket bind failed");
            exit(1);
        }

        listen(control_socket, 5);
        std::cout << "FTP Server is listening on port " << CONTROL_PORT << std::endl;

        // 接受控制连接
        while (true) {
            int client_socket = accept(control_socket, nullptr, nullptr);
            std::thread(&FTPServer::handleControlConnection, this, client_socket).detach();
        }
    }

private:
    int control_socket;

    void handleControlConnection(int client_socket) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        
        // 发送欢迎信息
        std::string welcome_msg = "220 Welcome to FTP Server\r\n";
        send(client_socket, welcome_msg.c_str(), welcome_msg.size(), 0);

        // 接收命令
        while (recv(client_socket, buffer, sizeof(buffer), 0) > 0) {
            std::string command(buffer);
            std::cout << "Received command: " << command << std::endl;

            if (command.substr(0, 4) == "RETR") {
                // 处理文件传输
                handleDataTransfer(client_socket);
            } else if (command.substr(0, 4) == "QUIT") {
                std::string response = "221 Goodbye\r\n";
                send(client_socket, response.c_str(), response.size(), 0);
                break;
            }

            memset(buffer, 0, sizeof(buffer));
        }

        close(client_socket);
    }

    void handleDataTransfer(int control_socket) {
        int data_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (data_socket < 0) {
            perror("Failed to create data socket");
            return;
        }

        sockaddr_in data_addr;
        memset(&data_addr, 0, sizeof(data_addr));
        data_addr.sin_family = AF_INET;
        data_addr.sin_addr.s_addr = INADDR_ANY;
        data_addr.sin_port = htons(DATA_PORT);

        // 绑定和监听数据连接
        if (bind(data_socket, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
            perror("Data socket bind failed");
            return;
        }

        listen(data_socket, 5);
        std::string data_response = "150 Opening data connection.\r\n";
        send(control_socket, data_response.c_str(), data_response.size(), 0);

        // 等待数据连接
        int data_client_socket = accept(data_socket, nullptr, nullptr);
        std::cout << "Data connection established." << std::endl;

        // 模拟文件传输
        std::string file_data = "This is the file content.\r\n";
        send(data_client_socket, file_data.c_str(), file_data.size(), 0);

        close(data_client_socket);
        close(data_socket);
    }
};

int main() {
    FTPServer server;
    return 0;
}