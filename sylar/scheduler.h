#pragma once

#include <list>
#include <memory>

#include "fiber.h"
#include "thread.h"
#include "hook.h"

namespace sylar {

class Scheduler {
public:
    using ptr = std::shared_ptr<Scheduler>;
    using MutexType = Mutex;

    Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "Scheduler");

    virtual ~Scheduler();

    const std::string &getName() const { return m_name; }

    static Scheduler *GetThis();

    static Fiber *GetSchedulerFiber();

    template <typename FiberOrCb>
    void schedule(FiberOrCb fc, int threadId = -1) {
        bool need_tickle{false};

        {
            MutexType::Lock lock{m_mutex};
            need_tickle = scheduleNoLock(fc, threadId);
        }

        if (need_tickle) {
            tickle();
        }
    }

    void start();

    void stop();

protected:
    virtual void tickle();

    void run();

    virtual void idle();

    virtual bool stopping();

    void setThis();

    bool hasIdleThreads() { return m_idleThreadCount > 0; }

private:
    template <typename FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int threadId) {
        bool need_tickle = m_tasks.empty();
        ScheduleTask task{fc, threadId};
        if (task.m_fiber || task.m_cb) {
            m_tasks.push_back(task);
        }
        return need_tickle;
    }

private:
    struct ScheduleTask {
        Fiber::ptr m_fiber;
        std::function<void()> m_cb;
        int m_threadId;

        ScheduleTask(Fiber::ptr fiber, int threadId) : m_fiber{fiber}, m_threadId{threadId} {}

        ScheduleTask(Fiber::ptr *fiber, int threadId) : m_threadId{threadId} { fiber->swap(*fiber); }

        ScheduleTask(std::function<void()> cb, int threadId) : m_cb{std::move(cb)}, m_threadId{threadId} {}

        ScheduleTask() : m_threadId{-1} {}

        void reset() {
            m_fiber = nullptr;
            m_cb = nullptr;
            m_threadId = -1;
        }
    };

private:
    std::string m_name;
    MutexType m_mutex;
    std::vector<Thread::ptr> m_threads;
    std::list<ScheduleTask> m_tasks;
    std::vector<int> m_threadIds;
    size_t m_threadCount{0};
    std::atomic<size_t> m_activeThreadCount{0};
    std::atomic<size_t> m_idleThreadCount{0};

    bool m_useCaller;
    Fiber::ptr m_rootFiber;
    int m_rootThreadId{0};

    bool m_stopping{false};
};

}  // namespace sylar