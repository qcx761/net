#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

#define CONTROL_PORT 2100
#define BUFFER_SIZE 1024

void *handle_client(void *arg);
void send_response(int client_socket, const char *message);
void handle_pasv(int client_socket);
void list_files(int data_socket);
int create_data_socket();

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 创建服务器 socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(CONTROL_PORT);

    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, (void *)(intptr_t)client_socket);
        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}

void *handle_client(void *arg) {
    int client_socket = (intptr_t)arg;
    char buffer[BUFFER_SIZE];

    while (recv(client_socket, buffer, sizeof(buffer), 0) > 0) {
        if (strncmp(buffer, "PASV", 4) == 0) {
            handle_pasv(client_socket);
        }
        // 处理其他命令...
    }

    close(client_socket);
    return NULL;
}

void handle_pasv(int client_socket) {
    int data_socket = create_data_socket();
    // 获取服务器 IP 和端口号...
    // 这里需要实现 IP 和端口的获取
    send_response(client_socket, "227 entering passive mode (192,168,1,1,19,136)");
}

void send_response(int client_socket, const char *message) {
    send(client_socket, message, strlen(message), 0);
}

int create_data_socket() {
    // 创建并返回数据传输 socket...
    return 0;
}


// 实现文件列表、上传和下载功能...