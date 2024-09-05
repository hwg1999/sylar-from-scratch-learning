#include "log.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "mutex.h"
#include "util.h"

namespace sylar {

const char *LogLevel::ToString(LogLevel::Level level) {
    switch (level) {
#define XX(name)         \
    case LogLevel::name: \
        return #name;
        XX(FATAL);
        XX(ALERT);
        XX(CRIT);
        XX(ERROR);
        XX(WARN);
        XX(NOTICE);
        XX(INFO);
        XX(DEBUG);
#undef XX
        default:
            return "NOTSET";
    }
    return "NOTSET";
}

LogLevel::Level LogLevel::FromString(const std::string &str) {
#define XX(level, v)            \
    if (str == #v) {            \
        return LogLevel::level; \
    }

    XX(FATAL, fatal);
    XX(ALERT, alert);
    XX(CRIT, crit);
    XX(ERROR, error);
    XX(WARN, warn);
    XX(NOTICE, notice);
    XX(INFO, info);
    XX(DEBUG, debug);

    XX(FATAL, FATAL);
    XX(ALERT, ALERT);
    XX(CRIT, CRIT);
    XX(ERROR, ERROR);
    XX(WARN, WARN);
    XX(NOTICE, NOTICE);
    XX(INFO, INFO);
    XX(DEBUG, DEBUG);
#undef XX

    return LogLevel::NOTSET;
}

LogEvent::LogEvent(const std::string &logger_name, LogLevel::Level level, const char *file, int32_t line,
                   int64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string &thread_name)
    : m_level{level},
      m_file{file},
      m_line{line},
      m_elapse{elapse},
      m_threadId{thread_id},
      m_fiberId{fiber_id},
      m_time{time},
      m_threadName{thread_name},
      m_loggerName{logger_name} {}

void LogEvent::printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void LogEvent::vprintf(const char *fmt, va_list ap) {
    auto deleter = [](char *buf) { ::free(buf); };
    char *buf = nullptr;
    std::unique_ptr<char, decltype(deleter)> bufPtr{buf, deleter};
    int len = vasprintf(&buf, fmt, ap);
    if (len != -1) {
        m_ss << std::string(buf, len);
    }
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getContent(); }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << LogLevel::ToString(event->getLevel()); }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getElapse(); }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
public:
    LoggerNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getLoggerName(); }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string &str) {}

    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getThreadId(); }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getFiberId(); }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getThreadName(); }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string &str = "%Y-%m-%d %H:%M:%S") : m_format{str} {
        if (m_format.empty()) {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof buf, m_format.c_str(), &tm);
        os << buf;
    }

private:
    std::string m_format;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
    FileNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getFile(); }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << event->getLine(); }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override { os << std::endl; }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string &str) : m_string{str} {}
    void format(std::ostream &os, LogEvent::ptr) override { os << m_string; }

private:
    std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string &str) {}

    void format(std::ostream &os, LogEvent::ptr event) override { os << "\t"; }
};

class PercentSignFormatItem : public LogFormatter::FormatItem {
public:
    PercentSignFormatItem(const std::string &str) {}

    void format(std::ostream &os, LogEvent::ptr event) override { os << "%"; }
};

LogFormatter::LogFormatter(const std::string &pattern) : m_pattern{pattern} { init(); }

void LogFormatter::init() {
    std::vector<std::pair<int, std::string>> patterns;
    std::string tmp;
    std::string dateFormat;
    bool error = false;

    bool parsing_string = true;

    size_t i = 0;
    while (i < m_pattern.size()) {
        std::string c = std::string(1, m_pattern[i]);
        if (c == "%") {
            if (parsing_string) {
                if (!tmp.empty()) {
                    patterns.push_back({0, tmp});
                }
                tmp.clear();
                parsing_string = false;
                ++i;
            } else {
                patterns.push_back({1, c});
                parsing_string = true;
                ++i;
                continue;
            }
        } else {
            if (parsing_string) {
                tmp += c;
                ++i;
                continue;
            } else {
                patterns.push_back({1, c});
                parsing_string = true;

                if (c != "d") {
                    ++i;
                    continue;
                    ;
                }
                ++i;
                if (i < m_pattern.size() && m_pattern[i] != '{') {
                    continue;
                }
                ++i;
                while (i < m_pattern.size() && m_pattern[i] != '}') {
                    dateFormat.push_back(m_pattern[i]);
                    ++i;
                }
                if (m_pattern[i] != '}') {
                    // %d后面的大括号没有闭合，直接报错
                    printf("[ERROR] LogFormatter::init() pattern[%s] '{' not closed\n", m_pattern.c_str());
                    error = true;
                    break;
                }
                i++;
                continue;
            }
        }
    }

    if (error) {
        m_error = true;
        return;
    }

    if (!tmp.empty()) {
        patterns.push_back({0, tmp});
        tmp.clear();
    }

    static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> s_format_items = {
#define XX(str, C)                                                               \
    {                                                                            \
        #str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); } \
    }
        XX(m, MessageFormatItem),      // m:消息
        XX(p, LevelFormatItem),        // p:日志级别
        XX(c, LoggerNameFormatItem),   // c:日志器名称
        XX(d, DateTimeFormatItem),     // d:日期时间
        XX(r, ElapseFormatItem),       // r:累计毫秒数
        XX(f, FileNameFormatItem),     // f:文件名
        XX(l, LineFormatItem),         // l:行号
        XX(t, ThreadIdFormatItem),     // t:线程号
        XX(F, FiberIdFormatItem),      // F:协程号
        XX(N, ThreadNameFormatItem),   // N:线程名称
        XX(%, PercentSignFormatItem),  // %:百分号
        XX(T, TabFormatItem),          // T:制表符
        XX(n, NewLineFormatItem),      // n:换行符
#undef XX
    };

    for (const auto &[patternType, patternStr] : patterns) {
        if (patternType == 0) {
            m_items.push_back(std::make_shared<StringFormatItem>(patternStr));
        } else if (patternStr == "d") {
            m_items.push_back(std::make_shared<DateTimeFormatItem>(dateFormat));
        } else {
            if (auto it = s_format_items.find(patternStr); it != s_format_items.end()) {
                const auto &[_, factory] = *it;
                m_items.push_back(factory(patternStr));
            } else {
                printf(
                    "[ERROR] LogFormatter::init() pattern: [%s] unknown format "
                    "item: %s\n",
                    m_pattern.c_str(), patternStr.c_str());
                error = true;
                break;
            }
        }
    }

    if (error) {
        m_error = true;
        return;
    }
}

std::string LogFormatter::format(LogEvent::ptr event) {
    std::stringstream ss;
    for (const auto &formatItem : m_items) {
        formatItem->format(ss, event);
    }
    return ss.str();
}

std::ostream &LogFormatter::format(std::ostream &os, LogEvent::ptr event) {
    for (const auto &formatItem : m_items) {
        formatItem->format(os, event);
    }
    return os;
}

LogAppender::LogAppender(LogFormatter::ptr default_formatter) : m_defaultFormatter{default_formatter} {}

void LogAppender::setFormatter(LogFormatter::ptr val) {
    MutexType::Lock lock{m_mutex};
    m_formatter = val;
}

LogFormatter::ptr LogAppender::getFormatter() {
    MutexType::Lock lock{m_mutex};
    return m_formatter ? m_formatter : m_defaultFormatter;
}

StdoutLogAppender::StdoutLogAppender() : LogAppender{std::make_shared<LogFormatter>()} {}

void StdoutLogAppender::log(LogEvent::ptr event) {
    if (m_formatter) {
        m_formatter->format(std::cout, event);
    } else {
        m_defaultFormatter->format(std::cout, event);
    }
}

FileLogAppender::FileLogAppender(const std::string &file) : LogAppender{std::make_shared<LogFormatter>()} {
    m_filename = file;
    reopen();
}

void FileLogAppender::log(LogEvent::ptr event) {
    uint64_t now = event->getTime();
    if (now >= (m_lastTime + 3)) {
        reopen();
        m_lastTime = now;
    }

    MutexType::Lock lock{m_mutex};
    if (m_formatter) {
        if (!m_formatter->format(m_filestream, event)) {
            printf("[ERROR] FileLogAppender::log() format error\n");
        }
    } else if (!m_defaultFormatter->format(m_filestream, event)) {
        printf("[ERROR] FileLogAppender::log() format error\n");
    }
}

bool FileLogAppender::reopen() {
    MutexType::Lock lock{m_mutex};
    if (m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename, std::ios::app);
    return !m_filestream;
}

Logger::Logger(const std::string &name) : m_name{name}, m_level{LogLevel::INFO}, m_createTime{GetElapsed()} {}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock{m_mutex};
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock{m_mutex};
    for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
        if (*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::clearAppenders() {
    MutexType::Lock lock{m_mutex};
    m_appenders.clear();
}

void Logger::log(LogEvent::ptr event) {
    if (event->getLevel() <= m_level) {
        for (const auto &appender : m_appenders) {
            appender->log(event);
        }
    }
}

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event) : m_logger{logger}, m_event{event} {}

LogEventWrap::~LogEventWrap() { m_logger->log(m_event); }

LoggerManager::LoggerManager() {
    m_root.reset(new Logger("root"));
    m_root->addAppender(std::make_shared<StdoutLogAppender>());
    m_loggers[m_root->getName()] = m_root;
    init();
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
    MutexType::Lock lock{m_mutex};
    if (auto it = m_loggers.find(name); it != m_loggers.end()) {
        return it->second;
    }

    Logger::ptr logger{new Logger{name}};
    m_loggers[name] = logger;
    return logger;
}

void LoggerManager::init() {}

}  // namespace sylar