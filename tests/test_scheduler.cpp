#include "sylar/sylar.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_fiber1() {
    SYLAR_LOG_INFO(g_logger) << "test_fiber1 begin";

    sylar::Scheduler::GetThis()->schedule(sylar::Fiber::GetThis());

    SYLAR_LOG_INFO(g_logger) << "before test_fiber1 yield";
    sylar::Fiber::GetThis()->yield();
    SYLAR_LOG_INFO(g_logger) << "after test_fiber1 yield";

    SYLAR_LOG_INFO(g_logger) << "test_fiber1 end";
}

void test_fiber2() {
    SYLAR_LOG_INFO(g_logger) << "test_fiber2 begin";

    sleep(3);

    SYLAR_LOG_INFO(g_logger) << "test_fiber2 end";
}

void test_fiber3() {
    SYLAR_LOG_INFO(g_logger) << "test_fiber3 begin";
    SYLAR_LOG_INFO(g_logger) << "test_fiber3 end";
}

void test_fiber5() {
    static int count = 0;

    SYLAR_LOG_INFO(g_logger) << "test_fiber5 begin, i = " << count;
    SYLAR_LOG_INFO(g_logger) << "test_fiber5 end i = " << count;

    count++;
}

void test_fiber4() {
    SYLAR_LOG_INFO(g_logger) << "test_fiber4 begin";

    for (int i = 0; i < 3; ++i) {
        sylar::Scheduler::GetThis()->schedule(test_fiber5, sylar::GetThreadId());
    }

    SYLAR_LOG_INFO(g_logger) << "test_fiber4 end";
}

int main() {
    SYLAR_LOG_INFO(g_logger) << "main begin";

    // sylar::Scheduler scheduler(3, false);
    sylar::Scheduler scheduler;

    scheduler.schedule(test_fiber1);
    scheduler.schedule(test_fiber2);

    sylar::Fiber::ptr fiber{new sylar::Fiber{&test_fiber3}};
    scheduler.schedule(fiber);

    scheduler.start();

    scheduler.schedule(test_fiber4);

    scheduler.stop();

    SYLAR_LOG_INFO(g_logger) << "main end";

    return 0;
}