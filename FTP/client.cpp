#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

const string SERVER_IP = "127.0.0.1";
const int CONTROL_PORT = 2100;

string send_command(int sockfd, const string &cmd) {
    send(sockfd, cmd.c_str(), cmd.length(), 0);
    char buffer[4096];
    string response;
    ssize_t n;
    while ((n = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        response += buffer;
        if (response.find("\r\n") != string::npos)
            break;
    }
    cout << "[SERVER] " << response;
    return response;
}

pair<string, int> parse_pasv_response(const string &response) {
    size_t start = response.find("(");
    size_t end = response.find(")");
    if (start == string::npos || end == string::npos)
        return {"", 0};

    string ip_port = response.substr(start + 1, end - start - 1);
    stringstream ss(ip_port);
    string token;
    vector<int> parts;

    while (getline(ss, token, ',')) {
        parts.push_back(stoi(token));
    }

    string ip = to_string(parts[0]) + "." + to_string(parts[1]) + "." +
                to_string(parts[2]) + "." + to_string(parts[3]);
    int port = parts[4] * 256 + parts[5];

    return {ip, port};
}

int connect_data_socket(const string &ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void handle_list(int control_sock) {
    auto pasv_response = send_command(control_sock, "PASV\r\n");
    auto [ip, port] = parse_pasv_response(pasv_response);

    int data_sock = connect_data_socket(ip, port);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket\n";
        return;
    }

    send_command(control_sock, "LIST\r\n");

    char buffer[4096];
    ssize_t n;
    while ((n = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        cout.write(buffer, n);
    }

    close(data_sock);
    send_command(control_sock, ""); // 获取 226 响应
}

void handle_stor(int control_sock, const string &local_filename) {
    auto pasv_response = send_command(control_sock, "PASV\r\n");
    auto [ip, port] = parse_pasv_response(pasv_response);

    int data_sock = connect_data_socket(ip, port);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket\n";
        return;
    }

    send_command(control_sock, "STOR " + local_filename + "\r\n");

    ifstream file(local_filename, ios::binary);
    if (!file) {
        cerr << "Failed to open local file\n";
        close(data_sock);
        return;
    }

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)))
        send(data_sock, buffer, file.gcount(), 0);
    send(data_sock, buffer, file.gcount(), 0);

    file.close();
    close(data_sock);
    send_command(control_sock, ""); // 等待 226
}

void handle_retr(int control_sock, const string &remote_filename) {
    auto pasv_response = send_command(control_sock, "PASV\r\n");
    auto [ip, port] = parse_pasv_response(pasv_response);

    int data_sock = connect_data_socket(ip, port);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket\n";
        return;
    }

    send_command(control_sock, "RETR " + remote_filename + "\r\n");

    ofstream file("downloaded_" + remote_filename, ios::binary);
    if (!file) {
        cerr << "Failed to create local file\n";
        close(data_sock);
        return;
    }

    char buffer[4096];
    ssize_t n;
    while ((n = recv(data_sock, buffer, sizeof(buffer), 0)) > 0)
        file.write(buffer, n);

    file.close();
    close(data_sock);
    send_command(control_sock, ""); // 等待 226
}

int main() {
    int control_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (control_sock < 0) {
        cerr << "Failed to create control socket\n";
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CONTROL_PORT);
    inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr);

    if (connect(control_sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Failed to connect to server\n";
        close(control_sock);
        return 1;
    }

    // 读取欢迎信息
    char buffer[1024];
    recv(control_sock, buffer, sizeof(buffer), 0);
    cout << "[SERVER] " << buffer;

    // 示例操作：
    handle_list(control_sock);
    handle_stor(control_sock, "upload_test.txt");
    handle_retr(control_sock, "upload_test.txt");

    send_command(control_sock, "QUIT\r\n");
    close(control_sock);
    return 0;
}
