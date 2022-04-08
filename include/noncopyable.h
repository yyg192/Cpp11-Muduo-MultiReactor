#pragma once  //编译级别，防止重复包含

namespace muduo{
class noncopyable
{
    /**
     * @brief 派生类可以正常构构造和析构，但是不能拷贝构造和赋值。这种做法一定要好好学习一下！！
     */
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete; //如果要做连续赋值的话就返回noncopyable&，不然就返回void
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};
}
