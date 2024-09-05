#pragma once

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include "mutex.h"
#include "singleton.h"

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

#define SYLAR_LOG_LEVEL(logger, level)                                                                           \
    if (level <= logger->getLevel())                                                                             \
    sylar::LogEventWrap(                                                                                         \
        logger, sylar::LogEvent::ptr(new sylar::LogEvent(                                                        \
                    logger->getName(), level, __FILE__, __LINE__, sylar::GetElapsed() - logger->getCreateTime(), \
                    sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::GetThreadName())))                \
        .getLogEvent()                                                                                           \
        ->getSS()

#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_ALERT(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ALERT)

#define SYLAR_LOG_CRIT(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::CRIT)

#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)

#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)

#define SYLAR_LOG_NOTICE(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::NOTICE)

#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...)                                                             \
    if (level <= logger->getLevel())                                                                             \
    sylar::LogEventWrap(                                                                                         \
        logger, sylar::LogEvent::ptr(new sylar::LogEvent(                                                        \
                    logger->getName(), level, __FILE__, __LINE__, sylar::GetElapsed() - logger->getCreateTime(), \
                    sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::GetThreadName())))                \
        .getLogEvent()                                                                                           \
        ->printf(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_ALERT(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ALERT, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_CRIT(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::CRIT, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_NOTICE(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::NOTICE, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)

namespace sylar {

class LogLevel {
public:
    enum Level {
        FATAL = 0,
        ALERT = 100,
        CRIT = 200,
        ERROR = 300,
        WARN = 400,
        NOTICE = 500,
        INFO = 600,
        DEBUG = 700,
        NOTSET = 800
    };

    static const char *ToString(LogLevel::Level level);

    static LogLevel::Level FromString(const std::string &str);
};

class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;
    LogEvent(const std::string &logger_name, LogLevel::Level level, const char *file, int32_t line, int64_t elapse,
             uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string &thread_name);

    LogLevel::Level getLevel() const { return m_level; }

    std::string getContent() const { return m_ss.str(); }

    std::string getFile() const { return m_file; }

    int32_t getLine() const { return m_line; }

    int64_t getElapse() const { return m_elapse; }

    uint32_t getThreadId() const { return m_threadId; }

    uint64_t getFiberId() const { return m_fiberId; }

    time_t getTime() const { return m_time; }

    const std::string &getThreadName() const { return m_threadName; }

    std::stringstream &getSS() { return m_ss; }

    const std::string &getLoggerName() const { return m_loggerName; }

    void printf(const char *fmt, ...);

    void vprintf(const char *fmt, va_list ap);

private:
    LogLevel::Level m_level;
    std::stringstream m_ss;
    const char *m_file = nullptr;
    int32_t m_line = 0;
    int64_t m_elapse = 0;
    uint32_t m_threadId = 0;
    uint64_t m_fiberId = 0;
    time_t m_time;
    std::string m_threadName;
    std::string m_loggerName;
};

class LogFormatter {
public:
    using ptr = std::shared_ptr<LogFormatter>;

    LogFormatter(const std::string &pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

    void init();

    bool isError() const { return m_error; }

    std::string format(LogEvent::ptr event);

    std::ostream &format(std::ostream &os, LogEvent::ptr event);

public:
    class FormatItem {
    public:
        using ptr = std::shared_ptr<FormatItem>;

        virtual ~FormatItem() {}

        virtual void format(std::ostream &os, LogEvent::ptr event) = 0;
    };

private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;
};

class LogAppender {
public:
    using ptr = std::shared_ptr<LogAppender>;
    using MutexType = SpinLock;

    LogAppender(LogFormatter::ptr default_formatter);

    virtual ~LogAppender() {}

    void setFormatter(LogFormatter::ptr val);

    LogFormatter::ptr getFormatter();

    virtual void log(LogEvent::ptr event) = 0;

protected:
    MutexType m_mutex;
    LogFormatter::ptr m_formatter;
    LogFormatter::ptr m_defaultFormatter;
};

class StdoutLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;

    StdoutLogAppender();

    void log(LogEvent::ptr event) override;
};

class FileLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<FileLogAppender>;

    FileLogAppender(const std::string &file);

    void log(LogEvent::ptr event) override;

    bool reopen();

private:
    std::string m_filename;
    std::ofstream m_filestream;
    uint64_t m_lastTime = 0;
};

class Logger {
public:
    using ptr = std::shared_ptr<Logger>;
    using MutexType = SpinLock;

    Logger(const std::string &name = "default");

    const std::string &getName() const { return m_name; }

    uint64_t getCreateTime() const { return m_createTime; }

    void setLevel(LogLevel::Level level) { m_level = level; }

    LogLevel::Level getLevel() const { return m_level; }

    void addAppender(LogAppender::ptr appender);

    void delAppender(LogAppender::ptr appender);

    void clearAppenders();

    void log(LogEvent::ptr event);

private:
    MutexType m_mutex;
    std::string m_name;
    LogLevel::Level m_level;
    std::list<LogAppender::ptr> m_appenders;
    uint64_t m_createTime;
};

class LogEventWrap {
public:
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);

    ~LogEventWrap();

    LogEvent::ptr getLogEvent() const { return m_event; }

private:
    Logger::ptr m_logger;
    LogEvent::ptr m_event;
};

class LoggerManager {
public:
    using MutexType = SpinLock;

    LoggerManager();

    void init();

    Logger::ptr getLogger(const std::string &name);

    Logger::ptr getRoot() { return m_root; }

private:
    MutexType m_mutex;
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

using LoggerMgr = sylar::Singleton<LoggerManager>;

}  // namespace sylar