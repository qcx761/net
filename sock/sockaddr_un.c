#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main() {
    int sockfd;
    struct sockaddr_un server_addr;
    
    // 创建 Unix Domain 套接字
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    // 配置地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, "/tmp/mysocket", sizeof(server_addr.sun_path) - 1);

    // 绑定和其他操作...

    close(sockfd);
    return 0;
}