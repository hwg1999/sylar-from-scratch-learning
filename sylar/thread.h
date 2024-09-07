#pragma once

#include <functional>
#include <memory>
#include <string>
#include "mutex.h"

namespace sylar {

class Thread : Noncopyable {
public:
    using ptr = std::shared_ptr<Thread>;

    Thread(std::function<void()> cb, const std::string &name);

    ~Thread();

    pid_t getId() const { return m_id; }

    const std::string &getName() const { return m_name; }

    void join();

    static Thread *GetThis();

    static const std::string &GetName();

    static void SetName(const std::string &name);

private:
    static void *run(void *arg);

private:
    pid_t m_id = -1;
    pthread_t m_thread = 0;
    std::function<void()> m_cb;
    std::string m_name;
    Semaphore m_semaphore;
};

}  // namespace sylar