#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 2100
#define BUFFER_SIZE 1024

int sock;

void connect_to_server(const char *server_ip);
void send_command(const char *command);
void receive_response();

int main() {
    const char *server_ip = "192.168.1.1"; // 替换为实际服务器 IP
    connect_to_server(server_ip);
    
    // 发送 PASV 命令
    send_command("PASV");
    receive_response();
    
    // 其他命令处理...
    close(sock);
    return 0;
}

void connect_to_server(const char *server_ip) {
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(SERVER_PORT);

    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

void send_command(const char *command) {
    send(sock, command, strlen(command), 0);
}

void receive_response() {
    char buffer[BUFFER_SIZE];
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Server response: %s\n", buffer);
}