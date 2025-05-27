#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 绑定到所有网卡
    addr.sin_port = htons(0);          // 让系统分配端口

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 此时 addr.sin_port 仍然是 0
    printf("sin_port before getsockname: %u\n", ntohs(addr.sin_port));

    // 获取实际分配的端口和 IP
    socklen_t len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getsockname");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 现在 addr.sin_port 是实际分配的端口
    printf("sin_port after getsockname: %u\n", ntohs(addr.sin_port));

    // 获取并打印 IP 地址
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    printf("Bound to IP: %s\n", ip_str);

    close(sockfd);
    return 0;
}