#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main() {
    int sockfd;
    struct sockaddr_un server_addr;

    //struct sockaddr_un {
    //     sa_family_t sun_family;         // 地址族，定义了套接字将使用的协议类型和地址格式，通常为 AF_UNIX
    //     char        sun_path[108];      // 套接字路径
    // };
    
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