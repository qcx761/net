#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <ctime>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <condition_variable>
#include <sstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <system_error>

using namespace std;

#define PORT 2100
#define PORT1 5000
#define SIZE 1024
#define EPSIZE 1024
#define maxevents 1024

// 全局变量
int epfd;
int server_fd; // 全局变量
bool is_continue = false;
std::mutex mtx; // 互斥锁

// 线程池（未实现）
// threadpool control_pool(10); // 控制连接线程池
// threadpool data_pool(10);     // 数据连接线程池

// 控制连接类
class ControlConnect {
public:
    int control_fd; // 控制连接的文件描述符
    string filename; // 文件名
    int n; // 命令类型（1=PASV, 2=LIST, 3=RETR, 4=STOR）

    ControlConnect(int fd, int m, const string& buf) : control_fd(fd), n(m), filename(buf) {}

    void set_msg(int m) {
        n = m;
    }
};

// 数据连接类
class DataConnect {
public:
    int data_fd; // 数据连接的文件描述符

    DataConnect(int fd) : data_fd(fd) {}
};

// 连接组管理类
class ConnectionGroup {
public:
    vector<ControlConnect> control_connections;
    vector<DataConnect> data_connections;
    unordered_map<int, int> data_to_control; // data_fd -> control_fd

    // 初始化控制连接
    void get_init_control(int fd, int n, const string& buf) {
        auto it = find_if(control_connections.begin(), control_connections.end(),
                          [fd](const ControlConnect& conn) { return conn.control_fd == fd; });

        if (it == control_connections.end()) {
            control_connections.emplace_back(fd, n, buf);
        } else {
            if (!buf.empty()) {
                it->filename = buf;
            }
            it->set_msg(n);
        }
    }

    // 初始化数据连接
    void get_init_data(int data_fd) {
        data_connections.emplace_back(data_fd);
    }

    // 添加连接（control_fd 和 data_fd 关联）
    void add_connection(int control_fd, int data_fd) {
        data_to_control[data_fd] = control_fd; // 存储反向映射
    }

    // 通过 data_fd 查找 control_fd
    int find_control_fd(int data_fd) {
        auto it = data_to_control.find(data_fd);
        if (it != data_to_control.end()) {
            return it->second;
        }
        return -1; // 未找到
    }

    void remove_control_connection(int fd) {
        auto it = find_if(control_connections.begin(), control_connections.end(),
                          [fd](const ControlConnect& conn) { return conn.control_fd == fd; });

        if (it != control_connections.end()) {
            control_connections.erase(it); // 删除单个元素
        }

        // 清理 data_to_control 中指向该 control_fd 的映射
        for (auto it = data_to_control.begin(); it != data_to_control.end();) {
            if (it->second == fd) {
                it = data_to_control.erase(it); // 删除并返回下一个迭代器
            } else {
                ++it;
            }
        }
    }

    int find_n(int fd) {
        auto it = find_if(control_connections.begin(), control_connections.end(),
                          [fd](const ControlConnect& conn) { return conn.control_fd == fd; });

        if (it == control_connections.end()) {
            return 0;
        } else {
            return it->n;
        }
    }

    string find_filename(int fd) {
        auto it = find_if(control_connections.begin(), control_connections.end(),
                          [fd](const ControlConnect& conn) { return conn.control_fd == fd; });

        if (it == control_connections.end()) {
            return "";
        } else {
            return it->filename;
        }
    }
};

// 处理 LIST 命令（传输目录下的文件）
void handle_list(int data_fd) {
    DIR* dir;
    struct dirent* entry;
    string buffer;

    dir = opendir(".");
    if (!dir) {
        send_str(data_fd, "550 Failed to open directory.\r\n");
        return;
    }

    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        buffer += entry->d_name;
        buffer += "\r\n";
    }

    closedir(dir);
    send_str(data_fd, buffer.c_str());
    send_str(data_fd, "226 Transfer complete.\r\n");
}

// 服务端将指定的文件传输给客户端
void handle_retr(int data_fd, const string& filename) {
    int file_fd = open(filename.c_str(), O_RDONLY);
    if (file_fd == -1) {
        send_str(data_fd, "550 File not found.\r\n");
        return;
    }

    struct stat statbuf;
    if (fstat(file_fd, &statbuf) == -1) { // 获取文件大小
        close(file_fd);
        send_str(data_fd, "550 Failed to get file size.\r\n");
        return;
    }

    // 偏移量,记录offset可支持端点续传？？
    off_t offset = 0;
    ssize_t bytes_sent = sendfile(data_fd, file_fd, &offset, statbuf.st_size);

    if (bytes_sent == -1) {
        perror("sendfile failed");
        close(file_fd);
        send_str(data_fd, "426 Connection closed; transfer aborted.\r\n");
        return;
    }

    close(file_fd);
    send_str(data_fd, "226 Transfer complete.\r\n");
}

// 服务端准备接收并存储客户端传输的文件
void handle_stor(int data_fd, const string& filename) {
    // 打开文件 如果存在则覆盖,不存在则创建
    int file_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("open failed");
        send_str(data_fd, "550 Failed to create file.\r\n");
        return;
    }

    char buffer[4096];
    ssize_t bytes_received;

    // 循环接收数据并写入文件
    while ((bytes_received = recv(data_fd, buffer, sizeof(buffer), 0)) > 0) {
        ssize_t bytes_written = write(file_fd, buffer, bytes_received);
        if (bytes_written == -1) {
            perror("write failed");
            close(file_fd);
            send_str(data_fd, "451 Write error.\r\n");
            return;
        }
    }

    // 检查接收是否出错
    if (bytes_received == -1) {
        perror("recv failed");
        close(file_fd);
        send_str(data_fd, "426 Connection closed; transfer aborted.\r\n");
        return;
    }

    // 关闭文件并发送成功响应
    close(file_fd);
    send_str(data_fd, "226 Transfer complete.\r\n");
}

// 发送字符串的辅助函数
void send_str(int fd, const string& msg) {
    send(fd, msg.c_str(), msg.size(), 0);
}

// 接收字符串的辅助函数
ssize_t recv_str(int fd, string& buf) {
    char temp[4096];
    ssize_t bytes = recv(fd, temp, sizeof(temp), 0);
    if (bytes > 0) {
        buf.assign(temp, bytes);
    }
    return bytes;
}

// 控制连接的建立
void handle_accept(int fd, ConnectionGroup& group) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int connect_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
    if (connect_fd == -1) {
        perror("accept");
        return;
    }

    // 设置非阻塞模式
    fcntl(connect_fd, F_SETFL, fcntl(connect_fd, F_GETFL, 0) | O_NONBLOCK);

    // 注册到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR; // 客户端向服务端发送信息触发
    ev.data.fd = connect_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, connect_fd, &ev) == -1) {
        perror("epoll_ctl");
        close(connect_fd);
    }
}

// 处理控制消息
void handle_control_msg(const string& buf, int server_fd, ConnectionGroup& group) {
    vector<string> tokens;
    string token;
    stringstream ss(buf);
    while (ss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return;

    string command = tokens[0];
    if (command == "PASV") {
        // 处理 PASV 命令
        int port = 5000; // 示例端口，实际应随机生成
        int p1 = port / 256;
        int p2 = port % 256;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server_fd), ip_str, sizeof(ip_str)); // 错误示例，server_fd 不是 IP

        // 正确获取 IP 的方式（需要从 socket 获取）
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        if (getsockname(server_fd, (struct sockaddr*)&addr, &addr_len) == -1) {
            perror("getsockname failed");
            return;
        }
        inet_ntop(AF_INET, &(addr.sin_addr), ip_str, sizeof(ip_str));

        char str[4][4];
        sscanf(ip_str, "%3[^.].%3[^.].%3[^.].%3[^.]", str[0], str[1], str[2], str[3]);
        char arr[100];
        snprintf(arr, sizeof(arr), "227 entering passive mode (%s,%s,%s,%s,%d,%d)", str[0], str[1], str[2], str[3], p1, p2);
        send_str(server_fd, arr); // 通过控制连接发送信息

        // 先发送端口号和 IP 再注册？？？？？？？？？？   要注册吗？？？？？？？？？？？？？？？？？
        struct epoll_event ev;
        ev.data.fd = server_fd; // 错误示例，应该是 listen_fd
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
            perror("epoll_ctl failed");
            return;
        }

        // listen_fd 数据连接套接字    control_fd 控制连接套接字

        {
            unique_lock<mutex> lock(mtx);
            is_continue = true;
        }

        // 模拟 PASV 处理线程
        thread client_thread([server_fd, group]() {
            handle_pasv(server_fd, group);
        });
        client_thread.detach();
    } else if (command == "LIST") {
        // 处理 LIST 命令
        if (tokens.size() > 1) {
            group.get_init_control(server_fd, 2, tokens[1]);
        } else {
            group.get_init_control(server_fd, 2, "");
        }
    } else if (command == "RETR") {
        // 处理 RETR 命令
        if (tokens.size() > 1) {
            group.get_init_control(server_fd, 3, tokens[1]);
        } else {
            group.get_init_control(server_fd, 3, "");
        }
    } else if (command == "STOR") {
        // 处理 STOR 命令
        if (tokens.size() > 1) {
            group.get_init_control(server_fd, 4, tokens[1]);
        } else {
            group.get_init_control(server_fd, 4, "");
        }
    } else if (command == "QUIT") {
        // 处理 QUIT 命令
        group.remove_control_connection(server_fd); // 取消关联
        epoll_ctl(epfd, EPOLL_CTL_DEL, server_fd, nullptr);
        close(server_fd);
    } else {
        // 其他命令
        send_str(server_fd, "500 Unknown command\r\n");
    }
}

// 数据连接创建（PASV 模式）
void handle_pasv(int control_fd, ConnectionGroup& group) {
    // 服务端控制线程接收到 PASV 请求后，创建一个数据传输线程，并将生成的端口号告知客户端控制线程，
    // 返回 227 entering passive mode (h1,h2,h3,h4,p1,p2)，其中端口号为 p1 * 256+p2，IP 地址为 h1.h2.h3.h4.

    // 假设服务器的 IP 地址为 192.168.1.1，生成的端口号为 5000
    // 那么返回的响应将是：227 entering passive mode (192,168,1,1,19,136)
    // 其中 19 和 136 分别是 5000 的高位和低位字节（5000 = 19 * 256 + 136）

    srand(time(nullptr));
    int port = rand() % 40000 + 1024;
    int p1 = port / 256;
    int p2 = port % 256;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket failed");
        return;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        close(listen_fd);
        return;
    }

    if (listen(listen_fd, 5) == -1) {
        perror("listen failed");
        close(listen_fd);
        return;
    }

    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    socklen_t len = sizeof(addr);
    if (getsockname(listen_fd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getsockname failed");
        return;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));

    char str[4][4];
    sscanf(ip_str, "%3[^.].%3[^.].%3[^.].%3[^.]", str[0], str[1], str[2], str[3]);
    char arr[100];
    snprintf(arr, sizeof(arr), "227 entering passive mode (%s,%s,%s,%s,%d,%d)", str[0], str[1], str[2], str[3], p1, p2);
    send_str(control_fd, arr); // 通过控制连接发送信息

    // 注册到 epoll
    struct epoll_event ev;
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        return;
    }

    while (is_continue) {
        int n = group.find_n(control_fd);
        string filename = group.find_filename(control_fd);

        if (n == 2) {
            handle_list(listen_fd); // 错误示例，LIST 应该在控制连接处理
        } else if (n == 3) {
            handle_retr(listen_fd, filename);
            // free(filename); // 错误，filename 是 std::string
        } else if (n == 4) {
            handle_stor(listen_fd, filename);
            // free(filename); // 错误，filename 是 std::string
        } else {
            continue;
        }

        {
            unique_lock<mutex> lock(mtx);
            is_continue = false;
        }
    }
}

// FTP 初始化
void FTP_init() {
    // int client_fd;
    struct sockaddr_in ser_addr; //,cli_addr
    socklen_t ser_len; // ,cli_len

    memset(&ser_addr, 0, sizeof(ser_addr));
    // memset(&cli_addr,0,sizeof(cli_addr));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(-1);
        //exit(EXIT_FAILURE);
    }

    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(PORT);
    ser_len = sizeof(ser_addr);

    if (bind(server_fd, (struct sockaddr*)&ser_addr, ser_len) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(-1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(-1);
    }

    epfd = epoll_create(EPSIZE);
    if (epfd == -1) {
        perror("epoll_create failed");
        return;
    }

    struct epoll_event ev;
    ev.data.fd = server_fd;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        return;
    }
}

// 控制连接的建立
void handle_accept(int fd, ConnectionGroup& group) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int connect_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
    if (connect_fd == -1) {
        perror("accept");
        return;
    }

    // 设置非阻塞模式
    fcntl(connect_fd, F_SETFL, fcntl(connect_fd, F_GETFL, 0) | O_NONBLOCK);

    // 注册到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR; // 客户端向服务端发送信息触发
    ev.data.fd = connect_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, connect_fd, &ev) == -1) {
        perror("epoll_ctl");
        close(connect_fd);
    }
}

// FTP 启动
void FTP_start(ConnectionGroup& group) {
    while (true) {
        struct epoll_event events[maxevents];
        int n = epoll_wait(epfd, events, maxevents, -1);
        if (n == -1) {
            perror("epoll_wait failed");
            break;
        }
        for (int i = 0; i < n; i++) {
            if (events[i].events & (EPOLLERR | EPOLLRDHUP)) { // 处理错误或连接关闭
                printf("Client error or disconnected: %d\n", events[i].data.fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                close(events[i].data.fd);
                continue;
            }
            if (events[i].data.fd == server_fd) { // 客户端连接
                handle_accept(server_fd, group);
            } else { // 数据连接和控制连接触发
                int fd = events[i].data.fd;
                if (get_port(fd) == PORT) { // 控制连接
                    char buf[1024];
                    ssize_t len = read(events[i].data.fd, buf, sizeof(buf) - 1);
                    if (len <= 0) {
                        if (len == 0) { // 客户端主动关闭
                            printf("Client disconnected: %d\n", events[i].data.fd);
                        } else { // 连接出错
                            perror("read failed");
                        }
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                        close(events[i].data.fd);
                    } else {
                        buf[len] = '\0';
                        handle_control_msg(buf, fd, group);
                    }
                } else { // 数据连接
                    if (events[i].events & EPOLLIN) { // 处理可读事件
                        // 接收文件中
                    } else if (events[i].events & EPOLLOUT) { // 处理可写事件
                        // 上传文件中
                    } else { // 不知道还有啥
                        return;
                    }

                    // 客户端发送信息
                    // 数据连接

                    // fd转换到client_fd
                    // 实现数据传输要放在哪

                    // 判断是写入还是读取触发

                    // 判断是在执行哪个命令，读取？写入？

                    // list 写入   其余 。。
                    // 再传入一个参数？？

                    // auto future2 = data_pool.enqueue(handle_msg, group, fd);
                    // future2.get();
                }
            }
        }
    }
}

// 获取端口号（示例函数，实际应从 epoll_event 获取）
int get_port(int fd) {
    // 示例实现，实际应从 epoll_event 获取
    return PORT;
}

int main() {
    ConnectionGroup group;
    // 初始化 group
    FTP_init();
    FTP_start(group);
    close(server_fd);
    return 0;

    // epdf  server_fd   event封装
}

// 线程池设计（未实现）
// 控制连接线程池:

// 用于处理客户端的控制连接和命令（如 USER、PASS、PASV 等）。
// 每当有新的控制连接时，从这个线程池中获取一个线程来处理。
// 数据连接线程池:

// 用于处理数据传输（如文件上传和下载）。
// 当在控制连接中接收到 PASV 命令并建立数据连接后，从这个线程池中获取一个线程来处理数据传输。