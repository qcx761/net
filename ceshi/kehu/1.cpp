#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <cstdio>

using namespace std;

#define CONTROL_PORT 2100
#define DATA_PORT_MIN 1024
#define DATA_PORT_MAX 49151
#define INET_ADDRSTRLEN 16

class FTPClient {
public:
    FTPClient(const string& server_ip) : server_ip(server_ip), control_fd(-1), data_fd(-1), is_connected(false) {}

    ~FTPClient() {
        if (control_fd != -1) close(control_fd);
        if (data_fd != -1) close(data_fd);
    }

    bool connect_to_server() {
        control_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (control_fd < 0) {
            perror("socket failed");
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(CONTROL_PORT);
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            perror("inet_pton failed");
            return false;
        }

        if (connect(control_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect failed");
            return false;
        }

        is_connected = true;
        cout << "Connected to server: " << server_ip << ":" << CONTROL_PORT << endl;
        return true;
    }

    void close_connection() {
        if (control_fd != -1) {
            close(control_fd);
            control_fd = -1;
        }
        if (data_fd != -1) {
            close(data_fd);
            data_fd = -1;
        }
        is_connected = false;
    }

    void send_command(const string& cmd) {
        if (is_connected) {
            send(control_fd, cmd.c_str(), cmd.size(), 0);
        }
    }

    string receive_response() {
        char buf[1024];
        ssize_t len = recv(control_fd, buf, sizeof(buf) - 1, 0);
        if (len <= 0) {
            perror("recv failed");
            return "";
        }
        buf[len] = '\0';
        return string(buf);
    }

    void pasv_mode() {
        send_command("PASV\n");
        string response = receive_response();
        cout << "PASV Response: " << response << endl;

        if (response.find("200 OK") != string::npos) {
            // Parse the PASV response to get the data port
            size_t start = response.find('(') + 1;
            size_t end = response.find(')', start);
            string pasv_info = response.substr(start, end - start);

            // Extract the port number
            size_t first_comma = pasv_info.find(',');
            size_t second_comma = pasv_info.find(',', first_comma + 1);
            size_t third_comma = pasv_info.find(',', second_comma + 1);
            int p1 = stoi(pasv_info.substr(second_comma + 1, third_comma - second_comma - 1));
            int p2 = stoi(pasv_info.substr(third_comma + 1));
            int data_port = p1 * 256 + p2;

            // Connect to the data port
            connect_data_socket(data_port);
        }
    }

    void list_files() {
        send_command("LIST\n");
        string response = receive_response();
        cout << "LIST Response: " << response << endl;

        if (response.find("200 OK") != string::npos) {
            // Receive the file list through the data connection
            char buf[1024];
            ssize_t len = recv(data_fd, buf, sizeof(buf) - 1, 0);
            if (len > 0) {
                buf[len] = '\0';
                cout << "File List:\n" << buf << endl;
            }
        }
    }

    void upload_file(const string& filename) {
        send_command("STOR " + filename + "\n");
        string response = receive_response();
        cout << "STOR Response: " << response << endl;

        if (response.find("200 OK") != string::npos) {
            // Open the file and send its content through the data connection
            int file_fd = open(filename.c_str(), O_RDONLY);
            if (file_fd < 0) {
                perror("open file failed");
                return;
            }

            char buf[4096];
            ssize_t len;
            while ((len = read(file_fd, buf, sizeof(buf))) > 0) {
                send(data_fd, buf, len, 0);
            }
            close(file_fd);
        }
    }

    void download_file(const string& filename) {
        send_command("RETR " + filename + "\n");
        string response = receive_response();
        cout << "RETR Response: " << response << endl;

        if (response.find("200 OK") != string::npos) {
            // Receive the file content through the data connection
            int file_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file_fd < 0) {
                perror("create file failed");
                return;
            }

            char buf[4096];
            ssize_t len;
            while ((len = recv(data_fd, buf, sizeof(buf), 0)) > 0) {
                write(file_fd, buf, len);
            }
            close(file_fd);
        }
    }

private:
    string server_ip;
    int control_fd;
    int data_fd;
    bool is_connected;

    void connect_data_socket(int port) {
        data_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (data_fd < 0) {
            perror("socket failed");
            return;
        }

        sockaddr_in data_addr{};
        data_addr.sin_family = AF_INET;
        data_addr.sin_port = htons(port);
        data_addr.sin_addr.s_addr = INADDR_ANY;

        if (connect(data_fd, (sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
            perror("connect data socket failed");
            close(data_fd);
            data_fd = -1;
        } else {
            cout << "Connected to data port " << port << endl;
        }
    }
};

int main() {
    string server_ip;
    cout << "Enter the server IP address: ";
    cin >> server_ip;

    FTPClient client(server_ip);
    if (!client.connect_to_server()) {
        cerr << "Failed to connect to server!" << endl;
        return 1;
    }

    // Request PASV mode (passive mode)
    client.pasv_mode();

    // List files in the current directory
    client.list_files();

    // Upload a file (change the filename to one you want to upload)
    string upload_filename = "test_upload.txt";
    client.upload_file(upload_filename);

    // Download a file (change the filename to one you want to download)
    string download_filename = "test_download.txt";
    client.download_file(download_filename);

    client.close_connection();

    return 0;
}
