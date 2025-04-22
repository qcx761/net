#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

int main() {
    struct addrinfo hints, *res, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 或 IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP 

    if ((status = getaddrinfo("example.com", "80", &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    // 遍历所有返回的地址
    for (p = res; p != NULL; p = p->ai_next) {
        char ipstr[INET6_ADDRSTRLEN];
        void *addr;

        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("IP: %s\n", ipstr);
    }

    freeaddrinfo(res); // 释放内存
    return 0;
}


// struct addrinfo {
//     int              ai_flags;       // 控制行为的标志（如 AI_PASSIVE, AI_CANONNAME）
//     int              ai_family;      // 地址族（如 AF_INET, AF_INET6, AF_UNSPEC）
//     int              ai_socktype;    // 套接字类型（如 SOCK_STREAM, SOCK_DGRAM）
//     int              ai_protocol;    // 协议（如 IPPROTO_TCP, IPPROTO_UDP）

//     socklen_t        ai_addrlen;     // ai_addr 的字节长度                              //
//     struct sockaddr *ai_addr;        // 指向 sockaddr 结构体的指针（存储IP地址）           // 初始化为0仍会返回
//     char            *ai_canonname;   // 主机的规范名称（如果设置了 AI_CANONNAME）          //
//     struct addrinfo *ai_next;        // 指向下一个 addrinfo 结构（链表形式）               //
// };

// 1. ai_flags - 控制行为的标志
// 作用：控制 getaddrinfo() 函数的行为方式。

// 常用标志：

// AI_PASSIVE：返回的地址适合用于 bind() 调用（通常用于服务器端）。如果指定此标志且主机名为 NULL，则返回的 IP 地址将是通配地址（INADDR_ANY 或 IN6ADDR_ANY_INIT）。

// AI_CANONNAME：请求返回主机的规范名称（存储在 ai_canonname 字段中）。

// AI_NUMERICHOST：指示主机名是数字格式的地址字符串（如 "192.0.2.1"），避免 DNS 查询。

// AI_NUMERICSERV：指示服务名是数字端口号（如 "80" 而不是 "http"）。

// AI_V4MAPPED：如果指定了 IPv6 但找不到 IPv6 地址，则返回 IPv4 映射的 IPv6 地址。

// AI_ALL：同时查询 IPv4 和 IPv6 地址（与 AI_V4MAPPED 一起使用）。

// 2. ai_family - 地址族
// 作用：指定期望的地址族（协议族）。

// 常用值：

// AF_INET：IPv4 地址

// AF_INET6：IPv6 地址

// AF_UNSPEC：不指定地址族，返回所有可能的地址（IPv4 和 IPv6）

// 3. ai_socktype - 套接字类型
// 作用：指定套接字类型，用于过滤返回的地址。

// 常用值：

// SOCK_STREAM：面向连接的流式套接字（TCP）

// SOCK_DGRAM：无连接的数据报套接字（UDP）

// 0：不指定类型，返回所有可能的套接字类型

// 4. ai_protocol - 协议类型
// 作用：指定传输层协议。

// 常用值：

// IPPROTO_TCP：TCP 协议

// IPPROTO_UDP：UDP 协议

// 0：不指定协议，由系统根据 ai_socktype 自动选择

// 5. ai_addrlen - 地址长度
// 作用：存储 ai_addr 指向的地址结构的长度（字节数）。

// 特点：

// 由 getaddrinfo() 填充

// 对于 IPv4 通常是 sizeof(struct sockaddr_in)

// 对于 IPv6 通常是 sizeof(struct sockaddr_in6)

// 6. ai_addr - 套接字地址结构指针
// 作用：指向一个 sockaddr 结构，包含实际的网络地址和端口信息。

// 特点：

// 可以强制转换为 struct sockaddr_in（IPv4）或 struct sockaddr_in6（IPv6）

// 包含 IP 地址和端口号

// 可以直接用于 connect(), bind(), sendto() 等套接字函数

// 7. ai_canonname - 主机的规范名称
// 作用：存储主机的规范名称（如果 ai_flags 包含 AI_CANONNAME）。

// 特点：

// 由 getaddrinfo() 分配内存

// 必须通过 freeaddrinfo() 释放

// 如果不需要规范名称，此字段为 NULL

// 8. ai_next - 链表指针
// 作用：指向下一个 addrinfo 结构，形成链表。

// 特点：

// getaddrinfo() 可能返回多个地址（如一个主机有多个 IP 地址）

// 链表中的每个节点代表一个可能的地址

// 最后一个节点的 ai_next 为 NULL

// 必须遍历整个链表来检查所有返回的地址