#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>

#define PORT 2100
#define PORT1 5000
#define BUFFER_SIZE 1024

using namespace std;

// 解析服务器返回的 PASV 响应，提取 IP 和端口
void parse_pasv_response(const string& response, string& ip, int& port) {
    size_t start = response.find('(');
    size_t end = response.find(')');
    if (start == string::npos || end == string::npos) {
        cerr << "Invalid PASV response format" << endl;
        exit(1);
    }

    string pasv_data = response.substr(start + 1, end - start - 1);
    stringstream ss(pasv_data);
    vector<int> parts;
    int num;

    while (ss >> num) {
        parts.push_back(num);
        if (ss.peek() == ',') ss.ignore();
    }

    // 提取 IP 和端口
    ip = to_string(parts[0]) + "." + to_string(parts[1]) + "." + to_string(parts[2]) + "." + to_string(parts[3]);
    port = parts[4] * 256 + parts[5];
}

// 发送命令并接收响应
string send_command(int sockfd, const string& cmd) {
    send(sockfd, cmd.c_str(), cmd.size(), 0);
    char buffer[BUFFER_SIZE];
    string response;

    while (true) {
        ssize_t bytes = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            cerr << "Connection closed by server" << endl;
            exit(1);
        }
        buffer[bytes] = '\0';
        response += buffer;

        // 检查是否收到完整响应（以 \r\n 结尾）
        if (response.find("\r\n") != string::npos) {
            break;
        }
    }

    // 去掉 \r\n
    if (!response.empty() && response.back() == '\n') {
        response.pop_back();
    }
    if (!response.empty() && response.back() == '\r') {
        response.pop_back();
    }

    return response;
}

// 连接数据端口
int connect_data_port(const string& ip, int port) {
    int data_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (data_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(data_fd);
        exit(1);
    }

    if (connect(data_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(data_fd);
        exit(1);
    }

    return data_fd;
}

// LIST 命令
void ftp_list(int control_fd) {
    string cmd = "LIST\r\n";
    string response = send_command(control_fd, cmd);

    if (response.substr(0, 3) == "500") {
        cerr << "Error: " << response << endl;
        return;
    }

    // 检查是否是 PASV 模式
    if (response.find("227") == 0) {
        string ip, pasv_ip;
        int port, pasv_port;
        parse_pasv_response(response, pasv_ip, pasv_port);

        // 连接数据端口
        int data_fd = connect_data_port(pasv_ip, pasv_port);

        // 重新发送 LIST 命令（有些服务器需要）
        send_command(control_fd, cmd);

        // 接收文件列表
        char buffer[BUFFER_SIZE];
        while (true) {
            ssize_t bytes = recv(data_fd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            cout << buffer;
        }

        close(data_fd);
    } else {
        cerr << "Unsupported LIST mode" << endl;
    }

    // 检查传输完成
    response = send_command(control_fd, "LIST\r\n"); // 有些服务器需要再次发送 LIST
    if (response.substr(0, 3) != "226") {
        cerr << "Error: Transfer failed" << endl;
    }
}

// RETR 命令（下载文件）
void ftp_retr(int control_fd, const string& filename) {
    string cmd = "RETR " + filename + "\r\n";
    string response = send_command(control_fd, cmd);

    if (response.substr(0, 3) == "500") {
        cerr << "Error: " << response << endl;
        return;
    }

    // 检查是否是 PASV 模式
    if (response.find("227") == 0) {
        string ip, pasv_ip;
        int port, pasv_port;
        parse_pasv_response(response, pasv_ip, pasv_port);

        // 连接数据端口
        int data_fd = connect_data_port(pasv_ip, pasv_port);

        // 重新发送 RETR 命令（有些服务器需要）
        send_command(control_fd, cmd);

        // 接收文件
        FILE* file = fopen(filename.c_str(), "wb");
        if (!file) {
            perror("fopen");
            close(data_fd);
            return;
        }

        char buffer[BUFFER_SIZE];
        while (true) {
            ssize_t bytes = recv(data_fd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            fwrite(buffer, 1, bytes, file);
        }

        fclose(file);
        close(data_fd);

        // 检查传输完成
        response = send_command(control_fd, "RETR " + filename + "\r\n");
        if (response.substr(0, 3) != "226") {
            cerr << "Error: Transfer failed" << endl;
        } else {
            cout << "File downloaded successfully" << endl;
        }
    } else {
        cerr << "Unsupported RETR mode" << endl;
    }
}

// STOR 命令（上传文件）
void ftp_stor(int control_fd, const string& filename) {
    string cmd = "STOR " + filename + "\r\n";
    string response = send_command(control_fd, cmd);

    if (response.substr(0, 3) == "500") {
        cerr << "Error: " << response << endl;
        return;
    }

    // 检查是否是 PASV 模式
    if (response.find("227") == 0) {
        string ip, pasv_ip;
        int port, pasv_port;
        parse_pasv_response(response, pasv_ip, pasv_port);

        // 连接数据端口
        int data_fd = connect_data_port(pasv_ip, pasv_port);

        // 重新发送 STOR 命令（有些服务器需要）
        send_command(control_fd, cmd);

        // 发送文件
        FILE* file = fopen(filename.c_str(), "rb");
        if (!file) {
            perror("fopen");
            close(data_fd);
            return;
        }

        char buffer[BUFFER_SIZE];
        while (true) {
            ssize_t bytes = fread(buffer, 1, BUFFER_SIZE - 1, file);
            if (bytes <= 0) break;
            send(data_fd, buffer, bytes, 0);
        }

        fclose(file);
        close(data_fd);

        // 检查传输完成
        response = send_command(control_fd, "STOR " + filename + "\r\n");
        if (response.substr(0, 3) != "226") {
            cerr << "Error: Transfer failed" << endl;
        } else {
            cout << "File uploaded successfully" << endl;
        }
    } else {
        cerr << "Unsupported STOR mode" << endl;
    }
}

// 登录 FTP 服务器
int ftp_login(const string& server_ip) {
    int control_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (control_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(control_fd);
        exit(1);
    }

    if (connect(control_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(control_fd);
        exit(1);
    }

    // 接收欢迎消息
    char buffer[BUFFER_SIZE];
    recv(control_fd, buffer, BUFFER_SIZE - 1, 0);
    buffer[BUFFER_SIZE - 1] = '\0';
    cout << "Server: " << buffer << endl;

    // 发送 USER 命令
    string response = send_command(control_fd, "USER anonymous\r\n");
    cout << "USER response: " << response << endl;

    // 发送 PASS 命令
    response = send_command(control_fd, "PASS anonymous@\r\n");
    cout << "PASS response: " << response << endl;

    return control_fd;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0]<< " <server_ip>" << endl;
        return 1;
    }

    string server_ip = argv[1];
    int control_fd = ftp_login(server_ip);

    while (true) {
        cout << "FTP> ";
        string cmd;
        getline(cin, cmd);

        if (cmd.empty()) continue;

        if (cmd == "quit" || cmd == "exit") {
            send_command(control_fd, "QUIT\r\n");
            close(control_fd);
            break;
        } else if (cmd.find("list") == 0) {
            ftp_list(control_fd);
        } else if (cmd.find("retr") == 0) {
            size_t space_pos = cmd.find(' ', 5);
            if (space_pos == string::npos) {
                cerr << "Usage: retr <filename>" << endl;
                continue;
            }
            string filename = cmd.substr(space_pos + 1);
            ftp_retr(control_fd, filename);
        } else if (cmd.find("stor") == 0) {
            size_t space_pos = cmd.find(' ', 5);
            if (space_pos == string::npos) {
                cerr << "Usage: stor <filename>" << endl;
                continue;
            }
            string filename = cmd.substr(space_pos + 1);
            ftp_stor(control_fd, filename);
        } else {
            cerr << "Unknown command: " << cmd << endl;
        }
    }

    return 0;
}