#pragma once
#include <memory>
#include "noncopyable.h"
#include <string>
#include <atomic>
#include "InetAddress.h"
#include "Callbacks.h"
#include "Timestamp.h"
#include "Buffer.h"
class Channel;
class EventLoop;
class Socket;

/**
原文链接：https://blog.csdn.net/breadheart/article/details/112451022 
1. 为什么要用enable_shared_from_this?
    * 需要在类对象的内部中获得一个指向当前对象的shared_ptr 对象
    * 如果在一个程序中，对象内存的生命周期全部由智能指针来管理。在这种情况下，
        要在一个类的成员函数中，对外部返回this指针就成了一个很棘手的问题。
2. 什么时候用？
    * 当一个类被 share_ptr 管理，且在类的成员函数里需要把当前类对象作为参数传给其他函数时，
        这时就需要传递一个指向自身的 share_ptr。
3. 效果：
    TcpConnection类继承 std::enable_shared_from_this ，则会为该类TcpConnection提供成员函数
    shared_from_this。TcpConnection对象 t 被一个为名为 pt 的 std::shared_ptr 类对象管理时，
    调用 T::shared_from_this 成员函数，将会返回一个新的 std::shared_ptr 对象，
    它与 pt 共享 t 的所有权。
*/


class TcpConnection : public muduo::noncopyable, public std::enable_shared_from_this<TcpConnection>
{
    /**
     * @brief TcpConnection主要用于打包成功连接的TCP连接通信链路。
     * TcpServer => Acceptor => 新用户连接，通过accept函数拿到connfd =》 TcpConnection设置回调 => Channel => Poller => Poller监听到事件 => Channel的回调操作
     */
public:
    TcpConnection(EventLoop* loop, const std::string &name, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();
    EventLoop* getLoop() const {return loop_;}
    const InetAddress& localAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const {return peerAddr_;}
    bool connected() const {return state_ == kConnected;}
    void send(const std::string &buf);
    void shutdown();
    const std::string& name() const { return name_;}
    void setConnectionCallback(const ConnectionCallback& cb) {connectionCallback_ = cb;}
    void setMessageCallback(const MessageCallback& cb) {messageCallback_ = cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {writeCompleteCallback_ = cb;}
    void setCloseCallback(const CloseCallback& cb){closeCallback_ = cb;}
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;}
    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();
private:
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state) {state_ = state;}
    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; //这里绝对不是baseLoop，因为TcpConnection都是在subLoop里面管理的
    const std::string name_;
    std::atomic<int> state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    
    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完后的回调
    HighWaterMarkCallback highWaterMarkCallback_;//发送和接收缓冲区超过水位线的话容易导致溢出，这是很危险的。
    CloseCallback closeCallback_;
    size_t highWaterMark_; //水位线
    Buffer inputBuffer_; //接收的缓冲区大小，我猜的
    Buffer outputBuffer_; //发送的缓冲区大小，我猜的

};