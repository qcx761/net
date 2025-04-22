#define _BSD_SOURCE     // 启用NI_MAXHOST和NI_MAXSERV等定义
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include "is_seqnum.h"  // 自定义头文件（需定义INT_LEN、PORT_NUM等）

void errExit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void fatal(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

// 自定义函数：从套接字读取一行（以\n结尾）
ssize_t readLine(int fd, char *buf, size_t maxLen) {
    ssize_t numRead = 0;
    size_t totRead = 0;
    char ch;

    while (totRead < maxLen - 1) {
        numRead = read(fd, &ch, 1);
        if (numRead == -1) {
            if (errno == EINTR) continue; // 被信号中断则重试
            return -1; // 读取错误
        } else if (numRead == 0) {
            if (totRead == 0) return 0;  // EOF且无数据
            break;                      // EOF但有数据
        }

        if (ch == '\n') break; // 遇到换行符终止
        buf[totRead++] = ch;
    }
    buf[totRead] = '\0'; // 添加字符串终止符
    return totRead;
}

int main(int argc, char *argv[]) {
    int cfd;
    ssize_t numRead;
    char seqNumStr[INT_LEN];  // 存储服务器返回的序列号
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    // 1. 检查命令行参数
    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "Usage: %s server-host [sequence-len]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 2. 配置服务器地址信息
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;      // 允许IPv4或IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP协议
    hints.ai_flags = AI_NUMERICSERV;  // 端口号为数字格式

    if (getaddrinfo(argv[1], PORT_NUM, &hints, &result) != 0)
        errExit("getaddrinfo");

    // 3. 尝试连接服务器
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        cfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (cfd == -1)
            continue; // 失败则尝试下一个地址

        if (connect(cfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; // 连接成功则退出循环

        close(cfd); // 连接失败则关闭套接字
    }

    if (rp == NULL)
        fatal("Could not connect socket to any address");

    freeaddrinfo(result); // 释放地址列表内存

    // 4. 发送请求的序列号长度（默认为1）
    const char *reqLenStr = (argc > 2) ? argv[2] : "1";
    if (write(cfd, reqLenStr, strlen(reqLenStr)) != strlen(reqLenStr))
        fatal("Partial/failed write (reqLenStr)");
    if (write(cfd, "\n", 1) != 1) // 发送终止换行符
        fatal("Partial/failed write (newline)");

    // 5. 读取服务器返回的序列号
    numRead = readLine(cfd, seqNumStr, INT_LEN);
    if (numRead == -1)
        errExit("readLine");
    if (numRead == 0)
        fatal("Unexpected EOF from server");

    printf("Sequence number: %s\n", seqNumStr); // 打印序列号
    close(cfd); // 关闭连接
    exit(EXIT_SUCCESS);
}