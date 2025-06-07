#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 2100
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // 创建 socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 附加选项以避免地址已被使用的错误
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址结构
    address.sin_family = AF_INET;                // 选择 IPv4
    address.sin_addr.s_addr = INADDR_ANY;       // 绑定到所有可用接口
    address.sin_port = htons(PORT);               // 设置端口号

    // 绑定 socket 到地址和端口
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 开始监听
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("服务器正在监听...\n");

    // 接受客户端连接
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // 发送消息到客户端
    const char *hello = "Hello, Client!";
    send(new_socket, hello, strlen(hello), 0);
    printf("消息已发送\n");

    // 接收客户端的消息
    int valread = read(new_socket, buffer, BUFFER_SIZE);
    printf("收到消息: %s\n", buffer);

    // 关闭 socket
    close(new_socket);
    close(server_fd);
    
    return 0;
}