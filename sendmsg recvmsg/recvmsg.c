#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    char buf1[10], buf2[10];
    struct iovec iov[2];
    iov[0].iov_base = buf1;
    iov[0].iov_len = sizeof(buf1);
    iov[1].iov_base = buf2;
    iov[1].iov_len = sizeof(buf2);

    struct msghdr msg = {0};
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;

    ssize_t received = recvmsg(sockfd, &msg, 0);
    if (received < 0) {
        perror("recvmsg");
    } else {
        printf("Received %zd bytes\n", received);
        printf("Data: %.*s%.*s\n", (int)iov[0].iov_len, (char *)iov[0].iov_base,
                                   (int)iov[1].iov_len, (char *)iov[1].iov_base);
    }

    close(sockfd);
    return 0;
}