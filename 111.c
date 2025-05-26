


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