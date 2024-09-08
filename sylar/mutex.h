#pragma once

#include "noncopyable.h"

#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <cstdint>

namespace sylar {

class Semaphore : Noncopyable {
public:
    Semaphore(uint32_t count = 0);

    ~Semaphore();

    void wait();

    void notify();

private:
    sem_t m_semaphore;
};

template <typename T>
class ScopedLockImpl {
public:
    ScopedLockImpl(T &mutex) : m_mutex{mutex} {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl() { unlock(); }

    void lock() {
        if (!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_locked;
};

template <typename T>
struct ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T &mutex) : m_mutex{mutex} {
        m_mutex.rdlock();
        m_locked = true;
    }

    ~ReadScopedLockImpl() { unlock(); }

    void lock() {
        if (!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_locked;
};

template <typename T>
struct WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T &mutex) : m_mutex{mutex} {
        m_mutex.wrlock();
        m_locked = true;
    }

    ~WriteScopedLockImpl() { unlock(); }

    void lock() {
        if (!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_locked;
};

class Mutex : Noncopyable {
public:
    using Lock = ScopedLockImpl<Mutex>;

    Mutex() { pthread_mutex_init(&m_mutex, nullptr); }

    ~Mutex() { pthread_mutex_destroy(&m_mutex); }

    void lock() { pthread_mutex_lock(&m_mutex); }

    void unlock() { pthread_mutex_unlock(&m_mutex); }

private:
    pthread_mutex_t m_mutex;
};

class NullMutex : Noncopyable {
public:
    using Lock = ScopedLockImpl<NullMutex>;

    NullMutex() {}

    ~NullMutex() {}

    void lock() {}

    void unlock() {}
};

class RWMutex : Noncopyable {
public:
    using ReadLock = ReadScopedLockImpl<RWMutex>;
    using WriteLock = WriteScopedLockImpl<RWMutex>;

    RWMutex() { pthread_rwlock_init(&m_lock, nullptr); }

    ~RWMutex() { pthread_rwlock_destroy(&m_lock); }

    void rdlock() { pthread_rwlock_rdlock(&m_lock); }

    void wrlock() { pthread_rwlock_wrlock(&m_lock); }

    void unlock() { pthread_rwlock_unlock(&m_lock); }

private:
    pthread_rwlock_t m_lock;
};

class NullRWMutex : Noncopyable {
public:
    using ReadLock = ReadScopedLockImpl<NullMutex>;
    using WriteLock = WriteScopedLockImpl<NullMutex>;

    NullRWMutex() {}

    ~NullRWMutex() {}

    void rdlock() {}

    void wrlock() {}

    void unlock() {}
};

class SpinLock : Noncopyable {
public:
    using Lock = ScopedLockImpl<SpinLock>;

    SpinLock() { pthread_spin_init(&m_mutex, 0); }

    ~SpinLock() { pthread_spin_destroy(&m_mutex); }

    void lock() { pthread_spin_lock(&m_mutex); }

    void unlock() { pthread_spin_unlock(&m_mutex); }

private:
    pthread_spinlock_t m_mutex;
};

class CASLock : Noncopyable {
public:
    using Lock = ScopedLockImpl<CASLock>;

    CASLock() { m_mutex.clear(); }

    ~CASLock() {}

    void lock() { while (std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire)); }

    void unlock() { std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release); }

private:
    volatile std::atomic_flag m_mutex;
};

}  // namespace sylar