#include "util.h"
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <ctime>
#include <string_view>

namespace sylar {

pid_t GetThreadId() { return syscall(SYS_gettid); }

uint64_t GetFiberId() { return 0; }

uint64_t GetElapsed() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

std::string GetThreadName() {
    char thread_name[16] = {0};
    pthread_getname_np(pthread_self(), thread_name, sizeof thread_name);
    return thread_name;
}

void SetThreadName(const std::string &name) { pthread_setname_np(pthread_self(), name.c_str()); }

}  // namespace sylar