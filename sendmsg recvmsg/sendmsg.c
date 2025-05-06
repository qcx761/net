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
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    char buf1[] = "Hello, ";
    char buf2[] = "World!";
    struct iovec iov[2];
    iov[0].iov_base = buf1;
    iov[0].iov_len = strlen(buf1);
    iov[1].iov_base = buf2;
    iov[1].iov_len = strlen(buf2);

    struct msghdr msg = {0};
    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;

    ssize_t sent = sendmsg(sockfd, &msg, 0);
    if (sent < 0) {
        perror("sendmsg");
    } else {
        printf("Sent %zd bytes\n", sent);
    }

    close(sockfd);
    return 0;
}