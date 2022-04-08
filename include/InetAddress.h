#pragma once

#include <arpa/inet.h> //
#include <netinet/in.h> //sockaddr_in
#include <string>

/***
 封装socket地址类型，这里就没有对ipv6进行封装了，只对ipv4进行封装
 */
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr);
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;
    
    const sockaddr_in *getSockAddr() const;
    void setSockAddr(const sockaddr_in &addr){addr_ = addr;}
private:
    sockaddr_in addr_;
};