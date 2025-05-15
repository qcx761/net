#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <thread>
#include "threadpool.hpp"

using namespace std;

#define PORT 2100
#define PORT1 5000
#define SIZE 1024

threadpool pool(10);


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


void handle_client(int control_socket){
    char buffer[SIZE];
    
    while(1){
        memset(buffer,0,sizeof(buffer));
        int received;
        if((received=recv(control_socket,buffer,sizeof(buffer)-1,0))<0){
            perror("recv failed");
        }        
        // if(buffer=="PASV"){
        //     handle_pasv_command(control_fd, data_pool);
        // }else if(buffer=="LIST"){
        //     handle_list_command(control_fd, data_pool);
        // }else if(buffer=="RETR"){
        //     handle_retr_command(control_fd, data_pool);
        // }else if(buffer=="STOR"){
        //     handle_stor_command(control_fd, data_pool);
        // }else if(buffer=="QUIT"){
        //     send_response(control_fd, "221 Goodbye.");
        //     break; // 退出循环
        // }else {
        //     send_response(control_fd, "500 Unknown command.");
        // }


        
        
        



    }
    
    // 关闭控制连接
    close(control_socket);
}

int main(){
    int server_fd,client_fd;
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

    

    while(1){
        if((client_fd=accept(server_fd,(struct sockaddr*)&cli_addr,&cli_len))<0){
            printf("Accept Error!\n");
            exit(1);
        }
        std::thread client_thread(handle_client, client_fd);
        //pool.enqueue(handle_client,client_fd);
        //？
        client_thread.detach();



//？？？？？？？？？？？？？？？？？？？？？？？？？？？？



    }
    

    close(server_fd);
    return 0;
}

// 线程池设计
// 控制连接线程池:

// 用于处理客户端的控制连接和命令（如 USER、PASS、PASV 等）。
// 每当有新的控制连接时，从这个线程池中获取一个线程来处理。
// 数据连接线程池:

// 用于处理数据传输（如文件上传和下载）。
// 当在控制连接中接收到 PASV 命令并建立数据连接后，从这个线程池中获取一个线程来处理数据传输。
