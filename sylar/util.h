#pragma once

#include <sched.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace sylar {

pid_t GetThreadId();

uint64_t GetFiberId();

uint64_t GetElapsed();

std::string GetThreadName();

void SetThreadName(const std::string& name);

}  // namespace sylar