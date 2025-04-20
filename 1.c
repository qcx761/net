#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

int main() {
    const char *hostname = "www.example.com";
    struct hostent *he;
    struct in_addr addr;

    // 获取主机信息
    he = gethostbyname(hostname);
    if (he == NULL) {
        herror("gethostbyname");
        return 1;
    }

    // 转换地址
    memcpy(&addr, he->h_addr_list[0], sizeof(struct in_addr));
    printf("IP address: %s\n", inet_ntoa(addr));

    return 0;
}