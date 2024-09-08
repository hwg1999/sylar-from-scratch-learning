#include "timer.h"
#include <limits>
#include "macro.h"
#include "util.h"

namespace sylar {

bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const {
    if (!lhs && !rhs) {
        return false;
    }
    if (!lhs) {
        return true;
    }
    if (!rhs) {
        return false;
    }
    if (lhs->m_next < rhs->m_next) {
        return true;
    }
    if (rhs->m_next < lhs->m_next) {
        return false;
    }
    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t interval, std::function<void()> cb, bool recurring, TimerManager *manager)
    : m_recurring{recurring}, m_interval{interval}, m_cb{cb}, m_manager{manager} {
    m_next = sylar::GetElapsedMS() + m_interval;
}

Timer::Timer(uint64_t next) : m_next{next} {}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock wlock{m_manager->m_mutex};
    if (m_cb) {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock wlock{m_manager->m_mutex};
    if (!m_cb) {
        return false;
    }

    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }

    m_manager->m_timers.erase(it);
    m_next = sylar::GetElapsedMS() + m_interval;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t interval, bool fromNow) {
    if (m_interval == interval && !fromNow) {
        return true;
    }

    TimerManager::RWMutexType::WriteLock lock{m_manager->m_mutex};
    if (!m_cb) {
        return false;
    }

    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }

    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if (fromNow) {
        start = sylar::GetElapsedMS();
    } else {
        start = m_next - m_interval;
    }
    m_interval = interval;
    m_next = start + m_interval;
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}

TimerManager::TimerManager() { m_previousTime = sylar::GetElapsedMS(); }

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t interval, std::function<void()> cb, bool recurring) {
    Timer::ptr timer{new Timer{interval, cb, recurring, this}};
    RWMutexType::WriteLock wlock{m_mutex};
    addTimer(timer, wlock);
    return timer;
}

static void OnTimer(std::weak_ptr<void> weakCond, std::function<void()> cb) {
    if (std::shared_ptr<void> guard = weakCond.lock(); guard) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t interval, std::function<void()> cb, std::weak_ptr<void> weakCond,
                                           bool recurring) {
    return addTimer(interval, std::bind(&OnTimer, weakCond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock rlock{m_mutex};
    m_tickled = false;
    if (m_timers.empty()) {
        return std::numeric_limits<uint64_t>::max();
    }

    const Timer::ptr &next = *m_timers.begin();
    uint64_t nowMs = sylar::GetElapsedMS();
    if (nowMs >= next->m_next) {
        return 0;
    }

    return next->m_next - nowMs;
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
    ;
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock rlock{m_mutex};
        if (m_timers.empty()) {
            return;
        }
    }

    RWMutexType::WriteLock wlock{m_mutex};
    if (m_timers.empty()) {
        return;
    }

    bool rollover = false;
    uint64_t nowMs = sylar::GetElapsedMS();
    if (SYLAR_UNLIKELY(detectClockRollover(nowMs))) {
        rollover = true;
    }

    if (!rollover && ((*m_timers.begin())->m_next > nowMs)) {
        return;
    }

    Timer::ptr nowTimer{new Timer{nowMs}};
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(nowTimer);
    while (it != m_timers.end() && (*it)->m_next == nowMs) {
        ++it;
    }

    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    cbs.reserve(expired.size());

    for (auto &timer : expired) {
        cbs.emplace_back(timer->m_cb);
        if (timer->m_recurring) {
            timer->m_next = nowMs + timer->m_interval;
            m_timers.insert(timer);
        } else {
            timer->m_cb = nullptr;
        }
    }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock &wlock) {
    auto [it, res] = m_timers.insert(val);
    bool atFront = (it == m_timers.begin()) && !m_tickled;
    if (atFront) {
        m_tickled = true;
    }
    wlock.unlock();

    if (atFront) {
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectClockRollover(uint64_t nowMs) {
    bool rollover = false;
    if (nowMs < m_previousTime && nowMs < (m_previousTime - 60 * 60 * 1000)) {
        rollover = true;
    }
    m_previousTime = nowMs;
    return rollover;
}

bool TimerManager::hasTimer() {
    RWMutexType::ReadLock rlock{m_mutex};
    return !m_timers.empty();
}

}  // namespace sylar