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
#include <sys/sendfile.h>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <algorithm>  
#include <sys/sendfile.h>


using namespace std;

#define PORT 2100
#define PORT1 5000
#define SIZE 1024
#define EPSIZE 1024
#define maxevents 1024



std::mutex mtx; // 互斥锁
int epfd;
int server_fd;// 全局变量
bool is_continue;


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
        strcpy(filename,buf);
        }
        void set_msg(int m){
            n=m;
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

        // 初始化控制连接
        void get_init_control(int fd,int n,char* buf){ // 判断fd是否存在，存在就修改，不存在初始化

            auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});
        
            if(it==control_connections.end()){
            control_connections.emplace_back(fd,n,buf);
            }else{
                // 修改参数
                if(buf){
                strcpy(it->filename,buf);
                }
                it->set_msg(n);
            }
        }

        // 初始化数据连接
        void get_init_data(int data_fd){
            data_connections.emplace_back(data_fd);
        }

        // 添加连接（control_fd 和 data_fd 关联）
        void add_connection(int control_fd,int data_fd){                                 // 好像没用。。。。。。   要关联吗？？？？？

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
                strcpy(a,it->filename);
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

    struct epoll_event ev;
    ev.data.fd=server_fd;
    ev.events=EPOLLIN|EPOLLET;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&ev)==-1){
        perror("epoll_ctl failed");
        return;
    }

    //记得close(epdf)
}
    




// 数据连接创建
void handle_pasv(int control_fd,ConnectionGroup& group){

        
        srand(time(NULL));
        int port=rand()%40000+1024;
        int p1=port/256;
        int p2=port%256;

        int listen_fd=socket(AF_INET,SOCK_STREAM,0);
        if(listen_fd==-1){
            perror("socket failed");
            return;
        }

        struct sockaddr_in addr{};
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=INADDR_ANY;
        addr.sin_port=htons(port);
        
        if(bind(listen_fd,(struct sockaddr*)&addr,sizeof(addr))==-1){
            perror("bind failed");
            close(listen_fd);
            return;
        }

        if(listen(listen_fd,5)==-1){
            perror("listen failed");
            close(listen_fd);
            return;
        }
        
        fcntl(listen_fd,F_SETFL,O_NONBLOCK);

        socklen_t len=sizeof(addr);
        if(getsockname(listen_fd,(struct sockaddr*)&addr,&len)==-1){
            perror("getsockname failed");
            return;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&addr.sin_addr,ip_str,sizeof(ip_str));

        char str[4][4];
        sscanf(ip_str,"%3[^.].%3[^.].%3[^.].%3[^.]",str[0],str[1],str[2],str[3]);
        char arr[100];
        sprintf(arr,"227 entering passive mode (%s,%s,%s,%s,%d,%d)",str[0],str[1],str[2],str[3],p1,p2);
        send(control_fd,arr,sizeof(arr),0);  // 通过控制连接发送信息

        struct epoll_event ev;
        ev.data.fd=listen_fd;
        ev.events = EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP|EPOLLERR;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&ev)==-1){
            perror("epoll_ctl failed");
            return;
        }

        while(1){
        if(is_continue){
            int n=group.find_n(control_fd);
            char *filename=group.find_filename(control_fd);
            if(n==2){
                handle_list(listen_fd);
                free(filename);
            }else if(n==3){
                handle_retr(listen_fd,filename);
                free(filename);
            }else if(n==4){
                handle_stor(listen_fd,filename);
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
    }

// 数据连接fd
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
        send(data_fd,buffer,strlen(buffer),0);
    }

    closedir(dir);
    send(data_fd,"226 Transfer complete.\r\n",24,0);
}



// 服务端将指定的文件传输给客户端

// 文件较大可以分块传输？？  实现？？
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






void handle_control_msg(char *buf,int server_fd,ConnectionGroup& group){ // 这个锁奇奇怪怪的
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

{
    unique_lock<mutex> lock(mtx);
    if(strstr(buf,"PASV")!=NULL){ // 处理数据连接
        group.get_init_control(server_fd,1,nullptr);
        std::thread client_thread(handle_pasv,server_fd,std::ref(group));
        client_thread.detach();
    }else if(strstr(buf,"LIST")!=NULL){ // 获取文件列表
        group.get_init_control(server_fd,2,str);
    }else if(strstr(buf,"RETR")!=NULL){ // 文件下载
        if(!strcmp(str,"wuxiao")&&str)
        group.get_init_control(server_fd,3,str);
        else
        group.get_init_control(server_fd,3,nullptr);
    }else if(strstr(buf,"STOR")!=NULL){ // 文件上传
        if(!strcmp(str,"wuxiao")&&str)
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
    is_continue=true;

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
