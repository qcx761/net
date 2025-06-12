#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <fstream>

using namespace std;

int connect_to_server(const string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }

    return sock;
}

pair<string, int> enter_passive_mode(int control_fd) {
    send(control_fd, "PASV\r\n", 6, 0);
    char buffer[1024];
    recv(control_fd, buffer, sizeof(buffer), 0);
    cout << "Server: " << buffer;

    int h1, h2, h3, h4, p1, p2;
    sscanf(buffer, "227 entering passive mode (%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    string ip = to_string(h1) + "." + to_string(h2) + "." + to_string(h3) + "." + to_string(h4);
    int port = p1 * 256 + p2;
    return {ip, port};
}

void ftp_list(int control_fd) {
    auto [ip, port] = enter_passive_mode(control_fd);
    int data_fd = connect_to_server(ip, port);

    send(control_fd, "LIST\r\n", 6, 0);

    char buffer[4096];
    ssize_t n;
    while ((n = recv(data_fd, buffer, sizeof(buffer), 0)) > 0) {
        cout.write(buffer, n);
    }
    if(n==0){
        cout << "Server closed the data connection." << endl;
    }
    close(data_fd);
}

void ftp_retr(int control_fd, const string& filename) {
    auto [ip, port] = enter_passive_mode(control_fd);
    int data_fd = connect_to_server(ip, port);

    string cmd = "RETR " + filename + "\r\n";
    send(control_fd, cmd.c_str(), cmd.size(), 0);

    ofstream ofs(filename, ios::binary);
    char buffer[4096];
    ssize_t n;
    while ((n = recv(data_fd, buffer, sizeof(buffer), 0)) > 0) {
        ofs.write(buffer, n);
    }
    ofs.close();
    close(data_fd);
    cout << "File " << filename << " downloaded." << endl;
}

void ftp_stor(int control_fd, const string& filename) {
    auto [ip, port] = enter_passive_mode(control_fd);
    int data_fd = connect_to_server(ip, port);

    string cmd = "STOR " + filename + "\r\n";
    send(control_fd, cmd.c_str(), cmd.size(), 0);

    ifstream ifs(filename, ios::binary);
    if (!ifs) {
        cerr << "Cannot open file: " << filename << endl;
        close(data_fd);
        return;
    }

    char buffer[4096];
    while (!ifs.eof()) {
        ifs.read(buffer, sizeof(buffer));
        send(data_fd, buffer, ifs.gcount(), 0);
    }
    ifs.close();
    close(data_fd);
    cout << "File " << filename << " uploaded." << endl;
}

int main() {
    string server_ip = "127.0.0.1";
    int control_port = 2100;

    int control_fd = connect_to_server(server_ip, control_port);
    cout << "Connected to FTP server at " << server_ip << ":" << control_port << endl;

    string cmd;
    while (true) {
        cout << "ftp> ";
        getline(cin, cmd);

        if (cmd == "LIST") {
            ftp_list(control_fd);
        } else if (cmd.substr(0, 4) == "RETR") {
            string filename = cmd.substr(5);
            ftp_retr(control_fd, filename);
        } else if (cmd.substr(0, 4) == "STOR") {
            string filename = cmd.substr(5);
            ftp_stor(control_fd, filename);
        } else if (cmd == "QUIT") {
            break;
        } else {
            cout << "Unknown command.\n";
        }
    }

    close(control_fd);
    return 0;
}
