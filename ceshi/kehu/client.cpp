#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>


int create_connection(const std::string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }
    return sockfd;
}

std::string read_line(int fd) {
    std::string line;
    char ch;
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return line;
}

void send_command(int fd, const std::string& cmd) {
    std::string full = cmd + "\r\n";
    send(fd, full.c_str(), full.size(), 0);
}

std::pair<std::string, int> parse_pasv_response(const std::string& resp) {
    size_t l = resp.find('('), r = resp.find(')');
    std::string inside = resp.substr(l + 1, r - l - 1);
    std::replace(inside.begin(), inside.end(), ',', ' ');
    std::stringstream ss(inside);
    int ip1, ip2, ip3, ip4, p1, p2;
    ss >> ip1 >> ip2 >> ip3 >> ip4 >> p1 >> p2;
    std::string ip = std::to_string(ip1) + "." + std::to_string(ip2) + "." +
                     std::to_string(ip3) + "." + std::to_string(ip4);
    int port = p1 * 256 + p2;
    return {ip, port};
}

void receive_data(int data_fd) {
    char buf[1024];
    ssize_t n;
    while ((n = read(data_fd, buf, sizeof(buf))) > 0) {
        std::cout.write(buf, n);
    }
    std::cout << std::endl;
}

void download_file(int data_fd, const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    char buf[1024];
    ssize_t n;
    while ((n = read(data_fd, buf, sizeof(buf))) > 0) {
        ofs.write(buf, n);
    }
    ofs.close();
    std::cout << "Downloaded file: " << filename << std::endl;
}

void upload_file(int data_fd, const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        std::cerr << "Can't open file to upload.\n";
        return;
    }
    char buf[1024];
    while (ifs.read(buf, sizeof(buf))) {
        send(data_fd, buf, sizeof(buf), 0);
    }
    if (ifs.gcount() > 0) {
        send(data_fd, buf, ifs.gcount(), 0);
    }
    ifs.close();
    std::cout << "Uploaded file: " << filename << std::endl;
}

int main() {
    std::string server_ip = "127.0.0.1";
    int control_port = 2100;

    int control_fd = create_connection(server_ip, control_port);
    std::cout << read_line(control_fd) << "\n"; // Welcome

    // === PASV ===
    send_command(control_fd, "PASV");
    std::string pasv_resp = read_line(control_fd);
    auto [data_ip, data_port] = parse_pasv_response(pasv_resp);

    // === LIST ===
    int data_fd = create_connection(data_ip, data_port);
    send_command(control_fd, "LIST");
    std::cout << read_line(control_fd) << "\n"; // 150 Opening...
    receive_data(data_fd);
    close(data_fd);
    std::cout << read_line(control_fd) << "\n"; // 226 Transfer complete

    // === RETR ===
    send_command(control_fd, "PASV");
    pasv_resp = read_line(control_fd);
    std::tie(data_ip, data_port) = parse_pasv_response(pasv_resp);
    data_fd = create_connection(data_ip, data_port);
    send_command(control_fd, "RETR test.txt");
    std::cout << read_line(control_fd) << "\n";
    download_file(data_fd, "downloaded_test.txt");
    close(data_fd);
    std::cout << read_line(control_fd) << "\n";

    // === STOR ===
    send_command(control_fd, "PASV");
    pasv_resp = read_line(control_fd);
    std::tie(data_ip, data_port) = parse_pasv_response(pasv_resp);
    data_fd = create_connection(data_ip, data_port);
    send_command(control_fd, "STOR upload.txt");
    std::cout << read_line(control_fd) << "\n";
    upload_file(data_fd, "upload.txt");
    close(data_fd);
    std::cout << read_line(control_fd) << "\n";

    close(control_fd);
    return 0;
}
