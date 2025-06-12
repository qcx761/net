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
#include <sys/sendfile.h>
#include <condition_variable>
#include <ifaddrs.h>
#include <linux/if.h>
#include <fcntl.h>
#include <future>
#include "threadpool.hpp"

using namespace std;

#define CONTROL_PORT 2100
#define DATA_PORT_MIN 1024
#define DATA_PORT_MAX 49151
#define MAX_EVENTS 1024
#define INET_ADDRSTRLEN 16

class ControlConnect {
public:
    int control_fd;
    int command_type;
    string filename;

    ControlConnect(int fd, int cmd, const string& fname = "") : control_fd(fd), command_type(cmd), filename(fname) {}
};

class ConnectionGroup {
public:
    mutex mtx;
    vector<ControlConnect> connections;
    unordered_map<int, int> data_to_control;
    unordered_map<int, int> listen_to_control;  // 新增：监听套接字到控制连接的映射

    void unbind_control_from_data(int data_fd) {
        lock_guard<mutex> lock(mtx);  // 加锁保证线程安全
        data_to_control.erase(data_fd);  // 直接删除该条目
    }

    void bind_data_to_control(int data_fd, int control_fd) {
        lock_guard<mutex> lock(mtx);
        data_to_control[data_fd] = control_fd;
    }

    int get_control_from_data(int data_fd) {
        lock_guard<mutex> lock(mtx);
        auto it = data_to_control.find(data_fd);
        return (it == data_to_control.end()) ? -1 : it->second;
    }

    void add_or_update(int fd, int cmd, const string& fname = "") {
        lock_guard<mutex> lock(mtx);
        auto it = find_if(connections.begin(), connections.end(), [fd](const ControlConnect& connect) { return connect.control_fd == fd; });
        if (it == connections.end()) {
            connections.emplace_back(fd, cmd, fname);
        } else {
            it->command_type = cmd;
            if (!fname.empty()) {
                it->filename = fname;
            }
        }
    }

    void remove(int fd) {
        lock_guard<mutex> lock(mtx);
        connections.erase(remove_if(connections.begin(), connections.end(), [fd](const ControlConnect& connect) { return connect.control_fd == fd; }), connections.end());
        // 同时从data_to_control中移除
        data_to_control.erase(fd);
    }

    int get_command_type(int fd) {
        lock_guard<mutex> lock(mtx);
        auto it = find_if(connections.begin(), connections.end(), [fd](const ControlConnect& connect) { return connect.control_fd == fd; });
        return (it == connections.end()) ? 0 : it->command_type;
    }

    string get_filename(int fd) {
        lock_guard<mutex> lock(mtx);
        auto it = find_if(connections.begin(), connections.end(), [fd](const ControlConnect& connect) { return connect.control_fd == fd; });
        return (it == connections.end()) ? "" : it->filename;
    }

    // 新增：添加监听套接字到控制连接的映射
    void add_listen_socket(int listen_fd, int control_fd) {
        lock_guard<mutex> lock(mtx);
        listen_to_control[listen_fd] = control_fd;
    }

    // 新增：通过监听套接字获取控制连接
    int get_control_from_listen(int listen_fd) {
        lock_guard<mutex> lock(mtx);
        auto it = listen_to_control.find(listen_fd);
        return (it == listen_to_control.end()) ? -1 : it->second;
    }

    void unbind_control_from_listen(int listen_fd) {
        lock_guard<mutex> lock(mtx);  // 加锁保证线程安全
        listen_to_control.erase(listen_fd);  // 直接删除该条目
    }

    void remove_listen_socket(int listen_fd) {
        lock_guard<mutex> lock(mtx);
        listen_to_control.erase(listen_fd);
    }
};

class FTPServer {
public:
    FTPServer(int control_port = CONTROL_PORT, size_t thread_count = 4) : control_port(control_port), thread_pool(thread_count), epfd(-1), server_fd(-1), is_running(false) {}

    bool init() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            perror("socket failed");
            return false;
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        socklen_t addr_len = sizeof(addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(control_port);

        if (bind(server_fd, (sockaddr*)&addr, addr_len) < 0) {
            perror("bind failed");
            close(server_fd);
            return false;
        }

        if (listen(server_fd, 10) < 0) {
            perror("listen failed");
            close(server_fd);
            return false;
        }

        epfd = epoll_create1(0);
        if (epfd == -1) {
            perror("epoll_create1 failed");
            close(server_fd);
            return false;
        }

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = server_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
            perror("epoll_ctl ADD server_fd failed");
            close(server_fd);
            close(epfd);
            return false;
        }

        is_running = true;
        return true;
    }

    void run() {
        epoll_event events[MAX_EVENTS];
        while (is_running) {
            int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
            if (n == -1) {
                if (errno == EINTR) {
                    continue;
                }
                perror("epoll_wait");
                break;
            }
            for (int i = 0; i < n; i++) {
                int fd = events[i].data.fd;
                uint32_t evs = events[i].events;

                if ((evs & EPOLLERR) || (evs & EPOLLHUP) || (evs & EPOLLRDHUP)) {
                    close_connection(fd);
                    continue;
                }

                if (fd == server_fd) { // 客户端连接
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            perror("accept");
                            break;
                        }
                    }

                    set_nonblocking(client_fd);
                    epoll_event client_ev{};
                    client_ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
                    client_ev.data.fd = client_fd;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
                        perror("epoll_ctl ADD client_fd failed");
                        close(client_fd);
                    }

                    cout << "New control connection accepted: fd=" << client_fd << endl;
                } else {
                    int port = get_socket_local_port(fd);

                    if (port == CONTROL_PORT) { // 控制连接
                        cout << "控制连接进入" << endl;
                        handle_control_fd(fd);
                    } else { // 数据连接
                        cout << "数据连接进入" << endl;

                        // 从数据连接获取控制连接fd
                        int control_fd = group.get_control_from_data(fd); // 能不能获得控制fd，不能就是数据监听fd
                        if (control_fd == -1) {
                            // 可能是PASV模式的监听套接字
                            control_fd = group.get_control_from_listen(fd); // 数据监听fd获取控制fd
                            if (control_fd != -1) {
                                // 有新的数据连接到来
                                sockaddr_in client_addr{};
                                socklen_t client_len = sizeof(client_addr);
                                int data_fd = accept(fd, (sockaddr*)&client_addr, &client_len);
                                if (data_fd == -1) {
                                    perror("accept data connection failed");
                                    continue;
                                }

                                set_nonblocking(data_fd);
                                epoll_event data_ev{};
                                data_ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR;
                                data_ev.data.fd = data_fd;

                                if (epoll_ctl(epfd, EPOLL_CTL_ADD, data_fd, &data_ev) == -1) {
                                    perror("epoll_ctl ADD data_fd failed");
                                    close(data_fd);
                                    continue;
                                }

                                // 关联数据连接和控制连接
                                group.bind_data_to_control(data_fd, control_fd);
                                cout << "New data connection accepted for control fd: " << control_fd << ", data fd: " << data_fd << endl;

                                // 不要在这里关闭监听套接字，因为数据传输可能还没有开始
                                // group.unbind_control_from_listen(fd);
                                // close(fd);

                                // 我们需要保留监听套接字，因为数据传输可能还没有开始
                                // 监听套接字的清理应该在数据传输完成后进行
                            } else {
                                cerr << "No control connection associated with data fd: " << fd << endl;
                                close_connection(fd);
                            }
                        } else {
                            // 正常的数据连接处理
                            cout << "处理数据连接中...." << endl;
                            int command = group.get_command_type(control_fd);
                            string filename = group.get_filename(control_fd);

                            thread_pool.enqueue([this, fd, command, filename]() {
                                handle_data_connection(fd, command, filename);
                            });

                            // 删除控制连接和数据链接的映射
                            group.unbind_control_from_data(fd);

                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr); // 结束数据连接
                        }
                    }
                }
            }
        }
    }

    void stop() {
        is_running = false;
        if (server_fd != -1) close(server_fd);
        if (epfd != -1) close(epfd);
    }

    ~FTPServer() {}

private:
    int control_port;
    int epfd;
    int server_fd;
    bool is_running;
    threadpool thread_pool;
    ConnectionGroup group;

    bool is_continue = false;

    // 设置非阻塞
    static void set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    // 通过套接字获取端口号
    static int get_socket_local_port(int fd) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);

        if (getsockname(fd, (sockaddr*)&addr, &len) == -1) {
            perror("getsockname");
            return -1;
        }

        return ntohs(addr.sin_port);
    }

    // 关闭套接字并注销epoll
    void close_connection(int fd) {
        cout << "Closing connection fd=" << fd << endl;
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        group.remove(fd);
    }

    // 控制连接获取命令
    void handle_control_fd(int fd) {
        char buf[1024];
        ssize_t len = recv(fd, buf, sizeof(buf) - 1, 0);
        if (len <= 0) {
            if (len == 0) {
                cout << "Client disconnected: fd=" << fd << endl;
            } else {
                perror("recv");
            }
            close_connection(fd);
            return;
        }
        buf[len] = '\0';
        cout << "Control message from fd=" << fd << ": " << buf;
        parse_and_handle_command(fd, string(buf));
    }

    // 解析并处理控制连接命令
    void parse_and_handle_command(int fd, const string& cmd_line) {
        string cmd, arg;
        size_t pos = cmd_line.find(' ');

        if (pos == string::npos) { // 没找到空格
            cmd = cmd_line;
            cmd.erase(remove(cmd.begin(), cmd.end(), '\r'), cmd.end());
            cmd.erase(remove(cmd.begin(), cmd.end(), '\n'), cmd.end());
            arg = "";
        } else {
            cmd = cmd_line.substr(0, pos);
            arg = cmd_line.substr(pos + 1);
            // 删除\r与\n
            arg.erase(remove(arg.begin(), arg.end(), '\r'), arg.end());
            arg.erase(remove(arg.begin(), arg.end(), '\n'), arg.end());
            
        }

        // 将命令转化为大写字母
        transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        if (cmd == "PASV") {
            handle_pasv(fd);
            group.add_or_update(fd, 1);
            string response = "200 OK\n";
            send(fd, response.c_str(), response.size(), 0);
        } else if (cmd == "LIST") {
            group.add_or_update(fd, 2);
            string response = "200 OK\n";
            send(fd, response.c_str(), response.size(), 0);
        } else if (cmd == "RETR") {
            group.add_or_update(fd, 3, arg);
            string response = "200 OK\n";
            send(fd, response.c_str(), response.size(), 0);
        } else if (cmd == "STOR") {
            group.add_or_update(fd, 4, arg);
            string response = "200 OK\n";
            send(fd, response.c_str(), response.size(), 0);
        } else {
            string response = "502 Command not implemented\n";
            send(fd, response.c_str(), response.size(), 0);
            return;
        }
    }

    void handle_data_connection(int fd, int command, const string& filename) {
        cout << "Command type: " << command << endl;

        if (command == 1) {  // PASV模式
            // 获取控制连接fd
            int control_fd = group.get_control_from_data(fd);
            if (control_fd != -1) {
                // 数据传输完成后，清理PASV资源
                cleanup_pasv_resources(control_fd);
            }
        } else if (command == 2) {  // LIST命令
            handle_list(fd);
        } else if (command == 3) {  // RETR命令（下载）
            if (!filename.empty()) {
                handle_retr(fd, filename);
            } else {
                string err = "550 No such file or directory.\n";
                send(fd, err.c_str(), err.size(), 0);
            }
        } else if (command == 4) {  // STOR命令（上传）
            if (!filename.empty()) {
                handle_stor(fd, filename);
            } else {
                string err = "550 No such file or directory.\n";
                send(fd, err.c_str(), err.size(), 0);
            }
        }
        
        close(fd);  // 关闭数据连接
    }

    void handle_pasv(int control_fd) {
        cout << "处理PASV.." << endl;
        srand(time(NULL));
        int port = rand() % 40000 + 1024;  // 随机选择数据端口（1024-49151）
        int p1 = port / 256;
        int p2 = port % 256;

        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == -1) {
            perror("socket1 failed");
            return;
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        std::string ip = get_server_ip();
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        addr.sin_port = htons(port);
        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("bind1 failed");
            close(listen_fd);
            return;
        }

        if (listen(listen_fd, 5) == -1) {
            perror("listen1 failed");
            close(listen_fd);
            return;
        }

        // 将监听套接字添加到epoll中
        struct epoll_event ev;
        ev.data.fd = listen_fd;
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
            perror("epoll_ctl failed for listen_fd");
            close(listen_fd);
            return;
        }

        // 存储监听套接字与控制连接的映射
        group.add_listen_socket(listen_fd, control_fd);

        // 通过控制连接发送PASV响应
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));

        char str[4][4];
        sscanf(ip_str, "%3[^.].%3[^.].%3[^.].%3[^.]", str[0], str[1], str[2], str[3]);
        char arr[100];
        sprintf(arr, "227 entering passive mode (%s,%s,%s,%s,%d,%d)\r\n", str[0], str[1], str[2], str[3], p1, p2);
        send(control_fd, arr, strlen(arr), 0);
    }

    void handle_list(int fd) {
        cout << "处理LIST.." << endl;

        DIR* dp = opendir(".");
        if (!dp) {
            string err = "550 Failed to open directory\n";
            send(fd, err.c_str(), err.size(), 0);
            return;
        }

        string list_str;
        struct dirent* entry;

        while ((entry = readdir(dp)) != nullptr) {
            if (entry->d_name[0] == '.') continue;  // 跳过隐藏文件
            list_str += entry->d_name;
            list_str += "\n";
        }

        closedir(dp);
        send(fd, list_str.c_str(), list_str.size(), 0);
    }

    void handle_retr(int fd, const string& filename) {
        cout << "处理RETR.." << endl;

        int file_fd = open(filename.c_str(), O_RDONLY);
        if (file_fd < 0) {
            string err = "550 Failed to open file\n";
            send(fd, err.c_str(), err.size(), 0);
            return;
        }

        struct stat statbuf{};
        if (fstat(file_fd, &statbuf) < 0) {
            string err = "550 Failed to get file info\n";
            send(fd, err.c_str(), err.size(), 0);
            close(file_fd);
            return;
        }

        off_t offset = 0;
        ssize_t sent_bytes = 0;

        while (offset < statbuf.st_size) {
            sent_bytes = sendfile(fd, file_fd, &offset, statbuf.st_size - offset);
            if (sent_bytes <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
        }

        close(file_fd);
    }

    void handle_stor(int fd, const string& filename) {
        cout << "处理STOR.." << endl;

        int file_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd < 0) {
            string err = "550 Failed to create file\n";
            send(fd, err.c_str(), err.size(), 0);
            return;
        }

        char buf[4096];
        ssize_t n;
        while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
            write(file_fd, buf, n);  // 写入文件
        }

        if (n < 0 && errno != EINTR) {
            perror("recv error");
        }

        close(file_fd);
    }

    string get_server_ip() {
        struct ifaddrs* ifaddr, * ifa;
        if (getifaddrs(&ifaddr) == -1) {
            perror("getifaddrs failed");
            return "0.0.0.0";
        }

        std::string server_ip = "0.0.0.0";
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            // 只处理IPv4地址
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sa->sin_addr, ip_str, sizeof(ip_str));

            // 过滤掉回环地址（127.0.0.1）和未启用的接口
            if (strcmp(ip_str, "127.0.0.1") != 0 && (ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING)) {
                server_ip = ip_str;
                break;
            }
        }
        freeifaddrs(ifaddr);
        return server_ip;
    }

    // 清理PASV模式的相关资源（监听套接字）
    void cleanup_pasv_resources(int control_fd) {
        // 获取与控制连接关联的监听套接字
        int listen_fd = group.get_control_from_listen(control_fd);
        if (listen_fd != -1) {
            // 从映射中移除监听套接字
            group.remove_listen_socket(listen_fd);
            // 关闭监听套接字
            close(listen_fd);
        }
    }
};

int main() {
    FTPServer server;
    if (!server.init()) {
        cerr << "Failed to initialize FTPServer" << endl;
        return 1;
    }
    cout << "FTP server running on port " << CONTROL_PORT << endl;
    server.run();
    return 0;
}