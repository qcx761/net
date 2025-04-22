#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

int main() {
    struct addrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // IPv4 或 IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    // 获取地址信息
    if ((status = getaddrinfo("www.example.com", "80", &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // 遍历结果
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        // 获取对应的地址
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // 将地址转换为字符串
        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        printf("%s: %s\n", ipver, ipstr);
    }

    freeaddrinfo(res); // 释放地址信息
    return 0;
}


// struct addrinfo {
//     int              ai_flags;     // 标志
//     int              ai_family;    // 地址族（AF_INET 或 AF_INET6）
//     int              ai_socktype;  // 套接字类型（SOCK_STREAM 或 SOCK_DGRAM）
//     int              ai_protocol;  // 协议（通常为 0）
//     socklen_t        ai_addrlen;   // 地址长度
//     struct sockaddr *ai_addr;      // 指向地址的指针
//     char            *ai_canonname; // 标准名称
//     struct addrinfo *ai_next;      // 指向下一个地址信息的指针
// };

// int getaddrinfo(const char *node, const char *service,const struct addrinfo *hints, struct addrinfo **res);