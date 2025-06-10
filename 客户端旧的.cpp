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
const string SERVER_IP="127.0.0.1";  // 替换为你的服务器IP
const int CONTROL_PORT=2100;        // 控制连接端口

// 辅助函数：发送命令并接收响应
string send_command(int sockfd, const string& cmd){
    if(send(sockfd,cmd.c_str(),cmd.size(),0)<0){
        cerr << "Error sending command: " << cmd << endl;
        return "";
    }

    // 接收响应
    char buffer[1024];
    string response;
    ssize_t bytes_received;
    while((bytes_received=recv(sockfd,buffer,sizeof(buffer)-1,0))>0){
        buffer[bytes_received]='\0';
        response+=buffer;

        // 如果响应以 "227" 开头（PASV模式），需要解析端口号
        if(response.find("227")==0){
            break;  // PASV响应可能跨多个recv调用，但通常一次足够
        }
        // 如果响应以 "226" 或 "250" 开头（操作完成），可以停止
        if(response.find("226")==0||response.find("250")==0||response.find("550")==0||response.find("500")==0||response.find("426")==0){
            break;
        }
    }

    if(bytes_received<0){
        cerr << "Error receiving response" << endl;
    }

    return response;
}

// 获取IP和端口
pair<string, int>parse_pasv_response(const string& response){
    size_t start=response.find("(");
    size_t end=response.find(")");
    if (start==string::npos||end==string::npos){
        cerr << "Invalid PASV response format" << endl;
        return {"", 0};
    }

    string ip_port_str=response.substr(start+1,end-start-1);
    vector<string> parts;
    stringstream str(ip_port_str);
    string part;
    while(getline(str,part,',')){
        parts.push_back(part);
    }

    if(parts.size()<6){
        cerr << "Invalid PASV response format" << endl;
        return {"", 0};
    }

    // 解析IP地址
    string ip=parts[0]+"."+parts[1]+"."+parts[2]+"."+parts[3];

    // 解析端口号
    int p1=stoi(parts[4]); // 将string转化为int
    int p2=stoi(parts[5]);
    int port=p1*256+p2;

    return {ip, port};
}

// 建立数据连接
int connect_data(const string& ip,int port){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        cerr << "Error creating data socket" << endl;
        return -1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&server_addr.sin_addr)<=0){
        cerr << "Invalid IP address: " << ip << endl;
        close(sockfd);
        return -1;
    }

    if(connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        cerr << "Error connecting to data server" << endl;
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// FTP客户端主函数
int main(){
    // 1. 建立控制连接
    int control_sock=socket(AF_INET,SOCK_STREAM,0);
    if(control_sock<0){
        cerr << "Error creating control socket" << endl;
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(CONTROL_PORT);
    if(inet_pton(AF_INET,SERVER_IP.c_str(),&server_addr.sin_addr)<=0){
        cerr << "Invalid server IP: " << SERVER_IP << endl;
        close(control_sock);
        return 1;
    }

    if(connect(control_sock,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        cerr << "Error connecting to control server" << endl;
        close(control_sock);
        return 1;
    }

    // 测试LIST命令
    cout << "Sending LIST command..." << endl;
    string list_response=send_command(control_sock,"PASV\r\n");  // 进入被动模式
    auto[data_ip,data_port]=parse_pasv_response(list_response);
    cout << "Data connection IP: " << data_ip << ", Port: " << data_port << endl;

    int data_sock=connect_data(data_ip,data_port);
    if(data_sock<0){
        cerr << "Failed to connect data socket" << endl;
        close(control_sock);
        return 1;
    }

    send_command(control_sock,"LIST\r\n");  // 发送LIST命令
    char buffer[4096];
    ssize_t bytes_received;
    while((bytes_received=recv(data_sock,buffer,sizeof(buffer),0))>0){
        cout << buffer << endl;
        // write(STDOUT_FILENO,buffer,bytes_received);  // 打印到终端
    }
    close(data_sock);

    // 测试RETR命令
    string filename="test.txt";
    cout << "Sending RETR command for " << filename << "..." << endl;
    list_response=send_command(control_sock,"PASV\r\n");  // 进入被动模式
    tie(data_ip, data_port)=parse_pasv_response(list_response);
    data_sock=connect_data(data_ip, data_port);
    if(data_sock<0){
        cerr << "Failed to connect data socket" << endl;
        close(control_sock);
        return 1;
    }

    send_command(control_sock,("RETR "+filename+"\r\n").c_str());  // 发送RETR命令
    ofstream outfile(filename,ios::binary);
    while((bytes_received=recv(data_sock,buffer,sizeof(buffer),0))>0){
        outfile.write(buffer, bytes_received);  // 写入文件
    }
    outfile.close();
    close(data_sock);

    // 测试STOR命令
    filename="upload_test.txt";  // 替换为你要上传的文件名
    cout << "Sending STOR command for " << filename << "..." << endl;
    list_response=send_command(control_sock,"PASV\r\n");  // 进入被动模式
    tie(data_ip,data_port)=parse_pasv_response(list_response);
    data_sock=connect_data(data_ip, data_port);
    if(data_sock<0){
        cerr << "Failed to connect data socket" << endl;
        close(control_sock);
        return 1;
    }

    send_command(control_sock,("STOR "+filename+"\r\n").c_str());  // 发送STOR命令
    ifstream infile(filename,ios::binary);
    while(infile.read(buffer,sizeof(buffer))){
        send(data_sock,buffer,infile.gcount(),0);  // 发送文件数据
    }
    send(data_sock,buffer,infile.gcount(),0);  // 发送剩余数据
    infile.close();
    close(data_sock);

    // 关闭控制连接
    send_command(control_sock,"QUIT\r\n");
    close(control_sock);

    cout << "FTP client finished." << endl;
    return 0;
}



































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
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>

using namespace std;

const string SERVER_IP="127.0.0.1";
const int CONTROL_PORT=2100;

mutex mtx;
condition_variable cv;
bool pasv_ready=false;
pair<string,int> data_conn_info;
bool data_conn_established=false;

string send_command(int sockfd,const string& cmd){
    if(send(sockfd,cmd.c_str(),cmd.size(),0)<0){
        cerr<<"Error sending command: "<<cmd<<endl;
        return "";
    }
    char buffer[1024];
    string response;
    ssize_t bytes_received;
    while((bytes_received=recv(sockfd,buffer,sizeof(buffer)-1,0))>0){
        buffer[bytes_received]='\0';
        response+=buffer;
        if(response.find("227")==0||response.find("226")==0||response.find("250")==0||response.find("550")==0||response.find("500")==0||response.find("426")==0)
            break;
    }
    if(bytes_received<0)cerr<<"Error receiving response"<<endl;
    return response;
}

pair<string,int>parse_pasv_response(const string& response){
    size_t start=response.find("(");
    size_t end=response.find(")");
    if(start==string::npos||end==string::npos){
        cerr<<"Invalid PASV response format"<<endl;
        return {"",0};
    }
    string ip_port_str=response.substr(start+1,end-start-1);
    vector<string> parts;
    stringstream str(ip_port_str);
    string part;
    while(getline(str,part,','))parts.push_back(part);
    if(parts.size()<6){
        cerr<<"Invalid PASV response format"<<endl;
        return {"",0};
    }
    string ip=parts[0]+"."+parts[1]+"."+parts[2]+"."+parts[3];
    int p1=stoi(parts[4]);
    int p2=stoi(parts[5]);
    return {ip,p1 * 256+p2};
}

int connect_data(const string& ip,int port){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        cerr<<"Error creating data socket"<<endl;
        return -1;
    }
    struct sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&server_addr.sin_addr)<=0){
        cerr<<"Invalid IP address: "<<ip<<endl;close(sockfd);
        return -1;
    }
    if(connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        cerr<<"Error connecting to data server"<<endl;
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void control_connection_thread(int control_sock){
    // 进入被动模式
    string pasv_response=send_command(control_sock,"PASV\r\n");
    auto [data_ip,data_port]=parse_pasv_response(pasv_response);
    cout<<"Data connection IP: "<<data_ip<<", Port: "<<data_port<<endl;
    
    // 通知数据连接线程
    {
        lock_guard<mutex> lock(mtx);
        data_conn_info={data_ip,data_port};
        pasv_ready=true;
    }
    cv.notify_one();
    
    // 等待数据连接建立
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock,[](){return data_conn_established;});
    }
    
    // 发送LIST命令
    send_command(control_sock,"LIST\r\n");
    
    // 发送STOR命令（示例：上传文件）
    string filename="upload_test.txt"; // 上传的文件名
    send_command(control_sock,("STOR "+filename+"\r\n").c_str());
    
    // 发送RETR命令（示例：下载文件）
    string retr_filename="test.txt"; // 下载的文件名
    send_command(control_sock,("RETR "+retr_filename+"\r\n").c_str());
    
    // 关闭控制连接
    send_command(control_sock,"QUIT\r\n");
    close(control_sock);
}

void data_connection_thread(){
    // 等待PASV响应
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock,[](){return pasv_ready;});
    }
    
    // 建立数据连接
    int data_sock=connect_data(data_conn_info.first,data_conn_info.second);
    if(data_sock<0){cerr<<"Failed to connect data socket"<<endl;return;}
    
    // 通知控制连接线程数据连接已建立
    {
        lock_guard<mutex> lock(mtx);
        data_conn_established=true;
    }
    cv.notify_one();
    
    // 接收数据（LIST响应）
    char buffer[4096];
    ssize_t bytes_received;
    while((bytes_received=recv(data_sock,buffer,sizeof(buffer),0))>0){
        cout<<buffer;
    }
    if(bytes_received<0)
        cerr<<"Error receiving data"<<endl;
    
    // 处理STOR命令（上传文件）
    ifstream infile("upload_test.txt"); // 上传的文件名
    if(infile.is_open()){
        while(infile.read(buffer,sizeof(buffer))){
            send(data_sock,buffer,infile.gcount(),0);
        }
        send(data_sock,buffer,infile.gcount(),0); // 发送剩余数据
        infile.close();
    }else{
        cerr<<"Failed to open file for upload"<<endl;
    }
    
    // 处理RETR命令（下载文件）
    ofstream outfile("test.txt"); // 下载的文件名
    if(outfile.is_open()){
        while((bytes_received=recv(data_sock,buffer,sizeof(buffer),0))>0){
            outfile.write(buffer,bytes_received);
        }
        if(bytes_received<0)
            cerr<<"Error receiving data"<<endl;
        outfile.close();
    }else{
        cerr<<"Failed to open file for download"<<endl;
    }
    
    close(data_sock);
}

int main(){
    int control_sock=socket(AF_INET,SOCK_STREAM,0);
    if(control_sock<0){
        cerr<<"Error creating control socket"<<endl;
        return 1;
    }
    struct sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(CONTROL_PORT);
    if(inet_pton(AF_INET,SERVER_IP.c_str(),&server_addr.sin_addr)<=0){
        cerr<<"Invalid server IP: "<<SERVER_IP<<endl;close(control_sock);
        return 1;
    }
    if(connect(control_sock,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        cerr<<"Error connecting to control server"<<endl;close(control_sock);
        return 1;
    }
    thread control_thread(control_connection_thread,control_sock);
    thread data_thread(data_connection_thread);
    control_thread.join();
    data_thread.join();
    cout<<"FTP client finished."<<endl;
    return 0;
}










































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
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

const string SERVER_IP = "127.0.0.1";
const int CONTROL_PORT = 2100;

mutex mtx;
condition_variable cv;
bool pasv_ready = false;
pair<string, int> data_conn_info;
bool data_conn_established = false;
bool command_done = false;

string send_command(int sockfd, const string& cmd) {
    if (send(sockfd, cmd.c_str(), cmd.size(), 0) < 0) {
        cerr << "Error sending command: " << cmd << endl;
        return "";
    }
    char buffer[1024];
    string response;
    ssize_t bytes_received;
    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;
        if (response.find("227") == 0 || response.find("226") == 0 || 
            response.find("250") == 0 || response.find("550") == 0 || 
            response.find("500") == 0 || response.find("426") == 0) {
            break;
        }
    }
    if (bytes_received < 0) cerr << "Error receiving response" << endl;
    return response;
}

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
    while (getline(str, part, ',')) parts.push_back(part);
    if (parts.size() < 6) {
        cerr << "Invalid PASV response format" << endl;
        return {"", 0};
    }
    string ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
    int p1 = stoi(parts[4]);
    int p2 = stoi(parts[5]);
    return {ip, p1 * 256 + p2};
}

int connect_data(const string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Error creating data socket" << endl;
        return -1;
    }
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
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

void control_connection_thread(int control_sock) {
    string pasv_response = send_command(control_sock, "PASV\r\n");
    auto[data_ip, data_port] = parse_pasv_response(pasv_response);
    cout << "Data connection IP: " << data_ip << ", Port: " << data_port << endl;

    {
        lock_guard<mutex> lock(mtx);
        data_conn_info = {data_ip, data_port};
        pasv_ready = true;
    }
    cv.notify_one();

    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return data_conn_established; });
    }

    // 发送 LIST 命令
    {
        lock_guard<mutex> lock(mtx);
        send_command(control_sock, "LIST\r\n");
        command_done = true;
    }
    cv.notify_one();

    // 等待 LIST 响应完成
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return command_done; });
        command_done = false; // 重置状态
    }

    // 发送 STOR 命令（上传文件）
    string filename = "upload_test.txt";
    {
        lock_guard<mutex> lock(mtx);
        send_command(control_sock, ("STOR " + filename + "\r\n"));
        command_done = true;
    }
    cv.notify_one();

    // 等待 STOR 响应完成
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return command_done; });
        command_done = false; // 重置状态
    }

    // 发送 RETR 命令（下载文件）
    string retr_filename = "test.txt";
    {
        lock_guard<mutex> lock(mtx);
        send_command(control_sock, ("RETR " + retr_filename + "\r\n"));
        command_done = true;
    }
    cv.notify_one();

    // 等待 RETR 响应完成
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return command_done; });
    }

    // 关闭控制连接
    send_command(control_sock, "QUIT\r\n");
    close(control_sock);
}

void data_connection_thread() {
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return pasv_ready; });
    }

    int data_sock = connect_data(data_conn_info.first, data_conn_info.second);
    if (data_sock < 0) {
        cerr << "Failed to connect data socket" << endl;
        return;
    }

    {
        lock_guard<mutex> lock(mtx);
        data_conn_established = true;
    }
    cv.notify_one();

    // 接收数据（LIST 响应）
    char buffer[4096];
    ssize_t bytes_received;
    
    // 等待 LIST 响应
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return command_done; });
        while ((bytes_received = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
            cout.write(buffer, bytes_received);
        }
        if (bytes_received < 0) cerr << "Error receiving data" << endl;
        command_done = false; // 重置状态
    }

    // 处理 STOR 命令（上传文件）
    ifstream infile("upload_test.txt", ios::binary);
    if (infile.is_open()) {
        while (infile.read(buffer, sizeof(buffer))) {
            send(data_sock, buffer, infile.gcount(), 0);
        }
        send(data_sock, buffer, infile.gcount(), 0);
        infile.close();
    } else {
        cerr << "Failed to open file for upload" << endl;
    }

    // 处理 RETR 命令（下载文件）
    ofstream outfile("test.txt", ios::binary);
    if (outfile.is_open()) {
        while ((bytes_received = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
            outfile.write(buffer, bytes_received);
        }
        if (bytes_received < 0) cerr << "Error receiving data during download" << endl;
        outfile.close();
    } else {
        cerr << "Failed to open file for download" << endl;
    }

    close(data_sock);
}

int main() {
    int control_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (control_sock < 0) {
        cerr << "Error creating control socket" << endl;
        return 1;
    }
    struct sockaddr_in server_addr{};
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

    thread control_thread(control_connection_thread, control_sock);
    thread data_thread(data_connection_thread);
    control_thread.join();
    data_thread.join();

    cout << "FTP client finished." << endl;
    return 0;
}









































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
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>

using namespace std;

const string SERVER_IP="127.0.0.1";
const int CONTROL_PORT=2100;

mutex mtx;
condition_variable cv;
bool pasv_ready=false;
pair<string,int> data_conn_info;
bool data_conn_established=false;

string send_command(int sockfd,const string& cmd){
    if(send(sockfd,cmd.c_str(),cmd.size(),0)<0){
        cerr<<"Error sending command: "<<cmd<<endl;
        return "";
    }
    char buffer[1024];
    string response;
    ssize_t bytes_received;
    while((bytes_received=recv(sockfd,buffer,sizeof(buffer)-1,0))>0){
        buffer[bytes_received]='\0';
        response+=buffer;
        if(response.find("227")==0||response.find("226")==0||response.find("250")==0||response.find("550")==0||response.find("500")==0||response.find("426")==0)
            break;
    }
    if(bytes_received<0)cerr<<"Error receiving response"<<endl;
    return response;
}

pair<string,int>parse_pasv_response(const string& response){
    size_t start=response.find("(");
    size_t end=response.find(")");
    if(start==string::npos||end==string::npos){
        cerr<<"Invalid PASV response format"<<endl;
        return {"",0};
    }
    string ip_port_str=response.substr(start+1,end-start-1);
    vector<string> parts;
    stringstream str(ip_port_str);
    string part;
    while(getline(str,part,','))parts.push_back(part);
    if(parts.size()<6){
        cerr<<"Invalid PASV response format"<<endl;
        return {"",0};
    }
    string ip=parts[0]+"."+parts[1]+"."+parts[2]+"."+parts[3];
    int p1=stoi(parts[4]);
    int p2=stoi(parts[5]);
    return {ip,p1 * 256+p2};
}

int connect_data(const string& ip,int port){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        cerr<<"Error creating data socket"<<endl;
        return -1;
    }
    struct sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&server_addr.sin_addr)<=0){
        cerr<<"Invalid IP address: "<<ip<<endl;close(sockfd);
        return -1;
    }
    if(connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        cerr<<"Error connecting to data server"<<endl;
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void control_connection_thread(int control_sock){
    // 进入被动模式
    string pasv_response=send_command(control_sock,"PASV\r\n");
    auto [data_ip,data_port]=parse_pasv_response(pasv_response);
    cout<<"Data connection IP: "<<data_ip<<", Port: "<<data_port<<endl;
    
    // 通知数据连接线程
    {
        lock_guard<mutex> lock(mtx);
        data_conn_info={data_ip,data_port};
        pasv_ready=true;
    }
    cv.notify_one();
    
    // 等待数据连接建立
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock,[](){return data_conn_established;});
    }
    
    // 发送LIST命令
    send_command(control_sock,"LIST\r\n");
    
    // 发送STOR命令（示例：上传文件）
    string filename="upload_test.txt"; // 上传的文件名
    send_command(control_sock,("STOR "+filename+"\r\n").c_str());
    
    // 发送RETR命令（示例：下载文件）
    string retr_filename="test.txt"; // 下载的文件名
    send_command(control_sock,("RETR "+retr_filename+"\r\n").c_str());
    
    // 关闭控制连接
    send_command(control_sock,"QUIT\r\n");
    close(control_sock);
}

void data_connection_thread(){
    // 等待PASV响应
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock,[](){return pasv_ready;});
    }
    
    // 建立数据连接
    int data_sock=connect_data(data_conn_info.first,data_conn_info.second);
    if(data_sock<0){cerr<<"Failed to connect data socket"<<endl;return;}
    
    // 通知控制连接线程数据连接已建立
    {
        lock_guard<mutex> lock(mtx);
        data_conn_established=true;
    }
    cv.notify_one();
    
    // 接收数据（LIST响应）

    char buffer[4096];
    ssize_t bytes_received;
    while((bytes_received=recv(data_sock,buffer,sizeof(buffer),0))>0){
        cout<<buffer<<endl;
    }
    if(bytes_received<0)
        cerr<<"Error receiving data"<<endl;
    
    // 处理STOR命令（上传文件）
    ifstream infile("upload_test.txt"); // 上传的文件名
    if(infile.is_open()){
        while(infile.read(buffer,sizeof(buffer))){
            send(data_sock,buffer,infile.gcount(),0);
        }
        send(data_sock,buffer,infile.gcount(),0); // 发送剩余数据
        infile.close();
    }else{
        cerr<<"Failed to open file for upload"<<endl;
    }
    
    // 处理RETR命令（下载文件）
    ofstream outfile("test.txt"); // 下载的文件名
    if(outfile.is_open()){
        while((bytes_received=recv(data_sock,buffer,sizeof(buffer),0))>0){
            outfile.write(buffer,bytes_received);
        }
        if(bytes_received<0)
            cerr<<"Error receiving data"<<endl;
        outfile.close();
    }else{
        cerr<<"Failed to open file for download"<<endl;
    }
    
    close(data_sock);
}

int main(){
    int control_sock=socket(AF_INET,SOCK_STREAM,0);
    if(control_sock<0){
        cerr<<"Error creating control socket"<<endl;
        return 1;
    }
    struct sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(CONTROL_PORT);
    if(inet_pton(AF_INET,SERVER_IP.c_str(),&server_addr.sin_addr)<=0){
        cerr<<"Invalid server IP: "<<SERVER_IP<<endl;close(control_sock);
        return 1;
    }
    if(connect(control_sock,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        cerr<<"Error connecting to control server"<<endl;close(control_sock);
        return 1;
    }
    thread control_thread(control_connection_thread,control_sock);
    thread data_thread(data_connection_thread);
    control_thread.join();
    data_thread.join();
    cout<<"FTP client finished."<<endl;
    return 0;
}















































