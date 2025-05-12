#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(){
    
    return 0;
}

int main(int arg, char *argv[]){
    int ser_sockfd;
    int cli_sockfd;
    int ser_len;
    int cli_len;
    int ser_port;
    struct sockaddr_in ser_addr;
    struct sockaddr_in cli_addr;
    char commd [N];
    system_get_visitors();
    getcwd(system_dir,sizeof(system_dir));

    bzero(commd,N);//将commd所指向的字符串的前N个字节置为0，包括'\0'
    bzero(&ser_addr,sizeof(ser_addr));

    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(8989);
    ser_len = sizeof(ser_addr);

    pthread_t system_thread_id;
    pthread_create(&system_thread_id,NULL,(void *)&system_commd,NULL);
    //创建socketfd
    if((ser_sockfd=socket(AF_INET, SOCK_STREAM, 0) ) < 0){
        printf("Sokcet Error!\n");
        return -1;
    }

    //将ip地址与套接字绑定
    if((bind(ser_sockfd, (struct sockaddr*)&ser_addr, ser_len)) < 0){
        printf("Bind Error!\n");
        return -1;
    }

    //服务器端监听
    if(listen(ser_sockfd, 5) < 0){
        printf("Linsten Error!\n");
        return -1;
    }
    
    bzero(&cli_addr, sizeof(cli_addr));
    ser_len = sizeof(cli_addr);
 
    while(1){
        pthread_t thread_id;

        if((cli_sockfd=accept(ser_sockfd, (struct sockaddr*)&cli_addr, &cli_len)) < 0){
            printf("Accept Error!\n");
            exit(1);
        }
        NULL,(void *)&system_accept,(void *)&cli_sockfd)==-1){ 
            printf("pthread_create error!\n");
            exit(1);
        }        
    }
    return 0;
}