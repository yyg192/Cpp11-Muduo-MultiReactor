#include "Thread.h"
#include "CurrentThread.h"
#include <semaphore.h>
using namespace std;

atomic<int> numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name): 
        started_(false), 
        joined_(false), //是join线程还是detach线程。
        tid_(0), 
        func_(std::move(func)),
        name_(name)
{
    setDefaultName();
}
Thread::~Thread(){
    if(started_ && !joined_){
        thread_->detach(); //分离线程
    }
}
void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    thread_ = shared_ptr<thread>(new thread([&] () {
        //获取线程的tid值
        tid_ = CurrentThread::tid(); //子线程获取当前所在线程的tid，注意执行到这一行已经是在新创建的子线程里面了。
        sem_post(&sem);
        func_(); //开启一个新线程，专门执行一个线程函数。
    }));
    //这里必须等待上面的新线程获取了它的tid值才能继续执行。
    sem_wait(&sem);
    //当这个start函数结束后，就可以放心的访问这个新线程了，因为他的tid已经获取了。
}
void Thread::join()
{ //待解释。。。。。。。。
    joined_ = true;
    thread_->join();
}


void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num); ///给这个线程一个名字
    }    
}
