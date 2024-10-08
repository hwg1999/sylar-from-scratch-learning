#include <iostream>
#include "sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test2() { printf("%s\n", sylar::BacktraceToString().c_str()); }

void test1() { test2(); }

void test_backtrace() { test1(); }

int main() {
    SYLAR_LOG_INFO(g_logger) << sylar::GetCurrentMS();
    SYLAR_LOG_INFO(g_logger) << sylar::GetCurrentUS();
    SYLAR_LOG_INFO(g_logger) << sylar::ToUpper("hello");
    SYLAR_LOG_INFO(g_logger) << sylar::ToLower("HELLO");
    SYLAR_LOG_INFO(g_logger) << sylar::Time2Str();
    SYLAR_LOG_INFO(g_logger) << sylar::Str2Time("1970-01-01 00:00:00");

    std::vector<std::string> files;
    sylar::FSUtil::ListAllFile(files, "./sylar", ".cpp");
    for (const auto &file : files) {
        SYLAR_LOG_INFO(g_logger) << file;
    }

    test_backtrace();

    SYLAR_ASSERT2(false, "assert");

    return 0;
}