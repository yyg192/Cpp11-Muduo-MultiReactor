#pragma once
#include "noncopyable.h"
#include <vector>
#include <unordered_map>
#include "Channel.h"
class Channel;
class EventLoop;
class Poller : public muduo::noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop *loop);
    virtual ~Poller();

    //给所有io复用保留统一的接口，当前激活的channels，需要poller去循查的channel(fd)
    virtual TimeStamp poll(int timeoutMs, ChannelList *ativateChannels) = 0;
    
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;
    
    bool hasChannel(Channel *channel) const; //判断一个poller里面有没有这个channel
    
    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_; 
};
