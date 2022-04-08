#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid; //通过__thread 修饰的变量，在线程中地址都不一样，
    //__thread变量每一个线程有一份独立实体，各个线程的值互不干扰。

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0)) //提供给程序员使用的，目的是将“分支转移”的信息提供给编译器，这样编译器可以对代码进行优化，以减少指令跳转带来的性能下降。
        { //这里的意思时说 t_cachedTid == 0的可能性很小，不过如果t_cachedTid==0那就要调用cacheTid获取线程的tid了。
            cacheTid();
        }
        return t_cachedTid;
    }
}