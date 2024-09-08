#include "scheduler.h"
#include "log.h"
#include "macro.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static thread_local Scheduler *t_scheduler = nullptr;

static thread_local Fiber *t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) : m_name{name}, m_useCaller{use_caller} {
    SYLAR_ASSERT(threads > 0);

    if (use_caller) {
        --threads;
        sylar::Fiber::GetThis();
        SYLAR_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

        m_rootFiber.reset(new Fiber{std::bind(&Scheduler::run, this), 0, false});

        sylar::Thread::SetName(m_name);
        t_scheduler_fiber = m_rootFiber.get();
        m_rootThreadId = sylar::GetThreadId();
        m_threadIds.push_back(m_rootThreadId);
    } else {
        m_rootThreadId = -1;
    }

    m_threadCount = threads;
}

Scheduler *Scheduler::GetThis() { return t_scheduler; }

Fiber *Scheduler::GetSchedulerFiber() { return t_scheduler_fiber; }

void Scheduler::setThis() { t_scheduler = this; }

Scheduler::~Scheduler() {
    SYLAR_LOG_DEBUG(g_logger) << "Scheduler::~Scheduler()";
    SYLAR_ASSERT(m_stopping);
    if (GetThis() == this) {
        t_scheduler = nullptr;
    }
}

void Scheduler::start() {
    SYLAR_LOG_DEBUG(g_logger) << "start";
    MutexType::Lock lock{m_mutex};
    if (m_stopping) {
        SYLAR_LOG_ERROR(g_logger) << "Scheduler is stopped";
        return;
    }

    SYLAR_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);
    for (size_t i = 0; i < m_threadCount; ++i) {
        m_threads[i].reset(new Thread{std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)});
        m_threadIds.push_back(m_threads[i]->getId());
    }
}

bool Scheduler::stopping() {
    MutexType::Lock lock{m_mutex};
    return m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
}

void Scheduler::tickle() { SYLAR_LOG_DEBUG(g_logger) << "tickle"; }

void Scheduler::idle() {
    SYLAR_LOG_DEBUG(g_logger) << "idle";
    while (!stopping()) {
        sylar::Fiber::GetThis()->yield();
    }
}

void Scheduler::stop() {
    SYLAR_LOG_DEBUG(g_logger) << "stop";
    if (stopping()) {
        return;
    }
    m_stopping = true;

    if (m_useCaller) {
        SYLAR_ASSERT(GetThis() == this);
    } else {
        SYLAR_ASSERT(GetThis() != this);
    }

    for (size_t i = 0; i < m_threadCount; ++i) {
        tickle();
    }

    if (m_rootFiber) {
        tickle();
    }

    if (m_rootFiber) {
        m_rootFiber->resume();
        SYLAR_LOG_DEBUG(g_logger) << "m_rootFiber end";
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock{m_mutex};
        thrs.swap(m_threads);
    }

    for (const auto &thr : thrs) {
        thr->join();
    }
}

void Scheduler::run() {
    SYLAR_LOG_DEBUG(g_logger) << "run";
    setThis();
    if (sylar::GetThreadId() != m_rootThreadId) {
        t_scheduler_fiber = sylar::Fiber::GetThis().get();
    }

    Fiber::ptr idle_fiber{new Fiber{std::bind(&Scheduler::idle, this)}};
    Fiber::ptr cb_fiber;

    ScheduleTask task;
    while (true) {
        task.reset();
        bool tickle_me = false;
        {
            MutexType::Lock lock{m_mutex};
            auto itTask = m_tasks.begin();
            while (itTask != m_tasks.end()) {
                if (itTask->m_threadId != -1 && itTask->m_threadId != sylar::GetThreadId()) {
                    ++itTask;
                    tickle_me = true;
                    continue;
                }

                SYLAR_ASSERT(itTask->m_fiber || itTask->m_cb);
                if (itTask->m_fiber) {
                    SYLAR_ASSERT(itTask->m_fiber->getState() == Fiber::READY);
                }
                task = *itTask;
                m_tasks.erase(itTask++);
                ++m_activeThreadCount;
                break;
            }
            tickle_me = tickle_me || itTask != m_tasks.end();
        }

        if (tickle_me) {
            tickle();
        }

        if (task.m_fiber) {
            task.m_fiber->resume();
            --m_activeThreadCount;
            task.reset();
        } else if (task.m_cb) {
            if (cb_fiber) {
                cb_fiber->reset(task.m_cb);
            } else {
                cb_fiber.reset(new Fiber(task.m_cb));
            }
            task.reset();
            cb_fiber->resume();
            --m_activeThreadCount;
            cb_fiber.reset();
        } else {
            if (idle_fiber->getState() == Fiber::TERM) {
                SYLAR_LOG_DEBUG(g_logger) << "idle fiber term";
                break;
            }
            ++m_idleThreadCount;
            idle_fiber->resume();
            --m_idleThreadCount;
        }
    }
    SYLAR_LOG_DEBUG(g_logger) << "Scheduler::run() exit";
}

}  // namespace sylar