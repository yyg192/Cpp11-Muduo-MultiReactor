#pragma once
#include <memory>
#include <functional>
class Buffer;
class TcpConnection;
class TimeStamp;
/***
这里的编程思想还有待摸清，首先我是先决定了TcpConnection应该用智能指针管理，毕竟这种对象
创建容易泄露也容易，而且也不是那种创建删除很频繁的对象。而TcpConnection内部
 */

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void (const TcpConnectionPtr&)>;
using CloseCallback = std::function<void (const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void (const TcpConnectionPtr&)>;
using MessageCallback = std::function<void (const TcpConnectionPtr&, Buffer*, TimeStamp)>;
using HighWaterMarkCallback = std::function<void (const TcpConnectionPtr&, size_t)>;
