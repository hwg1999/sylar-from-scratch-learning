#include "iomanager.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <limits>
#include "macro.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

enum EpollCtlOp {};

static std::ostream &operator<<(std::ostream &os, const EpollCtlOp &op) {
    switch ((int)op) {
#define XX(ctl) \
    case ctl:   \
        return os << #ctl;
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
#undef XX
        default:
            return os << (int)op;
    }
}

static std::ostream &operator<<(std::ostream &os, EPOLL_EVENTS events) {
    if (!events) {
        return os << "0";
    }
    bool first = true;
#define XX(E)          \
    if (events & E) {  \
        if (!first) {  \
            os << "|"; \
        }              \
        os << #E;      \
        first = false; \
    }
    XX(EPOLLIN);
    XX(EPOLLPRI);
    XX(EPOLLOUT);
    XX(EPOLLRDNORM);
    XX(EPOLLRDBAND);
    XX(EPOLLWRNORM);
    XX(EPOLLWRBAND);
    XX(EPOLLMSG);
    XX(EPOLLERR);
    XX(EPOLLHUP);
    XX(EPOLLRDHUP);
    XX(EPOLLONESHOT);
    XX(EPOLLET);
#undef XX
    return os;
}

IOManager::FdContext::EventContext &IOManager::FdContext::getEventContext(IOManager::Event event) {
    switch (event) {
        case IOManager::READ:
            return m_readCtx;
        case IOManager::WRITE:
            return m_writeCtx;
        default:
            SYLAR_ASSERT2(false, "getContext");
    }
    throw std::invalid_argument{"getContext invalid event"};
}

void IOManager::FdContext::resetEventContext(EventContext &ctx) {
    ctx.m_scheduler = nullptr;
    ctx.m_fiber.reset();
    ctx.m_cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    SYLAR_ASSERT(m_events & event);

    m_events = static_cast<Event>(m_events & ~event);
    EventContext &ctx = getEventContext(event);
    if (ctx.m_cb) {
        ctx.m_scheduler->schedule(ctx.m_cb);
    } else {
        ctx.m_scheduler->schedule(ctx.m_fiber);
    }

    resetEventContext(ctx);

    return;
}

#define PIPE_R 0
#define PIPE_W 1

IOManager::IOManager(size_t threads, bool use_caller, const std::string &name) : Scheduler{threads, use_caller, name} {
    m_epfd = ::epoll_create1(EPOLL_CLOEXEC);
    SYLAR_ASSERT(m_epfd > 0);

    int ret = ::pipe(m_tickleFds);
    SYLAR_ASSERT(!ret);

    epoll_event event;
    std::memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[PIPE_R];

    ret = ::fcntl(m_tickleFds[PIPE_R], F_SETFL, O_NONBLOCK);
    SYLAR_ASSERT(!ret);

    ret = ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[PIPE_R], &event);
    SYLAR_ASSERT(!ret);

    contextResize(32);

    start();
}

IOManager::~IOManager() {
    stop();
    close(m_epfd);
    close(m_tickleFds[PIPE_R]);
    close(m_tickleFds[PIPE_W]);
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (!m_fdContexts[i]) {
            m_fdContexts[i] = std::make_unique<FdContext>();
            m_fdContexts[i]->m_fd = i;
        }
    }
}

bool IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    FdContext *fdCtx{nullptr};
    {
        RWMutexType::ReadLock rlock{m_mutex};
        if (static_cast<int>(m_fdContexts.size()) <= fd) {
            rlock.unlock();
            RWMutexType::WriteLock wlock{m_mutex};
            contextResize(fd * 1.5);
        }
        fdCtx = m_fdContexts[fd].get();
    }

    FdContext::MutexType::Lock lock{fdCtx->m_mutex};
    if (SYLAR_UNLIKELY(fdCtx->m_events & event)) {
        SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd << " event=" << static_cast<EPOLL_EVENTS>(event)
                                  << " fdCtx.event=" << static_cast<EPOLL_EVENTS>(fdCtx->m_events);
        SYLAR_ASSERT(!(fdCtx->m_events & event));
    }

    int op = fdCtx->m_events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epEvent;
    epEvent.events = EPOLLET | fdCtx->m_events | event;
    epEvent.data.ptr = fdCtx;

    int ret = ::epoll_ctl(m_epfd, op, fd, &epEvent);
    if (ret) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << static_cast<EpollCtlOp>(op) << ", " << fd << ", "
                                  << static_cast<EPOLL_EVENTS>(epEvent.events) << "):" << ret << " (" << errno << ") ("
                                  << strerror(errno)
                                  << ") fdCtx->events=" << static_cast<EPOLL_EVENTS>(fdCtx->m_events);
        return false;
    }

    ++m_pendingEventCount;

    fdCtx->m_events = static_cast<Event>(fdCtx->m_events | event);
    FdContext::EventContext &event_ctx = fdCtx->getEventContext(event);
    SYLAR_ASSERT(!event_ctx.m_scheduler && !event_ctx.m_fiber && !event_ctx.m_cb);

    event_ctx.m_scheduler = Scheduler::GetThis();
    if (cb) {
        event_ctx.m_cb.swap(cb);
    } else {
        event_ctx.m_fiber = Fiber::GetThis();
        SYLAR_ASSERT2(event_ctx.m_fiber->getState() == Fiber::RUNNING, "state=" << event_ctx.m_fiber->getState());
    }

    return true;
}

bool IOManager::delEvent(int fd, Event event) {
    FdContext *fdCtx = nullptr;
    {
        RWMutexType::ReadLock rlock{m_mutex};
        if (static_cast<int>(m_fdContexts.size()) <= fd) {
            return false;
        }
        fdCtx = m_fdContexts[fd].get();
    }

    FdContext::MutexType::Lock lock{fdCtx->m_mutex};
    if (SYLAR_UNLIKELY(!(fdCtx->m_events & event))) {
        return false;
    }

    Event new_events = static_cast<Event>(fdCtx->m_events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epEvent;
    epEvent.events = EPOLLET | new_events;
    epEvent.data.ptr = fdCtx;

    int ret = ::epoll_ctl(m_epfd, op, fd, &epEvent);
    if (ret) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << static_cast<EpollCtlOp>(op) << ", " << fd << ", "
                                  << static_cast<EPOLL_EVENTS>(epEvent.events) << "):" << ret << " (" << errno << ") ("
                                  << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    fdCtx->m_events = new_events;
    FdContext::EventContext &event_ctx = fdCtx->getEventContext(event);
    fdCtx->resetEventContext(event_ctx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    FdContext *fdCtx = nullptr;
    {
        RWMutexType::ReadLock rlock{m_mutex};
        if (static_cast<int>(m_fdContexts.size()) <= fd) {
            return false;
        }
        fdCtx = m_fdContexts[fd].get();
    }

    FdContext::MutexType::Lock lock{fdCtx->m_mutex};
    if (SYLAR_UNLIKELY(!(fdCtx->m_events & event))) {
        return false;
    }

    Event new_events = static_cast<Event>(fdCtx->m_events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epEvent;
    epEvent.events = EPOLLET | new_events;
    epEvent.data.ptr = fdCtx;

    int ret = ::epoll_ctl(m_epfd, op, fd, &epEvent);
    if (ret) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << static_cast<EpollCtlOp>(op) << ", " << fd << ", "
                                  << static_cast<EPOLL_EVENTS>(epEvent.events) << "):" << ret << " (" << errno << ") ("
                                  << strerror(errno) << ")";
        return false;
    }

    fdCtx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd) {
    FdContext *fdCtx = nullptr;
    {
        RWMutexType::ReadLock rlock{m_mutex};
        if (static_cast<int>(m_fdContexts.size()) <= fd) {
            return false;
        }
        fdCtx = m_fdContexts[fd].get();
    }

    FdContext::MutexType::Lock lock{fdCtx->m_mutex};
    if (!fdCtx->m_events) {
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epEvent;
    epEvent.events = NONE;
    epEvent.data.ptr = fdCtx;

    int ret = ::epoll_ctl(m_epfd, op, fd, &epEvent);
    if (ret) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << static_cast<EpollCtlOp>(op) << ", " << fd << ", "
                                  << static_cast<EPOLL_EVENTS>(epEvent.events) << "):" << ret << " (" << errno << ") ("
                                  << strerror(errno) << ")";
        return false;
    }

    if (fdCtx->m_events & READ) {
        fdCtx->triggerEvent(READ);
        --m_pendingEventCount;
    }

    if (fdCtx->m_events & WRITE) {
        fdCtx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    SYLAR_ASSERT(fdCtx->m_events == NONE);
    return true;
}

IOManager *IOManager::GetThis() { return dynamic_cast<IOManager *>(Scheduler::GetThis()); }

void IOManager::tickle() {
    SYLAR_LOG_DEBUG(g_logger) << "tickle";
    if (!hasIdleThreads()) {
        return;
    }

    int ret = ::write(m_tickleFds[PIPE_W], "T", 1);
    SYLAR_ASSERT(ret == 1);
}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}

bool IOManager::stopping(uint64_t &timeout) {
    timeout = getNextTimer();
    return timeout == std::numeric_limits<uint64_t>::max() && m_pendingEventCount == 0 && Scheduler::stopping();
}

void IOManager::idle() {
    SYLAR_LOG_DEBUG(g_logger) << "idle";

    constexpr uint64_t MAX_EVENTS = 256;
    epoll_event *events = new epoll_event[MAX_EVENTS]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr) { delete[] ptr; });

    while (true) {
        uint64_t nextTimeout = 0;
        if (SYLAR_UNLIKELY(stopping(nextTimeout))) {
            SYLAR_LOG_DEBUG(g_logger) << "name=" << getName() << "idle stopping exit";
            break;
        }

        int ret = 0;
        do {
            static constexpr int MAX_TIMEOUT = 5000;
            if (nextTimeout != std::numeric_limits<uint64_t>::max()) {
                nextTimeout = std::min(static_cast<int>(nextTimeout), MAX_TIMEOUT);
            } else {
                nextTimeout = MAX_TIMEOUT;
            }

            ret = ::epoll_wait(m_epfd, events, MAX_EVENTS, MAX_TIMEOUT);
            if (ret < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        } while (true);

        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if (!cbs.empty()) {
            for (const auto &cb : cbs) {
                schedule(cb);
            }
            cbs.clear();
        }

        for (int i = 0; i < ret; ++i) {
            epoll_event &event = events[i];
            if (event.data.fd == m_tickleFds[PIPE_R]) {
                uint8_t dummy[256];
                while (::read(m_tickleFds[PIPE_R], dummy, sizeof dummy) > 0);
                continue;
            }

            FdContext *fdCtx = static_cast<FdContext *>(event.data.ptr);
            FdContext::MutexType::Lock lock{fdCtx->m_mutex};

            if (event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fdCtx->m_events;
            }

            int realEvents = NONE;
            if (event.events & EPOLLIN) {
                realEvents |= READ;
            }

            if (event.events & EPOLLOUT) {
                realEvents |= WRITE;
            }

            if ((fdCtx->m_events & realEvents) == NONE) {
                continue;
            }

            int leftEvents = fdCtx->m_events & ~realEvents;
            int op = leftEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | leftEvents;

            ret = ::epoll_ctl(m_epfd, op, fdCtx->m_fd, &event);
            if (ret) {
                SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << static_cast<EpollCtlOp>(op) << ", "
                                          << fdCtx->m_fd << ", " << static_cast<EPOLL_EVENTS>(event.events)
                                          << "):" << ret << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            if (realEvents & READ) {
                fdCtx->triggerEvent(READ);
                --m_pendingEventCount;
            }

            if (realEvents & WRITE) {
                fdCtx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        Fiber::ptr cur = Fiber::GetThis();
        auto rawPtr = cur.get();
        cur.reset();

        rawPtr->yield();
    }
}

void IOManager::onTimerInsertedAtFront() { tickle(); }

}  // namespace sylar