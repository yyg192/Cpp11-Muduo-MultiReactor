#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
 使用read()将数据读到不连续的内存、使用write将不连续的内存发送出去，需要多次调用read和write
 如果要从文件中读一片连续的数据到进程的不同区域，有两种方案，要么使用read一次将它们读到
 一个较大的缓冲区中，然后将他们分成若干部分复制到不同的区域，要么调用read若干次分批把他们读至
 不同的区域，这样，如果想将程序中不同区域的连续数据块写到文件，也必须进行类似的处理。
 频繁系统调用和拷贝开销比较大，所以Unix提供了另外两个函数readv()和writev()，它们只需要
 一次系统调用就可以实现在文件和进程的多个缓冲区之间传送数据，免除多次系统调用或复制数据的开销。
 readv叫散布读，即把若干连续的数据块读入内存分散的缓冲区中，
 writev叫聚集写，吧内存中分散的若干缓冲区写到文件的连续区域中
 */

ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    //从客户端套接字fd上读取数据, Poller工作在LT模式，只要数据没读完epoll_wait就会一直上报
    char extrabuf[65536] = {0}; //栈上的内存空间
    struct iovec vec[2];
    const size_t writableSpace = writableBytes(); //可写缓冲区的大小
    vec[0].iov_base = begin() + writerIndex_; //第一块缓冲区
    vec[0].iov_len = writableSpace; //当我们用readv从socket缓冲区读数据，首先会先填满这个vec[0]
                                    //也就是我们的Buffer缓冲区
    vec[1].iov_base = extrabuf; //第二块缓冲区，如果Buffer缓冲区都填满了，那就填到我们临时创建的
    vec[1].iov_len = sizeof(extrabuf); //栈空间上。
    const int iovcnt = (writableSpace < sizeof(extrabuf) ? 2 : 1);
    //如果Buffer缓冲区大小比extrabuf(64k)还小，那就Buffer和extrabuf都用上
    //如果Buffer缓冲区大小比64k还大或等于，那么就只用Buffer。这意味着，我们最少也能一次从socket fd读64k空间
    const ssize_t n = ::readv(fd, vec, iovcnt); 
    if(n < 0){
        *saveErrno = errno; //出错了！！
    }
    else if(n <= writableSpace){ //说明Buffer空间足够存了
        writerIndex_ += n;  //
    }
    else{ //Buffer空间不够存，需要把溢出的部分（extrabuf）倒到Buffer中（会先触发扩容机制）
        writerIndex_ = buffer_.size();
        append(extrabuf, n-writableSpace);
    }
    return n;
}
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    //向socket fd上写数据，假如TCP发送缓冲区满
    const size_t readableSpace = readableBytes();
    ssize_t n = ::write(fd, peek(), readableSpace); //从Buffer中有的数据(readableBytes)写到socket中
    if(n < 0)
    {   
        *saveErrno = errno;
    }
    return n;
}   