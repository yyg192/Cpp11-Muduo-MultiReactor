#include "Acceptor.h"
#include <sys/socket.h>
#include <sys/types.h>
#include "Logger.h"
#include <unistd.h>
#include "InetAddress.h"
/***

 */

static int createNonblocking() 
{
    /**
     * @brief 创建一个非阻塞的IO
     * 
     */
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK| SOCK_CLOEXEC, IPPROTO_TCP);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__,errno);
    }
    return sockfd;
    
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    :loop_(loop)
    ,acceptSocket_(createNonblocking()) //可以看出一个服务端一个acceptSocket_了
    ,acceptChannel_(loop, acceptSocket_.fd())//这里的loop是baseLoop_
    ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true); //设置socket选项  
    acceptSocket_.setReusePort(true); //设置socket选项
    acceptSocket_.bindAddress(listenAddr); //bind
    // TcpServer::start() Acceptor.listen 有新用户连接 执行一个回调 connfd => channel => subloop
    // baseLoop_ 监听到Accpetor有监听事件，baseLoop_就会帮我们新客户连接的回调函数
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}
void Acceptor::listen()
{
    /**
     * @brief 开启对server socket fd的可读事件监听
     * 
     */
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    //server socket fd 有读事件发生了，即有新用户连接了，就会调用这个handleRead
    InetAddress peerAddr;
    //当有新用户连接了又会调用Socket::accpet函数，该函数底层真正调用了sccket编程的accept函数
    //并且把peerAddr设置好后就传递给newConnectionCallback_来调用真正的新用户连接的处理函数。
    int connfd = acceptSocket_.accept(&peerAddr);
    
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
            newConnectionCallback_(connfd, peerAddr);//轮循找到subloop，唤醒，然后分发当前客户端的channel
        else
            ::close(connfd); //其实这个几乎不会被执行
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE) //进程的fd已用尽
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}