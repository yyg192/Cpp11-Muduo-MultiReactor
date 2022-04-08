#include "InetAddress.h"
#include <strings.h>
#include <string.h>
using namespace std;
/*
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr);
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;
    
    const sockaddr_in *getSockAddr() const;
    void setSockAddr(const sockaddr_in &addr);
private:
    sockaddr_in addr_;
};
*/

InetAddress::InetAddress(const sockaddr_in &addr):addr_(addr) {}

InetAddress::InetAddress(uint16_t port, string ip){
    bzero(&addr_, sizeof(addr_)); //man bzero 头文件在strings.h
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port); //从主机字节顺序转变成网络字节顺序
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); //inet_addr()用来将参数cp 所指的网络地址字符串转换成网络所使用的二进制数字
}

string InetAddress::toIp() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf)); //将数值格式转化为点分十进制的字符串ip地址格式
    return buf;
}
string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf); //这个函数在<string.h>里面，是个c函数而不在<strig>里面
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf+end, ":%u", port);
    return buf;
}
uint16_t InetAddress::toPort() const
{
    //
    return ntohs(addr_.sin_port);  //将一个无符号短整形数从网络字节顺序转换为主机字节顺序
}