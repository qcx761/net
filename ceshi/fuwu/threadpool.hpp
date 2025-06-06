#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP
#include <bits/stdc++.h>
using namespace std;
class threadpool {
    public:
    threadpool(size_t num):stop(false){
        for(int i=0;i<num;i++){
            this->workers.emplace_back([this]{
                while(1){
                    function<void()> task;
                    {
                    unique_lock<mutex> lock(this->mtx);
                    this->condition.wait(lock,[this]{return !this->tasks.empty()||this->stop;});
                    if(stop&&this->tasks.empty()) return;
                    task=this->tasks.front();
                    this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    ~threadpool(){
        {
            unique_lock<mutex> lock(this->mtx);
            this->stop=true;
        }
        this->condition.notify_all();
        for(int j=0;j<this->workers.size();j++){
            workers[j].join();
        }
    }
    
    template<typename F,class... Args>
auto enqueue(F&& f,Args&&... args) -> future<invoke_result_t<F,Args...>>{
    using return_type=invoke_result_t<F,Args...>;
     // 绑定函数和参数，创建packaged_task
    auto task=make_shared<packaged_task<return_type()>>(bind(forward<F>(f),forward<Args>(args)...));
    future<return_type> res=task->get_future();
    {
        unique_lock<mutex> lock(this->mtx);

        if(this->stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // 将任务包装为void()类型
        this->tasks.emplace([task]{(*task)();});
    }
    this->condition.notify_one();
    return res;
}
    
    private:
        vector<thread> workers; // 线程池中的线程
        queue<function<void()>> tasks; // 任务队列
        condition_variable condition; // 条件变量
        mutex mtx; // 互斥量
        bool stop; // 停止标志
    };

#endif





//queue
//vector
//function


        //lambda 表达式
        // [capture](parameters) -> return_type {
        //     // function body
        // }
        // capture：指定如何捕获外部变量，可以为空或包含变量。
        // parameters：参数列表，类似于函数的参数。
        // -> return_type：可选，指定返回类型（通常可以省略，编译器会进行推导）。
        // {}：函数体。