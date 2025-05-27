


class ConnectionGroup{
    public:
        std::vector<ControlConnect> control_connections;
        std::vector<DataConnect> data_connections;
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
















void remove_control_connection(int fd) {
    auto it=std::find_if(control_connections.begin(),control_connections.end(),[fd](const ControlConnect& conn){return conn.control_fd==fd;});

    if(it!=control_connections.end()){
        control_connections.erase(it);  // 删除单个元素
    }

    // 清理data_to_control中指向该control_fd的映射
    for(auto it=data_to_control.begin();it!=data_to_control.end();){
        if(it->second==control_fd){
            it=data_to_control.erase(it);  // 删除并返回下一个迭代器
        }else{
            ++it;
        }
    }
}

















#include <vector>
#include <unordered_map>
#include <iostream>

// 假设 ControlConnect 和 DataConnect 类的定义如下
class ControlConnect {
public:
    // 默认构造函数
    ControlConnect() : fd(-1) {}  // -1 表示无效的 fd

    // 带参数的构造函数
    ControlConnect(int fd) : fd(fd) {}

    // 获取 fd 的方法
    int getFd() const { return fd; }

private:
    int fd;
};

class DataConnect {
public:
    // 默认构造函数
    DataConnect() : fd(-1) {}  // -1 表示无效的 fd

    // 带参数的构造函数
    DataConnect(int fd) : fd(fd) {}

    // 获取 fd 的方法
    int getFd() const { return fd; }

private:
    int fd;
};

// ConnectionGroup 类，包含 ControlConnect 和 DataConnect
class ConnectionGroup {
public:
    std::vector<ControlConnect> control_connections;
    std::vector<DataConnect> data_connections;
    std::unordered_map<int, int> data_to_control;  // key: data_fd, value: control_fd

    // 添加连接（control_fd 和 data_fd 关联）
    void add_connection(int control_fd, int data_fd) {
        control_connections.emplace_back(control_fd);
        data_connections.emplace_back(data_fd);
        data_to_control[data_fd] = control_fd;  // 存储反向映射
    }

    // 通过 data_fd 查找 control_fd
    int find_control_fd(int data_fd) const {
        auto it = data_to_control.find(data_fd);
        if (it != data_to_control.end()) {
            return it->second;
        }
        return -1;  // 未找到
    }

    // 通过 control_fd 查找 ControlConnect 对象的索引
    int find_control_connect_index(int control_fd) const {
        for (size_t i = 0; i < control_connections.size(); ++i) {
            if (control_connections[i].getFd() == control_fd) {
                return static_cast<int>(i);
            }
        }
        return -1;  // 未找到
    }

    // 通过 data_fd 查找 DataConnect 对象的索引
    int find_data_connect_index(int data_fd) const {
        for (size_t i = 0; i < data_connections.size(); ++i) {
            if (data_connections[i].getFd() == data_fd) {
                return static_cast<int>(i);
            }
        }
        return -1;  // 未找到
    }

    // 通过 control_fd 获取 ControlConnect 对象的引用（确保存在）
    const ControlConnect& get_control_connect(int control_fd) const {
        int index = find_control_connect_index(control_fd);
        if (index != -1) {
            return control_connections[index];
        }
        throw std::out_of_range("ControlConnect not found");
    }

    // 通过 data_fd 获取 DataConnect 对象的引用（确保存在）
    const DataConnect& get_data_connect(int data_fd) const {
        int index = find_data_connect_index(data_fd);
        if (index != -1) {
            return data_connections[index];
        }
        throw std::out_of_range("DataConnect not found");
    }
};




char recvbuf[MAXBUF];
        char sendbuf[MAXBUF];
        ssize_t n;
        ctrl_args *new_arg = (ctrl_args*) args;

        while ((n = recv(new_arg->fd, recvbuf, MAXBUF - 1, MSG_DONTWAIT)) != 0){ 
            recvbuf[n] = '\0';
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                n = 0;
                break;
            }
        }

        if (n == 0) {
            if(strcmp(recvbuf,"PASV") == 0){
                char portnum_str[MAXBUF];
                char *result = new char[MAXBUF];
                sockaddr_storage addr;
                socklen_t len = sizeof(sockaddr_storage);

                srand(time(NULL));
                int listen_data_portnum = rand()%40000+1024;
                sprintf(portnum_str,"%d",listen_data_portnum);
                int listen_data_fd = inetlisten((const char*)portnum_str);
                while(listen_data_fd == -1){
                    perror("inetlisten");
                    listen_data_portnum = rand()%40000+1024;
                    sprintf(portnum_str,"%d",listen_data_portnum);
                    listen_data_fd = inetlisten((const char*)portnum_str);
                }
                set_nonblocking(listen_data_fd);

                struct epoll_event ev;
                ev.data.fd = listen_data_fd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(new_arg->epfd,EPOLL_CTL_ADD,listen_data_fd,&ev);
                
                getsockname(listen_data_fd,(sockaddr*)&addr,&len);
                address_str_portnum(result,MAXBUF,(sockaddr*)&addr,len) == NULL;
                const char delimiter[5] = "().,";
                char *token;
                char **res_token = new char*[10];
                for(int i=0;i<10;i++){
                    res_token[i] = new char[10];
                }
                int cnt = 0;
                cout << result << endl;
                token = strtok(result,delimiter);
                while(token != NULL){
                    res_token[cnt] = token;
                    token = strtok(NULL,delimiter);
                    cnt++;
                }
                sprintf(result,"(%s,%s,%s,%s,%d,%d)",res_token[0],res_token[1],res_token[2],res_token[3],atoi(res_token[4])/256,atoi(res_token[4])%256);
                sprintf(sendbuf,"%s %s","227 entering passive mode",result);
                send(new_arg->fd,sendbuf,sizeof(sendbuf),0);

                memset(sendbuf,0,sizeof(sendbuf));
                memset(recvbuf,0,MAXBUF);
                delete[] res_token;
                free(result);
            }


















































            void handle_pasv_request(int client_fd, const std::string& client_ip) {
                // 1. 计算端口号（假设随机生成 5000）
                int port = 5000;
                uint8_t p1 = port / 256;
                uint8_t p2 = port % 256;
            
                // 2. 创建数据传输线程
                std::thread data_thread([client_ip, port]() {
                    // 2.1 监听数据端口
                    int data_fd = socket(AF_INET, SOCK_STREAM, 0);
                    sockaddr_in data_addr{};
                    data_addr.sin_family = AF_INET;
                    data_addr.sin_port = htons(port);
                    inet_pton(AF_INET, client_ip.c_str(), &data_addr.sin_addr);
            
                    bind(data_fd, (sockaddr*)&data_addr, sizeof(data_addr));
                    listen(data_fd, 1);
            
                    // 2.2 等待客户端连接（简化版）
                    sockaddr_in client_data_addr{};
                    socklen_t client_len = sizeof(client_data_addr);
                    int client_data_fd = accept(data_fd, (sockaddr*)&client_data_addr, &client_len);
            
                    // 2.3 处理数据传输（如文件传输）
                    // ...
                });
            
                data_thread.detach();  // 或者使用线程池管理
            
                // 3. 返回 227 响应
                std::string response = "227 entering passive mode (" +
                                       client_ip + "," +
                                       std::to_string(p1) + "," +
                                       std::to_string(p2) + ")\r\n";
                send(client_fd, response.c_str(), response.size(), 0);
            }





                char portnum_str[MAXBUF];
                char* result = new char[MAXBUF];
                sockaddr_storage addr;
                socklen_t len = sizeof(sockaddr_storage);
            
                // 1. 随机选择数据端口（避免冲突）
                int listen_data_portnum;
                int max_retries = 10;
                int retries = 0;
                int listen_data_fd = -1;
            
                while (listen_data_fd == -1 && retries < max_retries) {
                    srand(time(NULL) + retries);  // 增加随机性
                    listen_data_portnum = rand() % 40000 + 1024;
                    sprintf(portnum_str, "%d", listen_data_portnum);
                    listen_data_fd = inetlisten(portnum_str);
                    retries++;
                }
            
                if (listen_data_fd == -1) {
                    send(new_arg->fd, "425 Cannot open data connection.\r\n", 30, 0);
                    delete[] result;
                    return;
                }
            
                set_nonblocking(listen_data_fd);
            
                // 2. 注册到 epoll
                struct epoll_event ev;
                ev.data.fd = listen_data_fd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(new_arg->epfd, EPOLL_CTL_ADD, listen_data_fd, &ev);
            
                // 3. 获取本地 IP 和端口
                if (getsockname(listen_data_fd, (sockaddr*)&addr, &len) == -1) {
                    perror("getsockname");
                    send(new_arg->fd, "425 Cannot get local address.\r\n", 32, 0);
                    close(listen_data_fd);
                    delete[] result;
                    return;
                }
            
                if (address_str_portnum(result, MAXBUF, (sockaddr*)&addr, len) == NULL) {
                    send(new_arg->fd, "425 Cannot format address.\r\n", 28, 0);
                    close(listen_data_fd);
                    delete[] result;
                    return;
                }
            
                // 4. 构造 227 响应
                const char delimiter[5] = "().,";
                char* token;
                char** res_token = new char*[10];
                for (int i = 0; i < 10; i++) {
                    res_token[i] = new char[10];
                }
                int cnt = 0;
                token = strtok(result, delimiter);
                while (token != NULL && cnt < 10) {
                    res_token[cnt] = token;
                    token = strtok(NULL, delimiter);
                    cnt++;
                }
            
                // 确保 res_token 有足够数据
                if (cnt < 5) {
                    send(new_arg->fd, "425 Invalid address format.\r\n", 29, 0);
                    for (int i = 0; i < 10; i++) delete[] res_token[i];
                    delete[] res_token;
                    close(listen_data_fd);
                    delete[] result;
                    return;
                }
            
                int p1 = atoi(res_token[4]) / 256;
                int p2 = atoi(res_token[4]) % 256;
            
                // 构造 227 响应
                char* formatted_response = new char[MAXBUF];
                snprintf(formatted_response, MAXBUF, "(%s,%s,%s,%s,%d,%d)", 
                         res_token[0], res_token[1], res_token[2], res_token[3], p1, p2);
            
                char* sendbuf = new char[MAXBUF * 2];  // 确保足够大
                snprintf(sendbuf, MAXBUF * 2, "227 entering passive mode %s", formatted_response);
            
                send(new_arg->fd, sendbuf, strlen(sendbuf), 0);
            
                // 清理
                for (int i = 0; i < 10; i++) delete[] res_token[i];
                delete[] res_token;
                delete[] formatted_response;
                delete[] sendbuf;
                memset(recvbuf, 0, MAXBUF);
            }












    int sockfd;
    struct sockaddr_in server_addr,actual_addr;
    socklen_t addr_len = sizeof(actual_addr);

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 绑定到所有可用的网络接口
    server_addr.sin_port = htons(port);       // 绑定到 8080 端口

    // 绑定套接字
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 获取实际绑定的地址
    if (getsockname(sockfd, (struct sockaddr *)&actual_addr, &addr_len) == -1) {
        perror("getsockname");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 打印绑定的 IP 和端口
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &actual_addr.sin_addr, ip, INET_ADDRSTRLEN);
    printf("Bound to IP: %s, Port: %d\n", ip, ntohs(actual_addr.sin_port));

    close(sockfd);
    return 0;







    // 数据连接创建
void handle_pasv(int client_fd){
    
    //     服务端控制线程接收到 PASV 请求后，创建一个数据传输线程，并将生成的端口号告知客户端控制线程，
    //返回 227 entering passive mode (h1,h2,h3,h4,p1,p2)，其中端口号为 p1*256+p2，IP 地址为 h1.h2.h3.h4。
    
    // 假设服务器的 IP 地址为 192.168.1.1，生成的端口号为 5000
    // 那么返回的响应将是：227 entering passive mode (192,168,1,1,19,136)
    // 其中 19 和 136 分别是 5000 的高位和低位字节（5000 = 19*256 + 136）
    
        
        srand(time(NULL));
        int port=rand()%40000+1024;
        int p1=port/256;
        int p2=port%256;
        char str[6];
        sprintf(str,"%d",port);

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

        struct epoll_event ev;
        ev.data.fd=listen_fd;
        ev.events=EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLERR;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&ev)==-1){
            perror("epoll_ctl failed");
            return;
        }

        socklen_t len=sizeof(addr);
        getsockname(listen_fd,(struct sockaddr*)&addr,&len);
        


        // struct sockaddr_storage addr;
        // socklen_t len=sizeof(sockaddr_storage);
        // getsockname(listen_fd,(sockaddr*)&addr,&len);
        // 用notify_one唤醒  哪里唤醒呢。。。
    
    
    
    
        // 创建线程
    
        //condition.wait(lock,[this]{return !this->tasks.empty()||this->stop;});
    
    
        // 在这里面查找控制连接的传递的参数吗
    
    }

    

        // 6. 构造 227 响应
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    
        unsigned char* bytes = (unsigned char*)&addr.sin_addr.s_addr;
        int h1 = bytes[0], h2 = bytes[1], h3 = bytes[2], h4 = bytes[3];
        int p1 = ntohs(addr.sin_port) / 256;
        int p2 = ntohs(addr.sin_port) % 256;
    
        char response[128];
        snprintf(response, sizeof(response),
                 "227 entering passive mode (%d,%d,%d,%d,%d,%d)\r\n",
                 h1, h2, h3, h4, p1, p2);
    
        // 7. 发送响应
        send(new_arg->fd, response, strlen(response), 0);
    
        // 8. 清理（如果需要）
    }




void handle_list(int data_fd) {
    DIR *dir;
    struct dirent *entry;
    char buffer[1024];

    // 打开当前目录（或指定目录）
    dir = opendir(".");
    if (!dir) {
        send(data_fd, "550 Failed to open directory.\r\n", 30, 0);
        return;
    }

    // 遍历目录并发送文件列表
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 获取文件信息（可选）
        struct stat statbuf;
        char path[256];
        snprintf(path, sizeof(path), "./%s", entry->d_name);
        lstat(path, &statbuf);

        // 格式化输出（类似 `ls -l`）
        snprintf(buffer, sizeof(buffer), "-rw-r--r-- 1 user group %ld %s %s\r\n",
                 (long)statbuf.st_size,
                 "Oct 10",  // 模拟日期（实际应使用 `strftime`）
                 entry->d_name);

        send(data_fd, buffer, strlen(buffer), 0);
    }

    closedir(dir);
    send(data_fd, "226 Transfer complete.\r\n", 24, 0);  // 传输完成
}