char* removeStars(char* s) {
    char* res = malloc(strlen(s) + 1);
    int len = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] != '*') {
            res[len++] = s[i];
        } else {
            len--;
        }
    }
    res[len] = '\0';
    return res;
}
int q(const void* a,const void* b){
    return *(int*)a-*(int*)b;
}

char* triangleType(int* nums, int numsSize) {
    qsort(nums,numsSize,sizeof(int),q);
    if (nums[0]+nums[1]<=nums[2]){
        return "none";
    }else if(nums[0]==nums[2]){
        return "equilateral";
    }else if(nums[0]==nums[1]||nums[1]==nums[2]){
        return "isosceles";
    }else{
        return "scalene";
    }
}

class ConnectionGroup {
    public:
        std::vector<ControlConnect> control_connections; // 存储 ControlConnect 实例
        std::vector<DataConnect> data_connections;       // 存储 DataConnect 实例
        std::map<int, int> connection_mapping;           // 存储控制和数据连接的映射
    
        // 添加控制连接的方法
        void add_control_connection(int control_fd) {
            control_connections.emplace_back(control_fd);
        }
    
        // 添加数据连接的方法
        void add_data_connection(int data_fd) {
            data_connections.emplace_back(data_fd);
        }
    
        // 关联控制连接和数据连接
        void associate_connections(int control_fd, int data_fd) {
            connection_mapping[control_fd] = data_fd; // 将控制 FD 和数据 FD 关联
        }
    
        // 打印所有连接信息
        void print_connections() {
            std::cout << "Control Connections:\n";
            for (const auto& conn : control_connections) {
                std::cout << "Control FD: " << conn.control_fd << std::endl;
    
                // 输出关联的数据连接
                auto it = connection_mapping.find(conn.control_fd);
                if (it != connection_mapping.end()) {
                    std::cout << "  Associated Data FD: " << it->second << std::endl;
                }
            }
    
            std::cout << "Data Connections:\n";
            for (const auto& conn : data_connections) {
                std::cout << "Data FD: " << conn.data_fd << std::endl;
            }
        }
    };