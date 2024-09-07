#include "sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber2() {
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber2 begin";
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber2 end";
}

void run_in_fiber() {
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber begin";

    sylar::Fiber::ptr fiber{new sylar::Fiber{run_in_fiber2}};
    fiber->resume();

    SYLAR_LOG_INFO(g_logger) << "run_in_fiber end";
}

int main(int argc, char *argv[]) {
    sylar::EnvMgr::GetInstance()->init(argc, argv);
    sylar::Config::LoadFromConfDir(sylar::EnvMgr::GetInstance()->getConfigPath());

    SYLAR_LOG_INFO(g_logger) << "main begin";

    sylar::Fiber::GetThis();

    sylar::Fiber::ptr fiber{new sylar::Fiber{run_in_fiber}};
    fiber->resume();

    SYLAR_LOG_INFO(g_logger) << "main end";

    return 0;
}