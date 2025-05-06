#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/sctp.h>

#define SERVER_IP "127.0.0.1"    // 服务端IP地址（本地回环）
#define SERVER_PORT 9876         // 服务端端口号
#define BUFFER_SIZE 1024         // 缓冲区大小

int main() {
    int sockfd, conn_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    struct sctp_sndrcvinfo sinfo;
    int flags;

    // 创建SCTP Sequenced Packet Socket
    if ((sockfd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 配置服务端地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed for server address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 绑定套接字到指定地址和端口
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 监听连接（SCTP的监听与TCP类似）
    if (listen(sockfd, 5) < 0) { // 5为等待连接队列的最大长度
        perror("listen failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("SCTP Server is listening on %s:%d\n", SERVER_IP, SERVER_PORT);

    // 接受客户端连接
    if ((conn_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        perror("accept failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // 接收消息（保留边界）
    memset(buffer, 0, BUFFER_SIZE);
    if (sctp_recvmsg(conn_sock, buffer, BUFFER_SIZE, NULL, 0, &sinfo, &flags) < 0) {
        perror("sctp_recvmsg failed");
        close(conn_sock);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Message received from client: %s\n", buffer);

    // 发送回显消息给客户端（保留边界）
    if (sctp_sendmsg(conn_sock, buffer, strlen(buffer), NULL, 0, 0, 0, 0, 0, 0) < 0) {
        perror("sctp_sendmsg failed");
        close(conn_sock);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Echoed message sent to client.\n");

    // 关闭连接套接字
    close(conn_sock);

    // 关闭主套接字
    close(sockfd);

    return 0;
}