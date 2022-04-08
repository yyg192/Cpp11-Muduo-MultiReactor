#pragma once
#include <vector>
#include <sys/types.h>
#include <string>
/*
好好看清楚Buffer的样子！！！
+-------------------------+----------------------+---------------------+
|    prependable bytes    |    readable bytes    |    writable bytes   |
|                         |      (CONTENT)       |                     |
+-------------------------+----------------------+---------------------+
|                         |                      |                     |
0        <=           readerIndex     <=     writerIndex             size
*/
class Buffer
{
public:
    //别搞傻了，readable bytes空间才是要服务端要发送的数据，writable bytes空间是从socket读来的数据存放的地方。
    static const size_t kCheapPrepend = 8; //记录数据包的长度的变量长度，用于解决粘包问题
    static const size_t kInitialSize = 1024; //缓冲区长度
    /*static const int可以在类里面初始化，是因为它既然是const的，那程序就不会再去试图初始化了。*/

    explicit Buffer(size_t initialSize = kInitialSize) 
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }
    size_t prependableBytes() const
    {
        return readerIndex_;
    }
    const char* peek() const //peek函数返回可读数据的起始地址
    {
        return begin() + readerIndex_;
    }
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ += len; //应用只读取可读缓冲区数据的一部分，就是len
        }
        else //len == readableBytes()
        {
            retrieveAll();
        }
        
    }
    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    std::string retrieveAllString()
    {
        //把onMessage函数上报的Buffer数据转成string类型的数据返回
        return retrieveAsString(readableBytes()); //应用可读取数据的长度
    }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len); //peek()返回的是可读数据的起始地址
        retrieve(len); //上面把一句把缓冲区中的可读数据读取出来，要然后要对缓冲区进行复位操作。
        return result;
    }
    void ensureWritableBytes(size_t len)
    {
        // writableBytes()返回可写缓冲区的大小
        if(writableBytes() < len){
            //扩容
            makeSpace(len);
        }
    }
    void append(const char* data, size_t len)
    {//把[data, data+len]内存上的数据添加到writable缓冲区当中
        ensureWritableBytes(len); //反正每次写之前都会看有没有必要调整一下Buffer缓冲区结构
        std::copy(data, data+len,beginWrite());
        writerIndex_ += len;
    }
    char* beginWrite()
    {
        return begin() + writerIndex_;
    }
    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }
    ssize_t readFd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin()
    {
        return &*buffer_.begin(); 
        //先调用begin()返回buffer_的首个元素的迭代器，然后再解引用得到这个变量的，再取地址，得到这个变量的首地址。
        //反正最后得到的就是vector底层的数组的首地址
    }
    const char* begin() const{ 
        return &*buffer_.begin();
    }
    
    void makeSpace(size_t len)
    {
       if(writableBytes() + prependableBytes() - kCheapPrepend < len)
       {//能用来写的缓冲区大小 < 我要写入的大小len，那么就要扩容了
           buffer_.resize(writerIndex_+len);
       }
       else{
            //如果能写的缓冲区大小 >= 要写的len，那么说明要重新调整一下Buffer的两个游标了。
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, //源 首迭代器 
                      begin() + writerIndex_, //源 尾迭代器
                      begin() + kCheapPrepend);  //目标 首迭代器
            //这里把可读区域的数据给前移了
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
       }
       
    }
    std::vector<char> buffer_; //为什么要用vector，因为可以动态扩容啊！！！
    size_t readerIndex_; 
    size_t writerIndex_;
};
