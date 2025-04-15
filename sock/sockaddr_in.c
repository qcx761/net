#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd;
    struct sockaddr_in server_address;

    // 创建套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 配置地址
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字
    bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address));

    // 开始监听
    listen(server_fd, 5);

    // 关闭套接字
    close(server_fd);
    return 0;
}