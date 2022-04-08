#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>
using namespace std;
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg):
    baseLoop_(baseLoop),
    name_(nameArg),
    started_(false),
    numThreads_(0),
    next_(0) //轮循分配channel给EventLoopThreadPool中的EvenLoopThread的下标
{

}
EventLoopThreadPool::~EventLoopThreadPool()
{
    //我们不需要去析构EventLoop对象，因为EventLoopThread对象，因为EventLoopThread对象是在栈上创建的，结束后会自动删除
}
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;
    for(int i = 0; i < numThreads_; i++)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d",name_.c_str(),i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        //其实这个EventLoopThread只提供了一些控制EventLoop线程的方法，而EventLoop对象的创建
        //则是在EventLoopThread::threadFunc()函数内创建的，当EventLoopThread析构了，
        //局部对象EvetLoop对象就会从栈中删除。
        loops_.push_back(t->startLoop());
    }
    if(numThreads_ == 0)
    {
        //整个服务端只有一个线程运行着baseLoop_;
        cb(baseLoop_);
    }   
}

//如果是工作在多线程中，baseLiio_默认以轮循的方式分配channel给subloop
//当然还有一个哈希的定制方式分配channel，这里就暂时不写他了
EventLoop* EventLoopThreadPool::getNextLoop()
{
    /**
     * 这个很好理解，就是普通的轮循而已
     */
    EventLoop *loop = baseLoop_;
    if(!loops_.empty())
    {
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size())
            next_ = 0;
    }
    return loop;
    
}

vector<EventLoop*> EventLoopThreadPool::getAllGroups()
{
    if(loops_.empty())
        return std::vector<EventLoop*> {baseLoop_};
    else
        return loops_;
}