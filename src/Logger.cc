#include "Logger.h"
#include <iostream>
#include "Timestamp.h"
using namespace std;


Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}
void Logger::setLogLevel(int level)
{
    level_ = level;
}
void Logger::log(string msg)
{
    // 写日志 [级别信息] time : msg
    switch(level_)
    {
        case INFO:
        {
            cout << "[INFO]";
            break;
        }
        case ERROR:
        {
            cout << "[ERROR]";
            break;
        }
        case FATAL:
        {
            cout << "[FATAL]";
            break;
        }
        case DEBUG:
        {
            cout << "[DEBUG]";
            break;
        }
        default:
            break;
    }
    //打印时间和消息
    cout << TimeStamp::now().toString() << " : " << msg << endl;
}
