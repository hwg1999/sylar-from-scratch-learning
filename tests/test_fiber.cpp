#include "sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber2() {
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber2 begin";
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber2 end";
}

void run_in_fiber() {
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber begin";

    SYLAR_LOG_INFO(g_logger) << "before run_in_fiber yield";
    sylar::Fiber::GetThis()->yield();
    SYLAR_LOG_INFO(g_logger) << "after run_in_fiber yield";

    SYLAR_LOG_INFO(g_logger) << "run_in_fiber end";
}

void test_fiber() {
    SYLAR_LOG_INFO(g_logger) << "test_fiber begin";

    sylar::Fiber::GetThis();

    sylar::Fiber::ptr fiber{new sylar::Fiber{run_in_fiber, 0, false}};
    SYLAR_LOG_INFO(g_logger) << "use_count:" << fiber.use_count();

    SYLAR_LOG_INFO(g_logger) << "before test_fiber resume";
    fiber->resume();
    SYLAR_LOG_INFO(g_logger) << "after test_fiber resume";

    SYLAR_LOG_INFO(g_logger) << "user_count:" << fiber.use_count();

    SYLAR_LOG_INFO(g_logger) << "fiber status: " << fiber->getState();

    SYLAR_LOG_INFO(g_logger) << "before test_fiber resume again";
    fiber->resume();
    SYLAR_LOG_INFO(g_logger) << "after test_fiber resume again";

    SYLAR_LOG_INFO(g_logger) << "use_count:" << fiber.use_count();
    SYLAR_LOG_INFO(g_logger) << "fiber status: " << fiber->getState();

    fiber->reset(run_in_fiber2);
    fiber->resume();

    SYLAR_LOG_INFO(g_logger) << "use_count:" << fiber.use_count();  // 1
    SYLAR_LOG_INFO(g_logger) << "test_fiber end";
}

int main(int argc, char *argv[]) {
    sylar::EnvMgr::GetInstance()->init(argc, argv);
    sylar::Config::LoadFromConfDir(sylar::EnvMgr::GetInstance()->getConfigPath());

    sylar::SetThreadName("main_thread");
    SYLAR_LOG_INFO(g_logger) << "main begin";

    std::vector<sylar::Thread::ptr> thrs;
    for (int i = 0; i < 2; ++i) {
        thrs.push_back(sylar::Thread::ptr{new sylar::Thread{&test_fiber, "thread_" + std::to_string(i)}});
    }

    for (const auto &thr : thrs) {
        thr->join();
    }

    SYLAR_LOG_INFO(g_logger) << "main end";
    
    return 0;
}