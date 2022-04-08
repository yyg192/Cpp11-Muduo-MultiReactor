#include "Poller.h"
#include <stdlib.h>
#include "EpollPoller.h"
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    //Poller是基类，基类不能引用派生类 PollPoller 或EpollPoller，所以这个
    //newDefaultPoller函数的实现必须不能写在Poller.cc里面，最好是专门写在
    //一个新的文件里面
    if(::getenv("MUDUO_USE_POLL")) //通过环境变量控制选择epoll还是poll
        return nullptr; //生成poll实例
    else
        return new EpollPoller(loop); //生成epoll实例
}