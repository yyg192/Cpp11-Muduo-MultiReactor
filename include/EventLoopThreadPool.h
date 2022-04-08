#pragma once
#include "noncopyable.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool:public muduo::noncopyable
{
    /*
        管理EventLoopThread的线程池
    */
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads){numThreads_ = numThreads;}
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //如果是工作在多线程中，baseLiio_默认以轮循的方式分配channel给subloop
    //当然还有一个哈希的定制方式分配channel，这里就暂时不写他了
    EventLoop* getNextLoop();
    std::vector<EventLoop*> getAllGroups();
    bool started() const {return started_;}
    const std::string name() const {return name_;}
    
private:
    EventLoop *baseLoop_; //如果你没有通过setThreadNum来设置线程数量，那整个网络框架就只有一个
    //线程，这唯一的一个线程就是这个baseLoop_，既要处理新连接，还要处理已连接的事件监听。
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_; 
    std::vector<EventLoop*> loops_; //包含了所有EventLoop线程的指针
    
    
};