#pragma once

#include "log.h"
#include "scheduler.h"

namespace sylar {

class IOManager : public Scheduler {
public:
    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex;

    enum Event {
        NONE = 0X0,
        READ = 0X1,
        WRITE = 0x4,
    };

private:
    struct FdContext {
        using MutexType = Mutex;
        struct EventContext {
            Scheduler *m_scheduler = nullptr;
            Fiber::ptr m_fiber;
            std::function<void()> m_cb;
        };

        EventContext &getEventContext(Event event);

        void resetEventContext(EventContext &ctx);

        void triggerEvent(Event event);

        EventContext m_readCtx;
        EventContext m_writeCtx;
        int m_fd{0};
        Event m_events{NONE};
        MutexType m_mutex;
    };

public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");

    ~IOManager();

    bool addEvent(int fd, Event event, std::function<void()> cb = nullptr);

    bool delEvent(int fd, Event event);

    bool cancelEvent(int fd, Event event);

    bool cancelAll(int fd);

    static IOManager *GetThis();

protected:
    void tickle() override;

    bool stopping() override;

    void idle() override;

    void contextResize(size_t size);

private:
    int m_epfd{0};
    int m_tickleFds[2];
    std::atomic<size_t> m_pendingEventCount{0};
    RWMutexType m_mutex;
    std::vector<std::unique_ptr<FdContext>> m_fdContexts;
};

}  // namespace sylar