#pragma once
#include <string>
#include "noncopyable.h"
/*
日志级别：INFO ERROR FATAL DEBUG
*/
//LOG_INFO(%s %d, agr1, arg2)
#define LOG_INFO(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::Instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf); \
    } while(0)
#define LOG_ERROR(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::Instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf); \
    } while(0)
#define LOG_FATAL(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::Instance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf); \
        exit(-1);\
    } while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::Instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf); \
    } while(0)
#else
    #define LOG_DEBUG(LogmsgFormat, ...)
#endif

enum LoggerLevel{
    INFO, //普通日志信息
    ERROR,  //错误日志信息
    FATAL,  //core dump信息
    DEBUG  //调试信息
};

class Logger
{
public:
    static Logger& Instance(); //获取日志实例对象
    void setLogLevel(int level); //设置日志级别
    void log(std::string msg); //写日志
private:
    Logger() = default;
    int level_; //日志级别
    
};

