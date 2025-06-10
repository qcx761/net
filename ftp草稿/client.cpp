#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <fstream>  
#include <algorithm>

using namespace std;

// FTP服务器配置
const string SERVER_IP = "127.0.0.1";  // 服务器IP
const int CONTROL_PORT = 2100;        // 控制连接端口

// 辅助函数：发送命令并接收响应
string send_command(int sockfd, const string& cmd) {
    if (send(sockfd, cmd.c_str(), cmd.size(), 0) < 0) {
        cerr << "Error sending command: " << cmd << endl;
        return "";
    }

    // 接收响应
    char buffer[1024];
    string response;
    ssize_t bytes_received;
    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;

        // 如果响应以 "227" 开头（PASV模式），需要解析端口号
        if (response.find("227") == 0) {
            break;  // PASV响应可能跨多个recv调用，但通常一次足够
        }
        // 如果响应以 "226" 或 "250" 开头（操作完成），可以停止
        if (response.find("226") == 0 || response.find("250") == 0 || response.find("550") == 0 || response.find("500") == 0 || response.find("426") == 0) {
            break;
        }
    }

    if (bytes_received < 0) {
        cerr << "Error receiving response" << endl;
    }

    return response;
}

// 获取IP和端口
pair<string, int> parse_pasv_response(const string& response) {
    size_t start = response.find("(");
    size_t end = response.find(")");
    if (start == string::npos || end == string::npos) {
        cerr << "Invalid PASV response format" << endl;
        return {"", 0};
    }

    string ip_port_str = response.substr(start + 1, end - start - 1);
    vector<string> parts;
    stringstream str(ip_port_str);
    string part;
    while (getline(str, part, ',')) {
        parts.push_back(part);
    }

    if (parts.size() < 6) {
        cerr << "Invalid PASV response format" << endl;
        return {"", 0};
    }

    // 解析IP地址
    string ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];

    // 解析端口号
    int p1 = stoi(parts[4]); // 将string转化为int
    int p2 = stoi(parts[5]);
    int port = p1 * 256 + p2;

    return {ip, port};
}

// 建立数据连接
int connect_data(const string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Error creating data socket" << endl;
        return -1;
    }

    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    //server_address.sin_addr.s_addr = INADDR_ANY;
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid IP address: " << ip << endl;
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error connecting to data server" << endl;
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// FTP客户端主函数
int main() {
    // 1. 建立控制连接
    int control_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (control_sock < 0) {
        cerr << "Error creating control socket" << endl;
        return 1;
    }

    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CONTROL_PORT);
    if (inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid server IP: " << SERVER_IP << endl;
        close(control_sock);
        return 1;
    }

    if (connect(control_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error connecting to control server" << endl;
        close(control_sock);
        return 1;
    }

    // 测试LIST命令
    cout << "Sending LIST command..." << endl;

    // 每次操作前都发送 PASV 命令
    string list_response = send_command(control_sock, "PASV\r\n");
    auto [data_ip, data_port] = parse_pasv_response(list_response);
    cout << "Data connection IP: " << data_ip << ", Port: " << data_port << endl;

    int data_sock = connect_data(data_ip, data_port);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket" << endl;
        close(control_sock);
        return 1;
    }

    send_command(control_sock, "LIST\r\n");  // 发送LIST命令
    char buffer[4096];
    ssize_t bytes_received;

    while ((bytes_received = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        cout.write(buffer, bytes_received);  // 正确地输出接收到的字节
    }
    close(data_sock);




    
    // 测试RETR命令
    string filename = "test.txt";
    cout << "Sending RETR command for " << filename << "..." << endl;

    // 每次操作前都发送 PASV 命令
    list_response = send_command(control_sock, "PASV\r\n");
    tie(data_ip, data_port) = parse_pasv_response(list_response);
    data_sock = connect_data(data_ip, data_port);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket" << endl;
        close(control_sock);
        return 1;
    }

    send_command(control_sock, ("RETR " + filename + "\r\n").c_str());  // 发送RETR命令
    ofstream outfile(filename, ios::binary);
    while ((bytes_received = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        outfile.write(buffer, bytes_received);  // 写入文件
    }
    outfile.close();
    close(data_sock);

    // 测试STOR命令
    filename = "upload_test.txt";  // 替换为你要上传的文件名
    cout << "Sending STOR command for " << filename << "..." << endl;

    // 每次操作前都发送 PASV 命令
    list_response = send_command(control_sock, "PASV\r\n");
    tie(data_ip, data_port) = parse_pasv_response(list_response);
    data_sock = connect_data(data_ip, data_port);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket" << endl;
        close(control_sock);
        return 1;
    }

    send_command(control_sock, ("STOR " + filename + "\r\n").c_str());  // 发送STOR命令
    ifstream infile(filename, ios::binary);
    while (infile.read(buffer, sizeof(buffer))) {
        send(data_sock, buffer, infile.gcount(), 0);  // 发送文件数据
    }
    send(data_sock, buffer, infile.gcount(), 0);  // 发送剩余数据
    infile.close();
    close(data_sock);

    // 关闭控制连接
    send_command(control_sock, "QUIT\r\n");
    close(control_sock);

    cout << "FTP client finished." << endl;
    return 0;
}
