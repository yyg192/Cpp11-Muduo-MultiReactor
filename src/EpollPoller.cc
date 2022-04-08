#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
/* 下面三个常量值表示了一个channel的三种状态 */
// channel未添加到poller中
const int kNew = -1;  // channel的成员index_ = -1
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop* loop) 
    : Poller(loop),
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    //自己去man一下这个epoll_create
    /*
    epoll_create()创建一个epoll的事例，通知内核需要监听size个fd。size指的并不是最大的后备存储设备，
    而是衡量内核内部结构大小的一个提示。当创建成功后，会占用一个fd，所以记得在使用完之后调用close()，
    否则fd可能会被耗尽。
    Note:自从Linux2.6.8版本以后，size值其实是没什么用的，不过要大于0，因为内核可以动态的分配大小，
    所以不需要size这个提示了。
    int epoll_create1(int flag);它和epoll_create差不多，不同的是epoll_create1函数的参数是flag，
    当flag是0时，表示和epoll_create函数完全一样，不需要size的提示了。
    当flag = EPOLL_CLOEXEC，创建的epfd会设置FD_CLOEXEC
    当flag = EPOLL_NONBLOCK，创建的epfd会设置为非阻塞
    一般用法都是使用EPOLL_CLOEXEC。
    关于FD_CLOEXEC它是fd的一个标识说明，用来设置文件close-on-exec状态的。当close-on-exec状态为0时，调用exec时，
    fd不会被关闭；状态非零时则会被关闭，这样做可以防止fd泄露给执行exec后的进程。因为默认情况下子进程是会继承父进程
    的所有资源的。
     */
    if(epollfd_ < 0) //epoll_create创建失败则fatal error退出
        LOG_FATAL("epoll create error:%d \n", errno);
}

EpollPoller::~EpollPoller()
{
    close(epollfd_); //关闭文件描述符, unistd.h
}

/**  下面这些代码其实就是对epoll_ctl的一些封装  **/
void EpollPoller::updateChannel(Channel *channel)
{
    /**
     * @brief 先获取channel的状态，根据channel的状态调用update()，其实就是调用epoll_ctl注册或修改或删除这个channel
     * 对应的fd的在epoll中的监听事件。
     * 
     */
    ///Channel如果想注册事件的话，它是没办法直接访问Poller的，它要通过EventLoop的updateChannel来注册
    //channel update remove => EventLoop updateChannel reomveChannel =>Poller updateChannel removeChannel
    const int index = channel->index(); //获取当前channel的状态，刚创建还是已在EventLoop上注册还是已在EventLoop删除
    LOG_INFO("fd=%d events=%d index=%d \n",channel->fd(), channel->events(), index);
    if(index == kNew || index == kDeleted)
    {
        if(index == kNew) //这个channel从来都没有添加到poller中，那么就添加到poller的channel_map中
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded); //设置当前channel的状态
        update(EPOLL_CTL_ADD, channel); //注册新的fd到epfd中；
    }
    else //channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if(channel->isNoneEvent()) //这个channel已经对任何事件都不感兴趣了
        {
            update(EPOLL_CTL_DEL,channel); //从epfd中删除一个fd；
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel); //这个channel还是对某些事件感兴趣的，修改已经注册的fd的监听事件；
        }
    }
    
}
void EpollPoller::removeChannel(Channel *channel)
{
    /**
     * @brief 从epoll里面删掉这个channel，然后更改这个channel的状态，从Poller的channel_map删除这个channel
     * 
     */
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd); //这个__FUNCTION__是获取函数名
    
    int index = channel->index();
    if (index == kAdded)
        update(EPOLL_CTL_DEL, channel);
    
    channel->set_index(kNew);
    
}

// 填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

// 更新channel通道
void EpollPoller::update(int operation, Channel *channel)
{
    //这里主要就是根据operation: EPOLL_CTL_ADD MOD DEL来具体的调用epoll_ctl更改这个channel对应的fd在epoll上的监听事件
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events(); //events()函数返回fd感兴趣的事件。
    event.data.ptr = channel; //这个epoll_event.data.ptr是给用户使用的，
    //epoll不关心里面的内容。用户通过这个可以附带一些自定义消息。这个 epoll_data 会随着
    // epoll_wait 返回的 epoll_event 一并返回。那附带的自定义消息，就是ptr指向这个自定义消息。
    int fd = channel->fd();
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
    
}

TimeStamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels) //这里不用写override，已经声明过了。
{
    /**
     * @brief poll其实就是调用epoll或poll，然后向activeChannels填入有事件发生的channel！
     * 
     */
    LOG_DEBUG("func=%s => fd total count: %lu \n", __FUNCTION__, channels.size());
    
    //这个写法很有意思！因为我们的events_是一个vector，支持动态扩容，而这里又要传入数组首地址，而events_又是一个对象，
    //events_.begin()是迭代器，*events_.begin()代表第一个元素（对象）&*events.begin()就代表第一个元素的地址。
    //这种技巧好好学一下！
    int numEvents = epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    TimeStamp now(TimeStamp::now());
    if(numEvents > 0)
    {
        LOG_DEBUG("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()) //如果有活动的连接数量大于我们的events_能承载的数量，就要对events_扩容
        {
            events_.resize(events_.size() * 2);//反正我们用的是LT模式，那些没被添加进activeChannels的通道后面还会有机会被添加进来的。
        }
    }
    else if(numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else{
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;

}
    
