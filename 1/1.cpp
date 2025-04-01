#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>

int main() {
    const char* fifoPath = "/tmp/myfifo"; // FIFO 文件路径

    // 删除已有的 FIFO 文件（如果存在）
    unlink(fifoPath);

    // 创建 FIFO 文件
    if (mkfifo(fifoPath, 0666) == -1) {
        perror("mkfifo error");
        return 1;
    }

    char buffer[128];
    
    std::cout << "Server is waiting for a client to connect...\n";

    // 打开 FIFO 文件进行读操作
    int fd = open(fifoPath, O_RDONLY);
    if (fd == -1) {
        perror("open error");
        return 1;
    }

    while (true) {
        // 从 FIFO 读取数据
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // 添加字符串结尾符
            std::cout << "Received from client: " << buffer << std::endl;

            // 发送回应
            std::string response = "Hello from server!";
            std::cout << "Sending to client: " << response << std::endl;
            write(fd, response.c_str(), response.size() + 1);
        }
    }

    close(fd); // 关闭文件描述符
    unlink(fifoPath); // 删除 FIFO 文件
    return 0;
}