关键区别总结​​ 
​​结构体​​	                     ​​用途​​	                          ​​协议族​​	                              ​​典型字段​​
sockaddr	        通用地址参数（函数接口）	              由 sa_family 决定               sa_family, sa_data
sockaddr_in	        IPv4 地址和端口	                        AF_INET	                       sin_port, sin_addr
sockaddr_in6	    IPv6 地址、端口、流标签、作用域	           AF_INET6	                      sin6_port, sin6_addr, sin6_scope_id
sockaddr_storage	通用地址存储（兼容 IPv4/IPv6）	         动态	                         无固定字段（足够大）
addrinfo	        地址解析结果（含元信息）	              由 ai_family 决定	              ai_family, ai_addr, ai_canonname


struct sockaddr_in {
    short int sin_family;        // 地址族（应设置为 AF_INET）
    unsigned short int sin_port; // 端口号（需要使用 htons() 函数将主机字节序转换为网络字节序）
    struct in_addr sin_addr;     // IP 地址（使用 in_addr 结构体表示）
    unsigned char sin_zero[8];   // 填充字节，使得结构体大小与 sockaddr 一致
};

struct in_addr {
    unsigned long s_addr;        // IPv4 地址（以网络字节序表示）
};


struct sockaddr_in6 {
    sa_family_t     sin6_family;   // 地址族（必须为 AF_INET6）
    in_port_t       sin6_port;     // 端口号（网络字节序）
    uint32_t        sin6_flowinfo; // IPv6 流标签
    struct in6_addr sin6_addr;     // IPv6 地址（128 位）
    uint32_t        sin6_scope_id; // 作用域 ID（如链路本地地址）
};

struct sockaddr_storage {
    sa_family_t ss_family;    // 地址族（AF_INET 或 AF_INET6）
    // 其他字段根据地址族动态扩展
    char __ss_padding[128 - sizeof(sa_family_t)]; // 填充字段（保证对齐）
};



struct addrinfo {
    int              ai_flags;     // 标志（如 AI_PASSIVE）
    int              ai_family;    // 地址族（AF_INET/AF_INET6）
    int              ai_socktype;  // 套接字类型（SOCK_STREAM/SOCK_DGRAM）
    int              ai_protocol;  // 协议（IPPROTO_TCP/IPPROTO_UDP）
    socklen_t        ai_addrlen;   // 地址长度（如 sizeof(sockaddr_in)）
    struct sockaddr *ai_addr;      // 指向具体地址结构的指针
    char            *ai_canonname; // 规范主机名（如解析后的域名）
    struct addrinfo *ai_next;      // 下一个 addrinfo 结构（链表）
};




场景​​	                              ​​涉及的结构体/参数​                 ​	​                         ​常用转换函数​​
​​Socket 绑定/连接​​	                sockaddr_in / sockaddr_in6	                                  inet_pton(), htons()
​​域名解析​​	                        getaddrinfo() 返回的 sockaddr_storage	                      inet_ntop()
​​IP/端口反向解析​                      sockaddr_in / sockaddr_in6	                                  getnameinfo()
​​UDP 发送/接收​​	                     sockaddr_in / sockaddr_in6	                                  inet_pton(), inet_ntop()
​​多路复用 I/O​​	                       sockaddr_in / sockaddr_in6	                             inet_pton()