#pragma once

#include <map>
#include <string>
#include <vector>

#include "mutex.h"
#include "singleton.h"

namespace sylar {

class Env {
public:
    using RWMutexType = RWMutex;

    bool init(int argc, char **argv);

    void add(const std::string &key, const std::string &value);

    bool has(const std::string &key);

    void del(const std::string &key);

    std::string get(const std::string &key, const std::string &defalut_value = "");

    void addHelp(const std::string &key, const std::string &desc);

    void removeHelp(const std::string &key);

    void printHelp();

    const std::string &getExe() const { return m_exe; }

    const std::string &getCwd() const { return m_cwd; }

    bool setEnv(const std::string &key, const std::string &value);

    std::string getEnv(const std::string &key, const std::string &defalut_value = "");

    std::string getAbsolutePath(const std::string &path) const;

    std::string getAbsoluteWorkPath(const std::string &path) const;

    std::string getConfigPath();

private:
    RWMutexType m_mutex;
    std::map<std::string, std::string> m_args;
    std::vector<std::pair<std::string, std::string>> m_helps;

    std::string m_program;
    std::string m_exe;
    std::string m_cwd;
};

using EnvMgr = sylar::Singleton<Env>;

}  // namespace sylar