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

const string SERVER_IP="127.0.0.1";
const int CONTROL_PORT=2100;

class FTPClient {
public:
    FTPClient(const string& server_ip,int control_port):server_ip(server_ip),control_port(control_port),control_sock(-1){}

    bool connectToServer() {
        control_sock=socket(AF_INET,SOCK_STREAM,0);
        if(control_sock<0) {
            cerr << "Failed to create control socket\n";
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family=AF_INET;
        server_addr.sin_port=htons(control_port);
        inet_pton(AF_INET,server_ip.c_str(),&server_addr.sin_addr);

        if(connect(control_sock,(sockaddr *)&server_addr,sizeof(server_addr))<0) {
            cerr << "Failed to connect to server\n";
            close(control_sock);
            return false;
        }

        // Read welcome message
        char buffer[1024];
        recv(control_sock,buffer,sizeof(buffer),0);
        cout << "[SERVER] " << buffer;
        return true;
    }

    void closeConnection() {
        if(control_sock!=-1) {
            sendCommand("QUIT\r\n");
            close(control_sock);
        }
    }

    string sendCommand(const string& cmd) {
        send(control_sock,cmd.c_str(),cmd.length(),0);
        char buffer[4096];
        string response;
        ssize_t n;
        while((n=recv(control_sock,buffer,sizeof(buffer)-1,0))>0) {
            buffer[n]='\0';
            response+=buffer;
            if(response.find("\r\n")!=string::npos)
                break;
        }
        cout << "[SERVER] " << response;
        return response;
    }

    pair<string,int> parsePasvResponse(const string& response) {
        size_t start=response.find("(");
        size_t end=response.find(")");
        if(start==string::npos || end==string::npos)
            return {"",0};

        string ip_port=response.substr(start+1,end-start-1);
        stringstream ss(ip_port);
        string token;
        vector<int> parts;

        while(getline(ss,token,',')) {
            parts.push_back(stoi(token));
        }

        string ip=to_string(parts[0])+"."+to_string(parts[1])+"."+
                  to_string(parts[2])+"."+to_string(parts[3]);
        int port=parts[4]*256+parts[5];

        return {ip,port};
    }

    int connectDataSocket(const string& ip,int port) {
        int sockfd=socket(AF_INET,SOCK_STREAM,0);
        if(sockfd<0) return -1;

        sockaddr_in addr{};
        addr.sin_family=AF_INET;
        addr.sin_port=htons(port);
        inet_pton(AF_INET,ip.c_str(),&addr.sin_addr);

        if(connect(sockfd,(sockaddr *)&addr,sizeof(addr))<0) {
            close(sockfd);
            return -1;
        }
        return sockfd;
    }

    vector<string> handleList() {
        auto pasvResponse=sendCommand("PASV\r\n");
        auto[ip,port]=parsePasvResponse(pasvResponse);

        int dataSock=connectDataSocket(ip,port);
        if(dataSock<0) {
            cerr << "Failed to connect data socket\n";
            return {};
        }

        sendCommand("LIST\r\n");

        // 读取文件列表
        char buffer[4096];
        ssize_t n;
        stringstream file_list;
        while((n=recv(dataSock,buffer,sizeof(buffer),0))>0) {
            file_list.write(buffer,n);
        }

        close(dataSock);
        sendCommand(""); // 等待 226

        // 解析文件列表
        vector<string> files;
        string line;
        while(getline(file_list,line)) {
            files.push_back(line);
            cout << line << endl;  // 打印每个文件名
        }

        return files;
    }

    void handleRetr(const string& remoteFilename) {
        auto pasvResponse=sendCommand("PASV\r\n");
        auto[ip,port]=parsePasvResponse(pasvResponse);

        int dataSock=connectDataSocket(ip,port);
        if(dataSock<0) {
            cerr << "Failed to connect data socket\n";
            return;
        }

        sendCommand("RETR "+remoteFilename+"\r\n");

        ofstream file("downloaded_"+remoteFilename,ios::binary);
        if(!file) {
            cerr << "Failed to create local file\n";
            close(dataSock);
            return;
        }

        char buffer[4096];
        ssize_t n;
        while((n=recv(dataSock,buffer,sizeof(buffer),0))>0)
            file.write(buffer,n);

        file.close();
        close(dataSock);
        sendCommand(""); // 等待 226
    }

    // 让用户选择一个文件进行下载
    void chooseFileToDownload(const vector<string>& files) {
        if(files.empty()) {
            cout << "No files available to download.\n";
            return;
        }

        cout << "Enter the number of the file you want to download (1 to " << files.size() << "): ";
        int choice;
        cin >> choice;

        if(choice<1 || choice>files.size()) {
            cout << "Invalid choice.\n";
            return;
        }

        string filename=files[choice-1];
        cout << "Downloading: " << filename << endl;
        handleRetr(filename);
    }

private:
    string server_ip;
    int control_port;
    int control_sock;
};

int main() {
    FTPClient client(SERVER_IP,CONTROL_PORT);
    if(!client.connectToServer()) {
        cerr << "Failed to connect to the FTP server\n";
        return 1;
    }

    // 获取文件列表
    vector<string> files=client.handleList();
    // 让用户选择文件下载
    client.chooseFileToDownload(files);

    client.closeConnection();
    return 0;
}
