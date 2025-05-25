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
#include <thread>
#include "threadpool.hpp"

using namespace std;

#define PORT 2100
#define PORT1 5000
#define SIZE 1024
#define EPSIZE 1024
#define maxevents 1024

threadpool control_pool(10); // 控制连接线程池
threadpool data_pool(10);     // 数据连接线程池

// ？？？？？？？？？？？？？？？？？？？？？？？？？
struct epoll_event ev;
std::mutex mtx; // 互斥锁
int epfd;
int server_fd;// 全局变量
//可以扔到类里面封装？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？/

class ControlConnect{
    public:
        int control_fd; // 控制连接的文件描述符
        int pasv;
        int list;
        int retr;
        int stor;
        ControlConnect(int fd,int n):control_fd(fd),pasv(0),list(0),retr(0),stor(0){
        if(n==1) pasv=1;
        if(n==2) list=1;
        if(n==3) retr=1;
        if(n==4) stor=1;
        }
        void set_msg(int n){
            if(n==1) pasv=1;
            if(n==2) list=1;
            if(n==3) retr=1;
            if(n==4) stor=1;
        }
};
    
class DataConnect{
    public:
        int data_fd; // 数据连接的文件描述符
        DataConnect(int fd):data_fd(fd){
            ;
        }
};


class ConnectionGroup{
    public:
        std::vector<ControlConnect> control_connections;
        std::vector<DataConnect> data_connections;
        std::unordered_map<int,int> data_to_control;

        void get_init_control(int fd,int n){ // 判断fd是否存在，存在就修改，不存在初始化

            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
        
            if(it==control_connections.end()){
            control_connections.emplace_back(fd,n);
            }else{
                // 修改参数
                it->set_msg(n);
            }
        }

        void get_init_data(int data_fd){
            data_connections.emplace_back(data_fd);
        }

        // 添加连接（control_fd 和 data_fd 关联）
        void add_connection(int control_fd,int data_fd){

            data_to_control[data_fd]=control_fd;  // 存储反向映射
        }

        // 通过 data_fd 查找 control_fd
        int find_control_fd(int data_fd){
            auto it=data_to_control.find(data_fd);
            if(it!=data_to_control.end()){
                return it->second;
            }
            return -1;  // 未找到
        }

        void remove_control_connection(int fd){
            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
        
            if(it!=control_connections.end()){
                control_connections.erase(it);  // 删除单个元素
            }
        
            // 清理data_to_control中指向该control_fd的映射
            for(auto it=data_to_control.begin();it!=data_to_control.end();){
                if(it->second==fd){
                    it=data_to_control.erase(it);  // 删除并返回下一个迭代器
                }else{
                    ++it;
                }
            }
        }
        
        
        




        // 查找fd的所在容器函数实现

        // ControlConnect get_control() const {
        //     if (is_control && control_result.has_value()) {
        //         return control_result.value();
        //     }
        //     throw std::runtime_error("Not a ControlConnect!");
        // }
        
        // // 获取 DataConnect（如果存在）
        // DataConnect get_data() const {
        //     if (!is_control && data_result.has_value()) {
        //         return data_result.value();
        //     }
        //     throw std::runtime_error("Not a DataConnect!");
        // }
};





// struct epoll_event {
//     uint32_t events;    // 发生的事件类型（如 EPOLLIN、EPOLLOUT）
//     epoll_data_t data;  // 用户自定义数据（通常存储 FD）
// };


void FTP_init(){
    int client_fd;
    struct sockaddr_in ser_addr,cli_addr;
    socklen_t ser_len,cli_len;

    memset(&ser_addr,0,sizeof(ser_addr));
    memset(&cli_addr,0,sizeof(cli_addr));

    if((server_fd=socket(AF_INET,SOCK_STREAM,0))<0){
        perror("Socket creation failed");
        exit(-1);
        //exit(EXIT_FAILURE);
    }

    ser_addr.sin_family=AF_INET;
    ser_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    ser_addr.sin_port=htons(PORT);
    ser_len=sizeof(ser_addr);

    if(bind(server_fd,(struct sockaddr *)&ser_addr,ser_len)<0){
        perror("Bind failed");
        close(server_fd);
        exit(-1);
    }

    if(listen(server_fd,5)<0){
        perror("Listen failed");
        close(server_fd);
        exit(-1);
    }

    epfd=epoll_create(EPSIZE);
    if(epfd==-1){
        perror("epoll_create failed");
        return;
    }
    
    ev.data.fd=server_fd;
    ev.events=EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLERR;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&ev)==-1){
        perror("epoll_ctl failed");
        return;
    }

    //记得close(epdf)
}
    
// 数据连接处理函数
void handle_pasv(int client_fd){
    return;

    // 创建线程


}

void handle_list(int client_fd){
    return;
}

void handle_retr(int client_fd){
    return;
}

void handle_stor(int client_fd){
    return;
}


void handle_msg(class ConnectionGroup group){
    ;
}

void handle_accept(int fd,class ConnectionGroup group){ // 控制连接的创建  线程中
    sockaddr_in client_addr{};
    socklen_t client_len=sizeof(client_addr);
    int connect_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);
    if(connect_fd==-1){
        perror("accept");
        return;
    }

    // 设置非阻塞模式
    fcntl(connect_fd,F_SETFL,fcntl(connect_fd,F_GETFL,0)|O_NONBLOCK);

    // 注册客户端连接到 epoll
    ev.events=EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLERR;
    ev.data.fd=connect_fd;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,connect_fd,&ev)==-1){
        perror("epoll_ctl");
        close(connect_fd);
    }


    //处理控制连接和数据连接实现
    // ?????????????????????









}



int get_port(int fd){
    struct sockaddr_in addr;
    socklen_t addr_len=sizeof(addr);

    if(getsockname(fd,(struct sockaddr *)&addr,&addr_len)==-1){
        perror("getsockname failed");
        return -1;
    }
    return ntohs(addr.sin_port);
}





void handle_control_msg(char *buf,int client_fd,class ConnectionGroup group){
{
    unique_lock<mutex> lock(mtx);
    if(strstr(buf,"PASV")!=NULL){ // 处理数据连接
        group.get_init_control(client_fd,1);
        
        
        handle_pasv(client_fd);





    }else if(strstr(buf,"LIST")!=NULL){ // 获取文件列表
        //handle_list(client_fd);
        group.get_init_control(client_fd,2);
    }else if(strstr(buf,"RETR")!=NULL){ // 文件下载
        //handle_retr(client_fd);
        group.get_init_control(client_fd,3);
    }else if(strstr(buf,"STOR")!=NULL){ // 文件上传
        //handle_stor(client_fd);
        group.get_init_control(client_fd,4);
    }else if(strstr(buf,"QUIT")!=NULL){ // 连接关闭
        group.remove_control_connection(client_fd);
        epoll_ctl(epfd,EPOLL_CTL_DEL,client_fd,nullptr);
        close(client_fd);
    }else{ // 其他命令
        send(client_fd,"500 Unknown command\r\n",21,0);
    }
}
}







void FTP_start(class ConnectionGroup group){
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
                std::thread client_thread(handle_accept,server_fd,group);
                client_thread.detach();
            }
            else{
                int fd=events[i].data.fd;
                if(get_port(fd)==2100){
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
                        auto future1=control_pool.enqueue(handle_control_msg,buf,fd,group);
                        future1.get();
                    }
                }else{
                    ;// 数据连接






                    auto future2=data_pool.enqueue(handle_msg,group);
                    future2.get();
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

// 线程池设计
// 控制连接线程池:

// 用于处理客户端的控制连接和命令（如 USER、PASS、PASV 等）。
// 每当有新的控制连接时，从这个线程池中获取一个线程来处理。
// 数据连接线程池:

// 用于处理数据传输（如文件上传和下载）。
// 当在控制连接中接收到 PASV 命令并建立数据连接后，从这个线程池中获取一个线程来处理数据传输。
