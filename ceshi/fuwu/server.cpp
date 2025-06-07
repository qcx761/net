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
                    
                        sockaddr_in client_addr{};
                        socklen_t client_len=sizeof(client_addr);
                        int client_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);

                        if(client_fd==-1) {
                            if(errno==EAGAIN||errno==EWOULDBLOCK) { // 判断是否是由于套接字处于非阻塞模式导致的错误(套接字没有准备好,暂时没有连接可以接受)
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
                    
                }else {
                    int port=get_socket_local_port(fd);

                    if(port==CONTROL_PORT) { // 控制连接
                        handle_control_fd(fd);
                    }else { // 数据连接





cout<<"数据连接进入"<<endl;




// 从数据u连接获取控制连接fd
                        int control_fd





                        int command=group.get_command_type(fd);




                        //cout<<command<<endl;


// 数据连接fd要关联控制连接的


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



    bool is_continue=false;



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
        cout<<"Control message from fd="<<fd<<": "<<buf;
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

        if(cmd=="PASV\r\n") {

            handle_pasv(fd);

            group.add_or_update(fd,1);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else if(cmd=="LIST\r\n") {











cout<<"111"<<endl;











            group.add_or_update(fd,2);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else if(cmd=="RETR\r\n") {
            group.add_or_update(fd,3,arg);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else if(cmd=="STOR\r\n") {
            group.add_or_update(fd,4,arg);
            string response="200 OK\n";
            send(fd,response.c_str(),response.size(),0);
        } else {
            string response="502 Command not implemented\n";
            send(fd,response.c_str(),response.size(),0);
        }
    }

    void handle_data_connection(int fd,int command,const string& filename) {

        //cout<<command<<endl;

        if(command==1) {
            handle_pasv(fd);
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

    void handle_pasv(int control_fd) {

        srand(time(NULL));
        int port=rand()%40000+1024;
        int p1=port/256;
        int p2=port%256;

        int listen_fd=socket(AF_INET,SOCK_STREAM,0);
        if(listen_fd==-1) {
            perror("socket1 failed");
            return;
        }

        struct sockaddr_in addr{};
        addr.sin_family=AF_INET;
        std::string ip=get_server_ip();
        inet_pton(AF_INET,ip.c_str(), &addr.sin_addr);

        addr.sin_port=htons(port);
        if(bind(listen_fd,(struct sockaddr*)&addr,sizeof(addr))==-1) {
            perror("bind1 failed");
            close(listen_fd);
            return;
        }

        if(listen(listen_fd,5)==-1) {
            perror("listen1 failed");
            close(listen_fd);
            return;
        }
        
        // fcntl(listen_fd,F_SETFL,O_NONBLOCK);

        socklen_t len=sizeof(addr);
        if(getsockname(listen_fd,(struct sockaddr*)&addr,&len)==-1) {
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
        ev.events=EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP|EPOLLERR;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,data_fd,&ev)==-1){
            perror("epoll_ctl failed");
            return;
        }



        

        // {
        //     unique_lock<mutex> lock(mtx);
        //     cv.wait(lock,[]{return is_continue;});   // 每个客户端弄个
        // }
        


        // {
        //     unique_lock<mutex> lock(mtx);
        //     is_continue=false;
        // }







         




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

    string get_server_ip() {
        struct ifaddrs *ifaddr,*ifa;
        if(getifaddrs(&ifaddr)==-1) {
            perror("getifaddrs failed");
            return "0.0.0.0";  // 失败时返回默认值
        }
    
        std::string server_ip="0.0.0.0";
        for(ifa = ifaddr; ifa!=nullptr;ifa=ifa->ifa_next) {
            // 只处理IPv4地址
            if(ifa->ifa_addr==nullptr||ifa->ifa_addr->sa_family!=AF_INET) {
                continue;
            }
    
            struct sockaddr_in *sa=(struct sockaddr_in *)ifa->ifa_addr;
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET,&sa->sin_addr,ip_str,sizeof(ip_str));
    
            // 过滤掉回环地址（127.0.0.1）和未启用的接口
            if(strcmp(ip_str,"127.0.0.1")!=0&& (ifa->ifa_flags&IFF_UP)&&(ifa->ifa_flags&IFF_RUNNING)) {
                server_ip=ip_str;  // 返回第一个符合条件的IP
                break;  // 可以修改逻辑选择特定IP（如优先选择eth0）
            }
        }
        freeifaddrs(ifaddr);
        return server_ip;
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
