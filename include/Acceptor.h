#pragma once
#include "noncopyable.h"
#include "Channel.h"
#include "Socket.h"
#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : public muduo::noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        //设置连接事件发生的回调函数。
        newConnectionCallback_ = cb; //当有连接事件发生的时候调用newConnectionCallback_
    }
    bool listenning() const {return listenning_;}
    void listen();
private:
    void handleRead();

    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_; //
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};