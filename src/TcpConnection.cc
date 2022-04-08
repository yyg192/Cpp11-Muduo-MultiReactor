#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <string>
using namespace std;
using namespace std::placeholders;
static EventLoop* CheckLoopNotNull(EventLoop* loop) //这里定义为static怕TcpConnection和TcpServer的这个函数产生冲突
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null \b", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string &nameArg, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnected)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024) //64M
{
    //给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作
    channel_->setReadCallback(bind(&TcpConnection::handleRead, this, _1));
    channel_->setWriteCallback(bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::creator[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::deletor[%s] at fd=%d state=%d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const string &buf)//发送数据 注意留意一下这个强制转换
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        { //其实在我们这个通信框架里面，因为TcpConnection的Channel肯定是注册到
          //一个EventLoop里面，这个send肯定是在对应的EventLoop线程里面执行的
          //不过有一些应用场景可能会把connection记录下来，在其他线程调用connection的send这也是有可能的
          sendInLoop(buf.c_str(), buf.size());
        }
        else{
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop,
                                        this,
                                        buf.c_str(),
                                        buf.size()));
        }
    }
}
void TcpConnection::sendInLoop(const void* data, size_t len)
{   //将Buffer的数据
    //发送数据 应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
    ssize_t nwrote = 0;
    size_t remaining = len; //还没发送的数据的长度
    bool faultError = false; //记录是否产生错误
    if(state_ == kDisconnected)
    {
        //之前调用过该connection的shutdown，不能再进行发送了
        LOG_ERROR("disconnected, give up writing");
        return ;
    }
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        //channel第一次开始写数据，而且用户空间的发送缓冲区中还没有待发送数据
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {   
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {//既然一次性发送完了就不用再给channel设置epollout事件了，即不用设置enableWriting
            //这样epoll_wait就不用监听可写事件并且执行handleWrite了。这算是提高效率吧
                loop_->queueInLoop(bind(writeCompleteCallback_, shared_from_this()));
                //发送完数据了就调用发送完后的处理函数吧
            }
        }
        else
        { //一次性没法把data全部拷贝到socket发送缓冲区中
            nwrote = 0; //
            if(errno != EWOULDBLOCK) 
            {//如果是非阻塞模式，socket发送缓冲区满了就会立即返回并且设置EWOULDBLOCK
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) //接收到对端的socket重置
                    faultError = true;
            }
        }
    }
    if(!faultError && remaining > 0)
    {
        //说明刚才的write没有把数据全部拷贝到socket发送缓冲区中，剩余的数据需要保存到用户的outputBuffer缓冲区当中。
        //然后给channel注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock channel
        //调用handleWrite回调方法，把发送缓冲区中的数据全部发送完成。
        size_t oldLen = outputBuffer_.readableBytes();//目前发送缓冲区Buffer剩余的待发送数据(待拷贝到socket缓冲区)的长度
        if(oldLen +remaining >= highWaterMark_
            && oldLen < highWaterMark_ //???????????
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
            channel_->enableWriting();//这里一定要注册channel的写事件，否则poller不会给channel通知pollout
    }
}
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting); //这里设置成kDisconnecting是怕你缓冲区还有数据没发完
        //在handleWrite函数中，如果发送完数据会检查state_是不是KDisconnecting，如果是就设为KDisconnected
        //
        loop_->runInLoop(bind(&TcpConnection::shutdownInLoop, this));
    }
    
    
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())
    {
        //说明当前outputBuffer中的数据全部发送完成
        socket_->shutdownWrite(); //关闭写端 触发Channel的EPOLLHUP
        
    }
}


// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    //tie(const std::shared_ptr<void> &obj);
    //shared_from_this()返回的是shared_ptr<TcpConnection>，这能被tie函数接受？
    //这个shared_ptr<void>实际上底层就是一个void*指针，而shared_ptr<TcpConnection>
    //实际上是一个TcpConnection*指针
    //Channel类里面有一个weak_ptr会指向这个传进来的shared_ptr<TcpConnection>
    //如果这个TcpConnection已经被释放了，那么Channel类中的weak_ptr就没办法在
    //Channel::handleEvent 就没法提升weak_ptr。
    /**
     * @brief 我倒是觉得这个思想很不错，当你的TcpConnection对象以shared_ptr的形式
     * 进入到Channel中被绑定，然后Channel类通过调用handleEvent把Channel类中的
     * weak_ptr提升为shared_ptr共同管理这个TcpConnection对象，这样的话，即使
     * 外面的这个TcpConnection智能指针被释放析勾删除，Channel类里面还有一个智能指针
     * 指向这个对象，这个TcpConnection对象也不会被释放。因为引用计数没有变为0.
     * 这个思想超级好，防止你里面干得好好的，外边却突然给你釜底抽薪
     * 
     */
    channel_->enableReading(); //向poller注册channel的epollin事件
    //新连接建立，执行回调
    connectionCallback_(shared_from_this());

}



void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int savedErrno = 0; 
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);//这里的channel的fd也一定仅有socket fd
    if(n > 0) //从fd读到了数据，并且放在了inputBuffer_上
    {
        //已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        //这个shared_from_this()就是TcpConnection对象的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n == 0)
        handleClose();
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting()) //当前感兴趣的事件是否包含可写事件？？？
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);//通过fd发送数据
        if(n > 0) //n > 0说明向Buffer写入成功，Buffer是要发出去给socket的数据
        {
            outputBuffer_.retrieve(n);//如果成功写入了，就要一定一下readerIndex;我觉得把这句话封装进writeFd更好吧
            if(outputBuffer_.readableBytes() == 0)
            { //如果说Buffer里面已经没有数据了，全都拷贝到发送缓冲区了
                channel_->disableWriting(); //那么就关闭这个channel的可写事件，
                //我推测，当Buffer又有数据了，就启动可写事件监听，这样做有一个好处，就是
                //尽可能的将epoll_wait留给可读关闭等事件的监听，不会频繁触发可写事件，提高效率！
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );//queueInLoop，唤醒这个loop对应的thread线程来执行回调，其实我觉得这里也可以是runInLoop，的确也可以是
                }
                if(state_ == kDisconnecting) //正在关闭中
                {
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else{
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n",channel_->fd(), static_cast<int>(state_));
    setState(kDisconnected);
    channel_->disableAll();
    TcpConnectionPtr connPtr(shared_from_this());

    //其实我觉得这里应该加入判断条件，万一没设置执行关闭连接的回调函数怎么办
    connectionCallback_(connPtr); //执行连接关闭的回调 ？？？？？？？？？？？？？？？？？？？？？？
    closeCallback_(connPtr); //关闭连接
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;

    //《Linux高性能服务器编程》page88，获取并清除socket错误状态,getsockopt成功则返回0,失败则返回-1
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen)<0)
        err = errno;
    else
        err = optval;
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);

}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();//把channel从poller中删除掉。
}

