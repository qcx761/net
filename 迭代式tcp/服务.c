#define _BSD_SOURCE     // 启用NI_MAXHOST和NI_MAXSERV等定义
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include "is_seqnum.h"  // 自定义头文件（需定义INT_LEN、PORT_NUM等）

#define BACKLOG 50      // 等待连接队列的最大长度
#define ADDRSTRLEN (NI_MAXHOST + NI_MAXSERV + 10)

void errExit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void errMsg(const char *msg) {
    perror(msg);
}

int main(int argc, char *argv[]) {
    uint32_t seqNum = 0;            // 序列号初始值
    char reqLenStr[INT_LEN];         // 客户端请求的序列号长度
    char seqNumStr[INT_LEN];         // 返回给客户端的序列号
    struct sockaddr_storage claddr;  // 客户端地址信息
    int lfd, cfd, optval, reqLen;
    socklen_t addrlen;
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    // 用于打印客户端地址的缓冲区
    char addrStr[ADDRSTRLEN];
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    // 1. 处理命令行参数（初始序列号）
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "Usage: %s [init-seq-num]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    seqNum = (argc > 1) ? atoi(argv[1]) : 0;

    // 2. 忽略SIGPIPE信号（防止写入断开连接的套接字导致进程终止）
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        errExit("signal");

    // 3. 配置服务器地址信息（支持IPv4/IPv6，通配地址）
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;     // 允许IPv4或IPv6
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV; // 通配地址 + 数字端口

    if (getaddrinfo(NULL, PORT_NUM, &hints, &result) != 0)
        errExit("getaddrinfo");

    // 4. 创建并绑定套接字
    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd == -1)
            continue; // 失败则尝试下一个地址

        // 设置SO_REUSEADDR选项（避免重启时地址占用）
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
            close(lfd);
            continue;
        }

        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; // 绑定成功则退出循环

        close(lfd); // 绑定失败则关闭套接字
    }

    if (rp == NULL)
        errExit("Could not bind socket to any address");

    freeaddrinfo(result); // 释放地址列表内存

    // 5. 开始监听连接
    if (listen(lfd, BACKLOG) == -1)
        errExit("listen");

    // 6. 主循环：迭代处理客户端请求
    for (;;) {
        addrlen = sizeof(struct sockaddr_storage);
        cfd = accept(lfd, (struct sockaddr *) &claddr, &addrlen);
        if (cfd == -1) {
            errMsg("accept");
            continue;
        }

        // 打印客户端地址信息
        if (getnameinfo((struct sockaddr *) &claddr, addrlen,
                       host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
            snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
        else
            snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
        printf("Connection from %s\n", addrStr);

        // 读取客户端请求（序列号长度）
        if (read(cfd, reqLenStr, INT_LEN) <= 0) {
            close(cfd);
            continue; // 读取失败则关闭连接
        }

        reqLen = atoi(reqLenStr);
        if (reqLen <= 0) { // 校验请求合法性
            close(cfd);
            continue;
        }

        // 发送当前序列号并更新
        snprintf(seqNumStr, INT_LEN, "%d\n", seqNum);
        if (write(cfd, seqNumStr, strlen(seqNumStr)) != strlen(seqNumStr))
            fprintf(stderr, "Error on write");
        seqNum += reqLen;

        close(cfd); // 关闭客户端连接
    }

    // 实际运行中不会到达此处（需信号处理或外部终止）
    close(lfd);
    return 0;
}