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


#include <ifaddrs.h>  // 包含网络接口信息（包含 ifaddrs 结构体）
#include <linux/if.h> // 包含网络接口标志（如 IFF_UP, IFF_RUNNING）

using namespace std;

#define PORT 2100
#define PORT1 5000
#define SIZE 1024
#define EPSIZE 1024
#define maxevents 1024
#define INET_ADDRSTRLEN 16


std::condition_variable cv;
std::mutex mtx; // 互斥锁
std::mutex mtx1; // 互斥锁
std::mutex mtx2; // 互斥锁
int epfd;
int server_fd;// 全局变量
bool is_continue=false;


void handle_list(int data_fd);
void handle_retr(int data_fd,char* filename);
void handle_stor(int data_fd,char *filename);


class ControlConnect{
    public:
        int control_fd; // 控制连接的文件描述符
        char filename[100];
        int n;
        ControlConnect(int fd,int m,char* buf):control_fd(fd){
            n=m;
            if(buf){
                std::lock_guard<std::mutex> lock(mtx1);
                strcpy(filename,buf);
            }
            else{
                std::lock_guard<std::mutex> lock(mtx1);
                strcpy(filename,"false");
            }
        }

        void set_msg(int m){
            std::lock_guard<std::mutex> lock(mtx2);
            n=m;
        }

        void set_filename(const char* buf){
            std::lock_guard<std::mutex> lock(mtx1);
            strcpy(filename,buf);
        }
    
        const char* get_filename(){
            std::lock_guard<std::mutex> lock(mtx1);
            return filename;
        }
};

class ConnectionGroup{
    public:
        std::vector<ControlConnect> control_connections;

        // 初始化控制连接
        void get_init_control(int fd,int n,char* buf){ // 判断fd是否存在，存在就修改，不存在初始化
            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
            if(it==control_connections.end()){
            control_connections.emplace_back(fd,n,buf);
            }else{
                // 修改参数
                if(buf){
                // strcpy(it->filename,buf);
                it->set_filename(buf);
                }
                it->set_msg(n);
            }

        }

        void remove_control_connection(int fd){
            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
        
            if(it!=control_connections.end()){
                control_connections.erase(it);  // 删除单个元素
            }
        }
        
        int find_n(int fd){
            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
        
            if(it==control_connections.end()){
            return 0;
            }else{
                return it->n;
            }
        }
        
        char* find_filename(int fd){
            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
        
            if(it==control_connections.end()){
                char* b=(char *)malloc(sizeof(char)*100);
                strcpy(b,"wuxiao");
                return b;
            }else{
                char* a=(char *)malloc(sizeof(char)*100);
                strcpy(a,it->get_filename());
                return a;
            }
        }
};


int get_port(int fd){
    struct sockaddr_in addr;
    socklen_t addr_len=sizeof(addr);

    if(getsockname(fd,(struct sockaddr *)&addr,&addr_len)==-1){
        perror("getsockname failed");
        return -1;
    }
    return ntohs(addr.sin_port);
}

void FTP_init(){
    struct sockaddr_in ser_addr; 
    socklen_t ser_len; 

    memset(&ser_addr,0,sizeof(ser_addr));

    if((server_fd=socket(AF_INET,SOCK_STREAM,0))<0){
        perror("Socket creation failed");
        exit(-1);
    }

    ser_addr.sin_family=AF_INET;
    ser_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    ser_addr.sin_port=htons(PORT);
    ser_len=sizeof(ser_addr);

    if(bind(server_fd,(struct sockaddr *)&ser_addr,ser_len)<0){
        perror("Bind0 failed");
        close(server_fd);
        exit(-1);
    }

    if(listen(server_fd,5)<0){
        perror("Listen0 failed");
        close(server_fd);
        exit(-1);
    }

    epfd=epoll_create(EPSIZE);
    if(epfd==-1){
        perror("epoll_create failed");
        return;
    }

    struct epoll_event ev;
    ev.data.fd=server_fd;
    ev.events=EPOLLIN|EPOLLET;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&ev)==-1){
        perror("epoll_ctl failed");
        return;
    }

    //记得close(epdf)
}


std::string get_server_ip() {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return "0.0.0.0";  // 失败时返回默认值
    }

    std::string server_ip = "0.0.0.0";
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        // 只处理IPv4地址
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, ip_str, sizeof(ip_str));

        // 过滤掉回环地址（127.0.0.1）和未启用的接口
        if (strcmp(ip_str, "127.0.0.1") != 0 && 
            (ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING)) {
            server_ip = ip_str;  // 返回第一个符合条件的IP
            break;  // 可以修改逻辑选择特定IP（如优先选择eth0）
        }
    }
    freeifaddrs(ifaddr);
    return server_ip;
}


// 数据连接创建
void handle_pasv(int control_fd,ConnectionGroup& group){
        
        srand(time(NULL));
        int port=rand()%40000+1024;
        int p1=port/256;
        int p2=port%256;

        int listen_fd=socket(AF_INET,SOCK_STREAM,0);
        if(listen_fd==-1){
            perror("socket1 failed");
            return;
        }

        struct sockaddr_in addr{};
        addr.sin_family=AF_INET;
        std::string ip = get_server_ip();
        inet_pton(AF_INET,ip.c_str(), &addr.sin_addr);


        //addr.sin_addr.s_addr=INADDR_ANY;

        addr.sin_port=htons(port);
        if(bind(listen_fd,(struct sockaddr*)&addr,sizeof(addr))==-1){
            perror("bind1 failed");
            close(listen_fd);
            return;
        }

        if(listen(listen_fd,5)==-1){
            perror("listen1 failed");
            close(listen_fd);
            return;
        }
        
        // fcntl(listen_fd,F_SETFL,O_NONBLOCK);

        socklen_t len=sizeof(addr);
        if(getsockname(listen_fd,(struct sockaddr*)&addr,&len)==-1){
            perror("getsockname1 failed");
            return;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&addr.sin_addr,ip_str,sizeof(ip_str));

        char str[4][4];
        sscanf(ip_str,"%3[^.].%3[^.].%3[^.].%3[^.]",str[0],str[1],str[2],str[3]);
        char arr[100];
        sprintf(arr,"227 entering passive mode (%s,%s,%s,%s,%d,%d)",str[0],str[1],str[2],str[3],p1,p2);
        send(control_fd,arr,sizeof(arr),0);  // 通过控制连接发送信息


        sockaddr_in server_addr{};
        socklen_t server_len=sizeof(server_addr);
        int data_fd=accept(listen_fd,(sockaddr*)&server_addr,&server_len);
        if(data_fd==-1){
            perror("accept");
            return;
        }


        struct epoll_event ev;
        ev.data.fd=data_fd;
        ev.events = EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP|EPOLLERR;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,data_fd,&ev)==-1){
            perror("epoll_ctl failed");
            return;
        }

        cout << "data_connection created" << endl;

        while(1){
            {
                unique_lock<mutex> lock(mtx);
                cv.wait(lock,[]{return is_continue;});
            }
            int n=group.find_n(control_fd);
            char *filename=group.find_filename(control_fd);



//cout << "111"<<endl;

            if(n==2){
//cout << "111"<<endl;

                handle_list(data_fd);
                free(filename);
            }else if(n==3){
//cout << "111"<<endl;

                handle_retr(data_fd,filename);
                free(filename);
            }else if(n==4){
//cout << "111"<<endl;

                handle_stor(data_fd,filename);
                free(filename);
            }else{

                free(filename);
                continue;
            }

            {
                unique_lock<mutex> lock(mtx);
                is_continue=false;
            }
    }    
}




void handle_list(int data_fd){ // 传输目录下的文件
    DIR *dir;
    struct dirent *entry;
    char buffer[1024];

    dir=opendir(".");
    if(!dir){
        send(data_fd,"550 Failed to open directory.\r\n",30,0);
        return;
    }
    while((entry=readdir(dir))!=NULL){
        if(strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0){
            continue;
        }
        snprintf(buffer,sizeof(buffer),"%s\r\n",entry->d_name);
        ssize_t bytes_sent=send(data_fd,buffer,strlen(buffer),0);
        if(bytes_sent==-1){
            if(errno==EPIPE){
                // 客户端已断开连接，退出循环
                break;
            }else{
                // 其他错误（如网络问题），记录日志或处理
                perror("send failed");
                break;
            }
        }
    }
    closedir(dir);
    send(data_fd,"226 Transfer complete.\r\n",24,0);
}





// 服务端将指定的文件传输给客户端
void handle_retr(int data_fd,char* filename){


    int file_fd=open(filename,O_RDONLY);
    if(file_fd==-1){
        send(data_fd,"550 File not found.\r\n",20,0);
        return;
    }

    struct stat statbuf;
    if(fstat(file_fd,&statbuf)==-1){ // 获取文件大小
        close(file_fd);
        send(data_fd,"550 Failed to get file size.\r\n",30,0);
        return;
    }

    // 偏移量,记录offset可支持端点续传？？

    off_t offset = 0;
    ssize_t bytes_sent=sendfile(data_fd,file_fd,&offset,statbuf.st_size);
    
    if(bytes_sent==-1){
        perror("sendfile failed");
        close(file_fd);
        send(data_fd,"426 Connection closed; transfer aborted.\r\n",42,0);
        return;
    }

    close(file_fd);
    send(data_fd, "226 Transfer complete.\r\n", 24, 0);
}




// 服务端准备接收并存储客户端传输的文件
void handle_stor(int data_fd,char *filename){
    // 打开文件 如果存在则覆盖,不存在则创建
    int file_fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(file_fd==-1){
        perror("open failed");
        send(data_fd,"550 Failed to create file.\r\n",28,0);
        return;
    }

    char buffer[4096];
    ssize_t bytes_received;

    // 循环接收数据并写入文件
    while((bytes_received=recv(data_fd,buffer,4096,0))>0){
        ssize_t bytes_written=write(file_fd,buffer,bytes_received);
        if(bytes_written==-1){
            perror("write failed");
            close(file_fd);
            send(data_fd,"451 Write error.\r\n",18,0);
            return;
        }
    }

    // 检查接收是否出错
    if(bytes_received==-1){
        perror("recv failed");
        close(file_fd);
        send(data_fd, "426 Connection closed; transfer aborted.\r\n", 42, 0);
        return;
    }

    //  关闭文件并发送成功响应
    close(file_fd);
    send(data_fd,"226 Transfer complete.\r\n",24,0);
}




// 控制连接的建立
void handle_accept(int fd,ConnectionGroup& group){ 
    sockaddr_in client_addr{};
    socklen_t client_len=sizeof(client_addr);
    int connect_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);
    if(connect_fd==-1){
        perror("accept");
        return;
    }

    // 设置非阻塞模式
    fcntl(connect_fd,F_SETFL,fcntl(connect_fd,F_GETFL,0)|O_NONBLOCK);

    // 注册到 epoll
    struct epoll_event ev;
    ev.events=EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLERR; // 客户端向服务端发送信息触发
    ev.data.fd=connect_fd;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,connect_fd,&ev)==-1){
        perror("epoll_ctl");
        close(connect_fd);
    }    
}






void handle_control_msg(char *buf,int server_fd,ConnectionGroup& group){ 
    size_t length=strlen(buf);
    char buf_copy[256];
    char str[128];
    strncpy(buf_copy,buf,sizeof(buf_copy)-1);
    buf_copy[sizeof(buf_copy)-1]='\0';
    char* token=strtok(buf_copy," ");
    if(token!=nullptr){
        token=strtok(nullptr," ");
        if(token!=nullptr){
            strcpy(str,token);
            str[strlen(str)]='\0';
        }else{
            strcpy(str,"wuxiao");
        }
    }
    if(strstr(buf,"PASV")!=NULL){ // 处理数据连接
        group.get_init_control(server_fd,1,nullptr);
        cout << "creating data_connection..." << endl;
        std::thread client_thread(handle_pasv,server_fd,std::ref(group));
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 等待50毫秒建立数据连接
        client_thread.detach();
        return;
    }else if(strstr(buf,"LIST")!=NULL){ // 获取文件列表
        group.get_init_control(server_fd,2,str);
    }else if(strstr(buf,"RETR")!=NULL){ // 文件下载
        if(strcmp(str,"wuxiao")!=0)
        group.get_init_control(server_fd,3,str);
        else
        group.get_init_control(server_fd,3,nullptr);
    }else if(strstr(buf,"STOR")!=NULL){ // 文件上传
        if(strcmp(str,"wuxiao")!=0)
        group.get_init_control(server_fd,4,str);
        else
        group.get_init_control(server_fd,4,nullptr);
    }else if(strstr(buf,"QUIT")!=NULL){ // 连接关闭
        group.remove_control_connection(server_fd);                          // 没关联要取消掉
        epoll_ctl(epfd,EPOLL_CTL_DEL,server_fd,nullptr);
        close(server_fd);
    }else{ // 其他命令
        send(server_fd,"500 Unknown command\r\n",21,0);
    }

    {
    unique_lock<mutex> lock(mtx);
    is_continue=true;
    cv.notify_one();
    }
}







void FTP_start(ConnectionGroup& group){
    while(1){
        struct epoll_event events[maxevents];
        int n=epoll_wait(epfd,events,maxevents,-1);
        if(n==-1){
            perror("epoll_wait failed");
            break;
        }
        for(int i=0;i<n;i++){
            if(events[i].events&(EPOLLERR|EPOLLRDHUP)){ // 处理错误或连接关闭
                printf("Client error or disconnected: %d\n",events[i].data.fd);
                epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr);
                close(events[i].data.fd);
                continue;
            }
            if(events[i].data.fd==server_fd){ // 客户端连接
                std::thread client_thread(handle_accept,server_fd,std::ref(group));
                client_thread.join();
            }
            else{ // 数据连接和控制连接触发
                int fd=events[i].data.fd;
                if(get_port(fd)==2100){ // 控制连接
                    char buf[1024];
                    ssize_t len=read(events[i].data.fd,buf,sizeof(buf)-1);
                    if(len<=0){
                        if(len==0){ // 客户端主动关闭
                            printf("Client disconnected: %d\n",events[i].data.fd);
                        }else{ // 连接出错
                            perror("read failed");
                        }
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr);
                    close(events[i].data.fd);
                    }else{
                        buf[len]='\0';
                        // auto future1=control_pool.enqueue(handle_control_msg,buf,fd,group);
                        // future1.get();
                        cout << "命令: " << buf << endl;
                        handle_control_msg(buf,fd,group);
                    } 
                }else{ // 数据连接
                    if(events[i].events&EPOLLIN){ // 处理可读事件
                        // 接收文件中
                        ;
                    }else if(events[i].events & EPOLLOUT){ // 处理可写事件
                        // 上传文件中
                        ;
                    }else{ // 不知道还有啥
                        return;
                    }
                }
            }
        }
    }
}

//epoll中的文件描述符关闭


int main(){
    ConnectionGroup group;
    // 初始化 group
    FTP_init();
    FTP_start(group);
    close(server_fd);
    return 0;

    // epdf  server——fd   event封装
}




// 多线程唤醒有问题std::map<int, std::condition_variable>

























































































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

    ControlConnect(int fd,int cmd,const string& fname=""):control_fd(fd),command_type(cmd),filename(fname){}
};

class ConnectionGroup {
public:
    mutex mtx;
    vector<ControlConnect> connections;

    void add_or_update(int fd,int cmd,const string& fname="") {
        lock_guard<mutex> lock(mtx);
        auto it=find_if(connections.begin(),connections.end(),[fd](const ControlConnect& connect){return connect.control_fd==fd;});
        if(it==connections.end()) {
            connections.emplace_back(fd,cmd,fname);
        } else {
            it->command_type=cmd;
            if(!fname.empty()) {
                it->filename=fname;
            }
        }
    }

    void remove(int fd) {
        lock_guard<mutex> lock(mtx);
        connections.erase(remove_if(connections.begin(),connections.end(),[fd](const ControlConnect& connect){return connect.control_fd==fd;}),connections.end());
        // 将所有满足的元素移动到容器末尾并删除
        // remove_if返回一个指向第一个符合条件的元素的新位置
    }

    int get_command_type(int fd) {
        lock_guard<mutex> lock(mtx);
        auto it=find_if(connections.begin(),connections.end(),[fd](const ControlConnect& connect){return connect.control_fd==fd;});
        return (it==connections.end()) ? 0 : it->command_type;
    }
    
    string get_filename(int fd) {
        lock_guard<mutex> lock(mtx);
        auto it=find_if(connections.begin(),connections.end(),[fd](const ControlConnect& connect){return connect.control_fd==fd;});
        return (it==connections.end()) ? "" :it->filename;
    }
};

class FTPServer {
public:
    FTPServer(int control_port=CONTROL_PORT,size_t thread_count=4):control_port(control_port),thread_pool(thread_count),epfd(-1),server_fd(-1),is_running(false){}
    // 可自定义传入端口和线程池中线程创建数量

    bool init() {
        server_fd=socket(AF_INET,SOCK_STREAM,0);
        if(server_fd<0) {
            perror("socket failed");
            return false;
        }

        int opt=1;
        setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
        // 允许多个套接字(在同一主机上)绑定到同一个端口号

        sockaddr_in addr{};
        socklen_t addr_len=sizeof(addr);
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=htonl(INADDR_ANY);
        addr.sin_port=htons(control_port);

        if(bind(server_fd,(sockaddr*)&addr,addr_len)<0) {
            perror("bind failed");
            close(server_fd);
            return false;
        }

        if(listen(server_fd,10)<0) {
            perror("listen failed");
            close(server_fd);
            return false;
        }

        epfd=epoll_create1(0);
        if(epfd==-1) {
            perror("epoll_create1 failed");
            close(server_fd);
            return false;
        }

        epoll_event ev{};
        ev.events=EPOLLIN|EPOLLET;
        ev.data.fd=server_fd;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&ev)==-1) {
            perror("epoll_ctl ADD server_fd failed");
            close(server_fd);
            close(epfd);
            return false;
        }

        is_running=true;
        return true;
    }

    void run() {
        epoll_event events[MAX_EVENTS];
        while(is_running) {
            int n=epoll_wait(epfd,events,MAX_EVENTS,-1);
            if(n==-1) {
                if(errno==EINTR){ // 信号被中断
                    continue;
                }
                perror("epoll_wait");
                break;
            }
            for(int i=0;i<n;i++) {
                int fd=events[i].data.fd;
                uint32_t evs=events[i].events;

                if((evs&EPOLLERR)||(evs&EPOLLHUP)||(evs&EPOLLRDHUP)) {
                    close_connection(fd);
                    continue;
                }

                if(fd==server_fd) { // 客户端连接
                    while(true) {
                        sockaddr_in client_addr{};
                        socklen_t client_len=sizeof(client_addr);
                        int client_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);

                        if(client_fd==-1) {
                            if(errno==EAGAIN||errno==EWOULDBLOCK) { // 判断是否是由于套接字处于非阻塞模式导致的错误(套接字没有准备好,暂时没有连接可以接受)
                                break;
                            } else {
                                perror("accept");
                                break;
                            }
                        }

                        set_nonblocking(client_fd); // 设置非阻塞模式
                        epoll_event client_ev{};
                        client_ev.events=EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLERR;
                        client_ev.data.fd=client_fd;

                        if(epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&client_ev)==-1) {
                            perror("epoll_ctl ADD client_fd failed");
                            close(client_fd);
                        }
                        
                        cout<<"New control connection accepted: fd="<<client_fd<<endl;
                    }
                }else {
                    int port=get_socket_local_port(fd);

                    if(port==CONTROL_PORT) { // 控制连接
                        handle_control_fd(fd);
                    }else {
                        int command=group.get_command_type(fd);
                        string filename=group.get_filename(fd);
                        thread_pool.enqueue([this,fd,command,filename](){handle_data_connection(fd,command,filename);});
                        // this允许访问handle_data_connection函数，并将其交给线程池实现
                        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr); // 结束数据连接 
                    }
                }
            }
        }
    }


    void stop() {
        is_running=false;
        if(server_fd!=-1) close(server_fd);
        if(epfd!=-1) close(epfd);
    }





    ~FTPServer() {
        
    }
private:
    int control_port;
    int epfd;
    int server_fd;
    bool is_running;
    threadpool thread_pool;
    ConnectionGroup group;

    // 设置非阻塞
    static void set_nonblocking(int fd) {
        int flags=fcntl(fd,F_GETFL,0);
        fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    }

    // 通过套接字获取端口号
    static int get_socket_local_port(int fd) {
        sockaddr_in addr{};
        socklen_t len=sizeof(addr);

        if(getsockname(fd,(sockaddr*)&addr,&len)==-1) {
            perror("getsockname");
            return -1;
        }

        return ntohs(addr.sin_port);
    }

    // 关闭套接字并注销epoll
    void close_connection(int fd) {
        cout<<"Closing connection fd="<<fd<<endl;
        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
        close(fd);
        group.remove(fd);
    }
    
    // 控制连接获取命令
    void handle_control_fd(int fd) {
        char buf[1024];
        ssize_t len=recv(fd,buf,sizeof(buf)-1,0);
        if(len<=0) {
            if(len==0) {
                cout<<"Client disconnected: fd="<<fd<<endl;
            }else {
                perror("recv");
            }
            close_connection(fd);
            return;
        }
        buf[len]='\0';























        cout<<buf<<endl;



        cout<<"Control message from fd="<<fd<<": "<<buf<<endl;
        parse_and_handle_command(fd,string(buf));
    }

    // 解析并处理控制连接命令
    void parse_and_handle_command(int fd,const string& cmd_line) {
        string cmd,arg;
        size_t pos=cmd_line.find(' '); // 查找第一个空格的位置

        if(pos==string::npos) { //没找到空格
            cmd=cmd_line;
            arg="";
        }else {
            cmd=cmd_line.substr(0,pos);
            arg=cmd_line.substr(pos+1);
            // 删除/r与/n
            arg.erase(remove(arg.begin(),arg.end(),'\r'),arg.end());
            arg.erase(remove(arg.begin(),arg.end(),'\n'),arg.end());
        }

        // 将命令转化为大写字母
        transform(cmd.begin(),cmd.end(),cmd.begin(),::toupper);

        if(cmd=="PASV") {
            group.add_or_update(fd,1);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else if(cmd=="LIST") {
            group.add_or_update(fd,2);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else if(cmd=="RETR") {
            group.add_or_update(fd,3,arg);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else if(cmd=="STOR") {
            group.add_or_update(fd,4,arg);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else {
            string response="502 Command not implemented\n";
            send(fd,response.c_str(),response.size(),0);
        }
    }

    void handle_data_connection(int fd,int command,const string& filename) {
        if(command==1) {
            string response="PASV mode data connection established\n";
            send(fd,response.c_str(),response.size(),0);
        }else if(command==2) {
            handle_list(fd);
        }else if(command==3) {
            if(!filename.empty()) {
                handle_retr(fd,filename);
            }else {
                string err="550 No such file or directory.\n";
                send(fd,err.c_str(),err.size(),0);
            }
        }else if(command==4) {
            if(!filename.empty()) {
                handle_stor(fd,filename);
            }else {
                string err="550 No such file or directory.\n";
                send(fd,err.c_str(),err.size(),0);
            }
        }
    }

    void handle_list(int fd) {
        DIR* dp=opendir(".");

        if(!dp) {
            string err="550 Failed to open directory\n";
            send(fd,err.c_str(),err.size(),0);
            return;
        }
        
        string list_str;
        struct dirent* entry;
        
        while((entry=readdir(dp))!=nullptr) {
            if(entry->d_name[0]=='.') continue;
            list_str+=entry->d_name;
            list_str+="\n";
        }
        
        closedir(dp);
        send(fd,list_str.c_str(),list_str.size(),0);
    }

    void handle_retr(int fd,const string& filename) {
        int file_fd=open(filename.c_str(),O_RDONLY);

        if(file_fd<0) {
            string err="550 Failed to open file\n";
            send(fd,err.c_str(),err.size(),0);
            return;
        }
        
        struct stat statbuf{};

        if(fstat(file_fd,&statbuf)<0) {
            string err="550 Failed to get file info\n";
            send(fd,err.c_str(),err.size(),0);
            close(file_fd);
            return;
        }
        
        off_t offset=0;
        ssize_t sent_bytes=0;

        while(offset<statbuf.st_size) {
            sent_bytes=sendfile(fd,file_fd,&offset,statbuf.st_size-offset);
            if(sent_bytes<=0) {
                if(errno==EINTR){
                    continue;
                }
                break;
            }
        }

        close(file_fd);
    }

    void handle_stor(int fd,const string& filename) {
        int file_fd=open(filename.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0666);

        if(file_fd<0) {
            string err="550 Failed to create file\n";
            send(fd,err.c_str(),err.size(),0);
            return;
        }

        char buf[4096];
        ssize_t n;
        while((n=recv(fd,buf,sizeof(buf),0))>0) {
            write(file_fd,buf,n);
        }
        close(file_fd);
    }
};

int main() {
    FTPServer server;
    if(!server.init()) {
        cerr<<"Failed to initialize FTPServer"<<endl;
        return 1;
    }
    cout<<"FTP server running on port "<<CONTROL_PORT<<endl;
    server.run();
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
    mutex map_mtx;
    vector<ControlConnect> connections;
    unordered_map<int, int> data_to_control;
    unordered_map<int, int> listen_to_control;  // 新增：监听套接字到控制连接的映射

    void bind_data_to_control(int data_fd, int control_fd) {
        lock_guard<mutex> lock(map_mtx);
        data_to_control[data_fd] = control_fd;
    }

    int get_control_from_data(int data_fd) {
        lock_guard<mutex> lock(map_mtx);
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
        lock_guard<mutex> lock2(map_mtx);
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
        lock_guard<mutex> lock(map_mtx);
        listen_to_control[listen_fd] = control_fd;
    }

    // 新增：通过监听套接字获取控制连接
    int get_control_from_listen(int listen_fd) {
        lock_guard<mutex> lock(map_mtx);
        auto it = listen_to_control.find(listen_fd);
        return (it == listen_to_control.end()) ? -1 : it->second;
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
                        int control_fd = group.get_control_from_data(fd);
                        if (control_fd == -1) {
                            // 可能是PASV模式的监听套接字
                            control_fd = group.get_control_from_listen(fd);
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



                            } else {
                                cerr << "No control connection associated with data fd: " << fd << endl;
                                close_connection(fd);
                            }
                        } else {
                            // 正常的数据连接处理
                            int command = group.get_command_type(control_fd);
                            string filename = group.get_filename(control_fd);

                            thread_pool.enqueue([this, fd, command, filename]() {
                                handle_data_connection(fd, command, filename);
                            });

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

        if (cmd == "PASV\r\n") {
            handle_pasv(fd);
            group.add_or_update(fd, 1);
            string response = "200 OK\n";
            send(fd, response.c_str(), response.size(), 0);
        } else if (cmd == "LIST\r\n") {
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

        if (command == 1) {
            cout << "数据连接早就建立了" << endl;
            // handle_pasv(fd);
            // string response="PASV mode data connection established\n";
            // send(fd,response.c_str(),response.size(),0);
        } else if (command == 2) {
            handle_list(fd);
        } else if (command == 3) {
            if (!filename.empty()) {
                handle_retr(fd, filename);
            } else {
                string err = "550 No such file or directory.\n";
                send(fd, err.c_str(), err.size(), 0);
            }
        } else if (command == 4) {
            if (!filename.empty()) {
                handle_stor(fd, filename);
            } else {
                string err = "550 No such file or directory.\n";
                send(fd, err.c_str(), err.size(), 0);
            }
        }
        
        close(fd);
    }

    void handle_pasv(int control_fd) {
        srand(time(NULL));
        int port = rand() % 40000 + 1024;
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
        DIR* dp = opendir(".");
        if (!dp) {
            string err = "550 Failed to open directory\n";
            send(fd, err.c_str(), err.size(), 0);
            return;
        }

        string list_str;
        struct dirent* entry;

        while ((entry = readdir(dp)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            list_str += entry->d_name;
            list_str += "\n";
        }

        closedir(dp);
        send(fd, list_str.c_str(), list_str.size(), 0);
    }

    void handle_retr(int fd, const string& filename) {
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

    void handle_stor(int fd, const string& filename) {
        int file_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd < 0) {
            string err = "550 Failed to create file\n";
            send(fd, err.c_str(), err.size(), 0);
            return;
        }

        char buf[4096];
        ssize_t n;
        while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
            write(file_fd, buf, n);
        }
        close(file_fd);
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