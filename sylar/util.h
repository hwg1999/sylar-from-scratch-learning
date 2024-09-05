#pragma once

#include <sched.h>
#include <cstdint>
#include <ctime>
#include <ios>
#include <string>
#include <vector>

namespace sylar {

pid_t GetThreadId();

uint64_t GetFiberId();

uint64_t GetElapsed();

std::string GetThreadName();

void SetThreadName(const std::string &name);

void Backtrace(std::vector<std::string> &bt, int size = 64, int skip = 1);

std::string BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "");

uint64_t GetCurrentMS();

uint64_t GetCurrentUS();

std::string ToUpper(const std::string &name);

std::string ToLower(const std::string &name);

std::string Time2Str(time_t ts = time(0), const std::string &format = "%Y-%m-%d %H:%M:%S");

time_t Str2Time(const char *str, const char *format = "%Y-%m-%d %H:%M:%S");

class FSUtil {
public:
    static void ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string &subfix);

    static bool Mkdir(const std::string &name);

    static bool IsRunningPidfile(const std::string &pidfile);

    static bool Rm(const std::string &path);

    static bool Mv(const std::string &name, const std::string &to);

    static bool Realpath(const std::string &path, std::string &rpath);

    static bool Symlink(const std::string &from, const std::string &to);

    static bool Unlink(const std::string &filename, bool exist = false);

    static std::string Dirname(const std::string &filename);

    static std::string Basename(const std::string &filename);

    static bool OpenForRead(std::ifstream &ifs, const std::string &filename, std::ios_base::openmode mode);

    static bool OpenForWrite(std::ofstream &ofs, const std::string &filename, std::ios_base::openmode mode);
};

class TypeUtil {
public:
    static int8_t ToChar(const std::string &str);

    static int64_t Atoi(const std::string &str);

    static double Atof(const std::string &str);

    static int8_t ToChar(const char *str);

    static int64_t Atoi(const char *str);

    static double Atof(const char *str);
};

}  // namespace sylar