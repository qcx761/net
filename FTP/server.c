#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bits/pthreadtypes.h>

#define PORT 2100
#define PORT1 5000
#define SIZE 1024

void handle_client(int control_socket){
    char buffer[SIZE];
    
    while(1){
        // 清空缓冲区
        memset(buffer,0,sizeof(buffer));
        int received;
        if((received=recv(control_socket,buffer,sizeof(buffer)-1,0))<=0){
            break;
        }
        


        // 处理 PASV 命令
        if(strncmp(buffer,"PASV",4)==0){
            // 生成数据端口并回复客户端
            int h1=192,h2=168,h3=1,h4=1;
            int p1=PORT1/256;
            int p2=PORT1%256;
            sprintf(buffer,"227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n", h1, h2, h3, h4, p1, p2);
            send(control_socket, buffer, strlen(buffer),0);
        }
        
        





    }
    
    // 关闭控制连接
    close(control_socket);
}

int main(){
    int server_fd;
    struct sockaddr_in ser_addr;
    socklen_t ser_len;

    memset(&ser_addr,0,sizeof(ser_addr));
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

    while (1){
        int client_sock=accept(server_fd, NULL, NULL);
        pthread_t thread_id;
        if(pthread_create(&thread_id,NULL,handle_client,(void *)&client_sock)!=0){
            perror("线程创建失败");
        }
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}