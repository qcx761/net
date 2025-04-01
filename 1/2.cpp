#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>

int main() {
    const char* fifoPath = "/tmp/myfifo"; // FIFO 文件路径

    // 打开 FIFO 文件进行写操作
    int fd = open(fifoPath, O_WRONLY);
    if (fd == -1) {
        perror("open error");
        return 1;
    }

    std::string message;
    std::cout << "Enter your message to the server (type 'exit' to quit):\n";

    while (true) {
        std::getline(std::cin, message);
        if (message == "exit") { // 输入 exit 结束
            break;
        }

        // 发送消息到服务器
        write(fd, message.c_str(), message.size() + 1);
        
        // 打开 FIFO 文件进行读操作以获取服务器的响应
        int read_fd = open(fifoPath, O_RDONLY);
        char buffer[128];
        ssize_t bytesRead = read(read_fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // 添加字符串结尾符
            std::cout << "Received from server: " << buffer << std::endl;
        }
        close(read_fd); // 关闭读文件描述符
    }

    close(fd); // 关闭写文件描述符
    return 0;
}