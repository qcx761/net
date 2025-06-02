





void handle_retr(int data_fd, const char *filename) {
    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        send(data_fd, "550 File not found.\r\n", 20, 0);
        return;
    }

    // 获取文件大小
    struct stat statbuf;
    fstat(file_fd, &statbuf);
    off_t file_size = statbuf.st_size;

    // 发送文件数据
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(data_fd, buffer, bytes_read, 0);
    }

    close(file_fd);
    send(data_fd, "226 Transfer complete.\r\n", 24, 0);  // 传输完成
}





#include <fcntl.h>
#include <unistd.h>

void handle_stor(int data_fd, const char *filename) {
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        send(data_fd, "550 Failed to create file.\r\n", 28, 0);
        return;
    }

    // 接收文件数据
    char buffer[4096];
    ssize_t bytes_received;
    while ((bytes_received = recv(data_fd, buffer, sizeof(buffer), 0)) > 0) {
        write(file_fd, buffer, bytes_received);
    }

    close(file_fd);
    send(data_fd, "226 Transfer complete.\r\n", 24, 0);  // 传输完成
}