/***
 EventLoopThread类绑定了一个EventLoop和一个线程
 */

#pragma once
#include "noncopyable.h"
#include <functional>
#include <string>
#include <mutex>
#include "Thread.h"
#include <condition_variable>
class EventLoop;

class EventLoopThread : public muduo::noncopyable
{
public:
    using ThreadInitCallback = std::function<void (EventLoop*)>;
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const std::string &name = std::string());
    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();
    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};
