#include "EventLoop.h"
#include <sys/eventfd.h>
#include "Poller.h"
#include "Channel.h"
#include <memory>
#include <errno.h>
#include "Logger.h"
#include "CurrentThread.h"
using namespace std;
//__thread是一个thread_local的机制，代表这个变量是这个线程独有的全局变量，而不是所有线程共有
__thread EventLoop *t_loopInThisThread = nullptr; //防止一个线程创建多个EventLoop
//当一个eventloop被创建起来的时候,这个t_loopInThisThread就会指向这个Eventloop对象。
//如果这个线程又想创建一个EventLoop对象的话这个t_loopInThisThread非空，就不会再创建了。

const int kPollTimeMs = 10000; //定义默认的Pooler IO复用接口的超时时间

//创建wakeupfd，用来通notify subreactor处理新来的channelx
int createEventfd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd error: %d \n", errno);
    }
    return evtfd;
    
}

EventLoop::EventLoop() : 
    looping_(false), 
    quit_(false), 
    callingPendingFunctors_(false), 
    threadId_(CurrentThread::tid()), //获取当前线程的tid
    poller_(Poller::newDefaultPoller(this)), //获取一个封装着控制epoll操作的对象
    wakeupFd_(createEventfd()), //生成一个eventfd，每个EventLoop对象，都会有自己的eventfd
    wakeupChannel_(new Channel(this, wakeupFd_)),//每个channel都要知道自己所属的eventloop，
    currentActiveChannel_(nullptr) 
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread) //如果当前线程已经绑定了某个EventLoop对象了，那么该线程就无法创建新的EventLoop对象了
        LOG_FATAL("Another EventLoop %p exits in this thread %d \n", t_loopInThisThread, threadId_);
    else
    {
        t_loopInThisThread = this;
    }
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //用了functtion就要用bind，别用什么函数指针函数地址之类的。

    wakeupChannel_->enableReading(); //每一个EventLoop都将监听wakeupChannel的EpollIN读事件了。
    //mainReactor通过给wakeupFd_给sbureactor写东西。
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFd_); //回收资源
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));//mainReactor给subreactor发消息，subReactor通过wakeupFd_感知。
    if(n != sizeof(one))
        LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8", n);
    
}
void EventLoop::loop()
{ //EventLoop 所属线程执行
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);    
    while(!quit_)
    {
        //在我们的Epoller里面poll方法里面，我们把EventLoop的ActiveChannels传给了poll方法
        //当poll方法调用完了epoll_wait()之后，把有事件发生的channel都装进了这个ActiveChannels数组里面
        activeChannels_.clear();

        //监听两类fd，一个是client的fd，一个是wakeupfd，用于mainloop和subloop的通信
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);//此时activeChannels已经填好了事件发生的channel
        for(Channel *channel : activeChannels_)
        {
            //Poller监听哪些channel发生事件了，然后上报（通过activeChannels）给EventLoop，通知channel处理相应事件
            channel->HandlerEvent(pollReturnTime_);
        }
        /**
        IO线程 mainLoop负责接收新用户的连接，
        mainloop实现注册一个回调cb，这个回调由subloop来执行。
        wakeup subloop后执行下面的方法，执行mainloop的cb操作。
         */
        doPendingFunctors(); //执行当前EventLoop事件循环需要处理的回调操作。

    }
    LOG_INFO("EventLoop %p stop looping. \n", t_loopInThisThread);
}

void EventLoop::quit()
{
    //退出事件循环，有可能是loop在自己的线程中调用quit，loop()函数的while循环发现quit_=true就结束了循环
    //如果是在其他线程中调用的quit，在一个subloop线程中调用了mainloop的quit，那就调用wakeup唤醒mainLoop线程，
    /*
                    mainLoop

        subloop1    subloop2     subloop3
        这个quit可以是自己的loop调用，也可以是别的loop调用。比如mainloop调用subloop的quit
    */
    quit_ = true;
    if(!isInLoopThread()) 
        wakeup();  
}

void EventLoop::runInLoop(Functor cb)
{//保证了调用这个cb一定是在其EventLoop线程中被调用。
    if(isInLoopThread()){
        //如果当前调用runInLoop的线程正好是EventLoop的运行线程，则直接执行此函数
        cb();
    }
    else{
        //否则调用 queueInLoop 函数
        queueInLoop(cb);
    }
}
void EventLoop::queueInLoop(Functor cb)
{

    {
        unique_lock<mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //唤醒相应的，需要执行上面回调操作的loop线程
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调，
    // 这个时候就要wakeup()loop所在线程，让它继续去执行它的回调。
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        /***
        这里还需要结合下EventLoop循环的实现，其中doPendingFunctors()是每轮循环的最后一步处理。 
        如果调用queueInLoop和EventLoop在同一个线程，且callingPendingFunctors_为false时，
        则说明：此时尚未执行到doPendingFunctors()。 那么此时即使不用wakeup，也可以在之后照旧
        执行doPendingFunctors()了。这么做的好处非常明显，可以减少对eventfd的io读写。
        ***/
        wakeup(); 
        /***
        为什么要唤醒 EventLoop，我们首先调用了 pendingFunctors_.push_back(cb), 
        将该函数放在 pendingFunctors_中。EventLoop 的每一轮循环在最后会调用 
        doPendingFunctors 依次执行这些函数。而 EventLoop 的唤醒是通过 epoll_wait 实现的，
        如果此时该 EventLoop 中迟迟没有事件触发，那么 epoll_wait 一直就会阻塞。 
        这样会导致，pendingFunctors_中的任务迟迟不能被执行了。
        所以必须要唤醒 EventLoop ，从而让pendingFunctors_中的任务尽快被执行。
        ***/

    }
}

void EventLoop::wakeup()
{
    /**
     * @brief 在 EventLoop 建立之后，就创建一个 eventfd，并将其可读事件注册到 EventLoop 中。
    wakeup() 的过程本质上是对这个 eventfd 进行写操作，以触发该 eventfd 的可读事件。
    这样就起到了唤醒 EventLoop 的作用。
     * 
     */
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(n))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel); //
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    poller_->hasChannel(channel);
}
void EventLoop::doPendingFunctors()
{
    /***
    这里面又有精华！！
    我们在queueInLoop里面往pendingFunctors_数组里面插入新回调，
    这里定义了一个局部的functors数组，每次执行doPendingFunctors的时候都和pendingFunctors_
    交换，相当于把pendingFunctors_的对象全部导入到functors数组里面，让那后把pendingFunctors
    置为空，这样的好处是避免频繁的锁，因为如果你不用这种机制的话，生产者queueInLoop函数插入新回调，
    消费者doPendingFunctors消费回调，他们共用一个pendingFunctors_，这样生产者插入和消费者消费
    就会不停的互相触发锁机制。
    ***/
   std::vector<Functor> functors;
   callingPendingFunctors_ = true;
   {
       unique_lock<mutex> lock(mutex_);
       functors.swap(pendingFunctors_); //这里的swap其实只是交换的vector对象指向的内存空间的指针而已。
   }
   for(const Functor &functor:functors)
   {
       functor();
   }
   callingPendingFunctors_ = false;
}