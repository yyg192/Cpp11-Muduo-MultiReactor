#pragma once
//TcpServere和使用网络库的程序员直接交互，最好把该包含的头文件都包含了，不然用户还要自己包含，
//就挺拉稀的
#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include <functional>
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include <string>
#include <atomic>
#include <memory>
#include <unordered_map>
class TcpServer : public muduo::noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    enum Option
    { //预置两个选项，是否对端口进行复用
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop* loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort); //默认不重用端口
    ~TcpServer();
    void setThreadInitcallback(const ThreadInitCallback &cb){
        threadInitCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb){
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback &cb){
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){
        writeCompleteCallback_ = cb;
    }
    void setThreadNum(int numThreads); //设置底层subloop个数
    void start(); //开启服务器监听
private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop *loop_; //baseLoop，用户自己定义的
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; //运行在baseLoop，任务就是监听新连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_; //one loop per thread
    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成的回调
    ThreadInitCallback threadInitCallback_; //loop线程初始化的回调
    
    std::atomic<int> started_;
    int nextConnId_;
    ConnectionMap connections_;  //保存所有的连接
    
};