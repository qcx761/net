#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/sctp.h>

#define SERVER_IP "127.0.0.1"    // 服务器IP地址（本地回环）
#define SERVER_PORT 9876         // 服务器端口号
#define BUFFER_SIZE 1024         // 缓冲区大小

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    struct sctp_sndrcvinfo sinfo;
    int flags;

    // 创建SCTP Sequenced Packet Socket
    if ((sockfd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed for server address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to SCTP Server at %s:%d\n", SERVER_IP, SERVER_PORT);

    // 发送消息（保留边界）
    const char *message = "Hello, SCTP Sequenced Packet!";
    if (sctp_sendmsg(sockfd, message, strlen(message), NULL, 0, 0, 0, 0, 0, 0) < 0) {
        perror("sctp_sendmsg failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Message sent: %s\n", message);

    // 接收回显消息（保留边界）
    memset(buffer, 0, BUFFER_SIZE);
    if (sctp_recvmsg(sockfd, buffer, BUFFER_SIZE, NULL, 0, &sinfo, &flags) < 0) {
        perror("sctp_recvmsg failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Message received from server: %s\n", buffer);

    // 关闭套接字
    close(sockfd);

    return 0;
}