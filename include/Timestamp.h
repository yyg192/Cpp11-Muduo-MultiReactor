#pragma once

#include <iostream>
#include <string>
class TimeStamp
{
    /*
        这里只复现了一部分用到的TimeStamp方法，其他的方法因为没用到，所以就没复现了。
    */
public:
    TimeStamp();
    explicit TimeStamp(int64_t microSecondSinceEpoch); //禁止隐式转换
    static TimeStamp now(); //返回当前时间
    std::string toString() const;
    
private:
    int64_t microSecondSinceEpoch_; //muduo中用了一个int64_t作为当前时间，单位为微妙
};