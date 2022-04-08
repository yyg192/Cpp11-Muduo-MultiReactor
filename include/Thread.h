#pragma once
#include "noncopyable.h"
#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : muduo::noncopyable
{ //只关注一个线程
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc, const std::string &name = std::string());//这个写法我倒是少见
    ~Thread();
    void start();
    void join();

    bool started() const{return started_;}
    pid_t tid() const {return tid_;} //muduo库上返回的tid相当于linux上用top命令查出来的tid，不是pthread的tid。
    static int numCreated(){return numCreated_;}
    
    
private:
    void setDefaultName();
    bool started_;
    bool joined_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    std::shared_ptr<std::thread> thread_;//注意这里，如果你直接定义一个thread对象，
    //那这个线程就直接开始运行了，所以这里定义一个智能指针，在需要运行的时候再给他创建对象。
    static std::atomic<int> numCreated_;
};