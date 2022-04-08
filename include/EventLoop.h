#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include "noncopyable.h"
#include "Timestamp.h"
#include <memory>
#include <mutex>
#include "CurrentThread.h"
class Channel;
class Poller;

//事件循环类， 主要包含了两大模块，channel 和 poller
class EventLoop : public muduo::noncopyable
{
public:
    using Functor = std::function<void()>; 
    EventLoop();
    ~EventLoop();
    void loop(); //开启事件循环
    void quit(); //关闭事件循环
    TimeStamp poolReturnTime() const {return pollReturnTime_;}
    void runInLoop(Functor cb); //mainReactor用于唤醒Subreactor的
    void queueInLoop(Functor cb); //
    void wakeup();
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);
    
    //判断当前的eventloop对象是否在自己的线程里面
    bool isInLoopThread() const {return threadId_ == CurrentThread::tid();}

private:
    void handleRead(); //处理唤醒相关的逻辑。
    void doPendingFunctors();//执行回调的

    using ChannelList = std::vector<Channel*>;
    std::atomic<bool> looping_; //标志进入loop循环
    std::atomic<bool> quit_; //标志退出loop循环 这个和looping_ 其实本质上有重叠
    std::atomic<bool> callingPendingFunctors_; //标识当前loop是否有需要执行回调操作
    const pid_t threadId_; //当前loop所在的线程的id
    TimeStamp pollReturnTime_; //poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_; //一个EventLoop需要一个poller，这个poller其实就是操控这个EventLoop的对象。
    //统一事件源
    int wakeupFd_; //主要作用，当mainLoop获取一个新用户的channel通过轮询算法选择一个subloop(subreactor)来处理channel。
    std::unique_ptr<Channel> wakeupChannel_;
    ChannelList activeChannels_;
    Channel *currentActiveChannel_;
    std::vector<Functor> pendingFunctors_; //存储loop需要执行的所有回调操作。
    std::mutex mutex_; //保护上面vector容器的线程安全操作。
};
