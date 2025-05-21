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
        ControlConnect(int fd):control_fd(fd){
            ;// 其他初始化代码
        }
};
    
class DataConnect{
    public:
        int data_fd; // 数据连接的文件描述符
        DataConnect(int fd):data_fd(fd){
            ;// 其他初始化代码
        }
};

class ConnectionGroup{
    public:
        std::vector<ControlConnect> control_connects;
        std::vector<DataConnect> data_connects;
        std::map<ControlConnect, DataConnect> connections;
    
        // void add_control_connection(int control_fd){
        //     control_connections.emplace_back(control_fd);
        // }
        
        // void add_data_connection(int data_fd){
        //     data_connections.emplace_back(data_fd);
        // }

        void add_connection(int control_fd,int data_fd){
            control_connections.emplace_back(control_fd);
            data_connections.emplace_back(data_fd);
            connections.emplace(control_fd,data_fd);
        }

};







// struct epoll_event {
//     uint32_t events;    // 发生的事件类型（如 EPOLLIN、EPOLLOUT）
//     epoll_data_t data;  // 用户自定义数据（通常存储 FD）
// };



// void handle_client(int control_fd) {
//     // 处理控制命令的逻辑
// }

// void handle_data_transfer(int data_fd) {
//     // 处理文件上传或下载的逻辑
// }

// void handle_pasv_command(int control_fd, threadpool& data_pool) {
//     // 选择随机端口并监听
//     int data_port = choose_random_port();
//     listen_on_port(data_port);
    
//     // 返回PASV响应给客户端
//     send_response(control_fd, data_port);
    
//     // 等待客户端连接
//     int data_fd = accept_connection(data_port);
    
//     // 从数据连接线程池获取线程处理数据传输
//     data_pool.enqueue(handle_data_transfer, data_fd);
// }



// void send_response(int client_fd,const string& message){
//     string response=message+"\r\n"; // FTP协议的CRLF结尾
//     send(client_fd,response.c_str(),response.size(),0); // 发送响应
// }

// void handle_pasv(control_fd){   
// ;
// }
// void handle_list(control_fd){
// ;
// }
// void handle_retr(control_fd){
// ;
// }
// void handle_stor(control_fd){
// ;
// }


// void handle_client(int control_fd){
//     char buffer[SIZE];
    
//     while(1){
//         memset(buffer,0,sizeof(buffer));
//         int received;
//         if((received=recv(control_fd,buffer,sizeof(buffer)-1,0))<0){
//             perror("recv failed");
//         }        
//         if(buffer=="PASV"){
//             handle_pasv(control_fd);
//         }else if(buffer=="LIST"){
//             handle_list(control_fd);
//         }else if(buffer=="RETR"){
//             handle_retr(control_fd);
//         }else if(buffer=="STOR"){
//             handle_stor(control_fd);
//         }else if(buffer=="QUIT"){
//             send_response(control_fd,"221 Goodbye.");
//             break;
//         }else {
//             send_response(control_fd,"500 Unknown command.");
//         }

//?????????????????????

    //}
    
    // 关闭控制连接
    //close(control_fd);
//}

//???????????????????????????????????????


// class FTP{
//     public:
//     FTP(){


//     }
    


//     }
//     ~FTP(){
//         close(epdf);
//     }
    

// void handle_PASV(struct FtpClient* client) {

// 	if (client->_data_socket > 0) {
// 		close(client->_data_socket);
// 		client->_data_socket = -1;
// 	}
// 	if (client->_data_server_socket > 0) {
// 		close(client->_data_server_socket);
// 	}
// 	client->_data_server_socket = socket(AF_INET, SOCK_STREAM, 0);
// 	if (client->_data_server_socket < 0) {
// 		perror("opening socket error");
// 		send_msg(client->_client_socket, "426 pasv failure\r\n");
// 		return;
// 	}
// 	struct sockaddr_in server;
// 	server.sin_family = AF_INET;
// 	server.sin_addr.s_addr = inet_addr(client->_ip);
// 	server.sin_port = htons(0);
// 	if (bind(client->_data_server_socket, (struct sockaddr*) &server,
// 			sizeof(struct sockaddr)) < 0) {
// 		perror("binding error");
// 		send_msg(client->_client_socket, "426 pasv failure\r\n");
// 		return;
// 	}
// 	show_log("server is estabished. Waiting for connnect...");
// 	if (listen(client->_data_server_socket, 1) < 0) {
// 		perror("listen error...\r\n");
// 		send_msg(client->_client_socket, "426 pasv failure\r\n");
// 	}
// 	struct sockaddr_in file_addr;
// 	socklen_t file_sock_len = sizeof(struct sockaddr);
// 	getsockname(client->_data_server_socket, (struct sockaddr*) &file_addr,
// 			&file_sock_len);
// 	show_log(client->_ip);
// 	int port = ntohs(file_addr.sin_port);
// 	show_log(parseInt2String(port));
// 	char* msg = _transfer_ip_port_str(client->_ip, port);
// 	char buf[200];
// 	strcpy(buf, "227 Entering Passive Mode (");
// 	strcat(buf, msg);
// 	strcat(buf, ")\r\n");
// 	send_msg(client->_client_socket, buf);
// 	free(msg);

// }

// 控制连接线程
void handle_msg(int fd,char *buf,class ConnectionGroup group){


        while(1){
        if(*buf=="PASV"){
            handle_pasv(fd);
        }else if(*buf=="LIST"){
            handle_list(fd);
        }else if(*buf=="RETR"){
            handle_retr(fd);
        }else if(*buf=="STOR"){
            handle_stor(fd);
        }else if(*buf=="QUIT"){
            send_response(fd,"221 Goodbye.");
            break;
        }else{
            send_response(fd,"500 Unknown command.");
        }    

    }



}



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
    
void FTP_start(class ConnectionGroup group){
    while(1){
        struct epoll_event events[maxevents];
        int n=epoll_wait(epfd,events,maxevents,-1);
        if(n==-1){
            perror("epoll_wait failed");
            break;
        }
        for(int i=0;i<n;i++){
            if(events[i].data.fd==server_fd){ // 客户端连接
                sockaddr_in client_addr{};
                socklen_t client_len=sizeof(client_addr);
                int connect_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);
                if(connect_fd==-1){
                    perror("accept");
                    continue;
                }

                // 设置非阻塞模式
                fcntl(connect_fd,F_SETFL,fcntl(connect_fd,F_GETFL,0)|O_NONBLOCK);

                // 注册客户端连接到 epoll
                ev.events=EPOLLIN|EPOLLET;
                ev.data.fd=connect_fd;
                if(epoll_ctl(epfd,EPOLL_CTL_ADD,connect_fd,&ev)==-1){
                    perror("epoll_ctl");
                    close(connect_fd);
                }
            }else{    // 客户端
                if(events[i].events&EPOLLIN){// 处理可读事件
                    char buf[1024];
                    while(1){
                    // memset(buf,0,sizeof(buf));
                    ssize_t len=read(events[i].data.fd,buf,sizeof(buf));
                        if(len<=0){
                            if(len==0){ // 客户端主动关闭
                                printf("Client disconnected: %d\n",events[i].data.fd);
                            }else{ // 连接出错
                                perror("read failed");
                            }
                            close(events[i].data.fd);
                            epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr);
                        }else{
                            

                        
                            int fd=events[i].data.fd;
                            std::thread client_thread(handle_msg,fd,buf,group);
                            //control_pool.enqueue(handle_client,client_fd);
                            //?????


                            // 线程中要实现

                            client_thread.detach();
                
                            // 创建线程？
                            // handle_msg(buf);
                            //写个处理函数处理buf数组
                            // ？？？？？



                
                            //控制连接和数据连接的实现与绑定
                        }
                    }
                }
                if(events[i].events&EPOLLOUT){// 处理可写事件
                    ;
                }
            }
            // free???
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
