#include "fd_manager.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits>
#include "hook.h"

namespace sylar {

FdCtx::FdCtx(int fd)
    : m_isInit{false},
      m_isSocket{false},
      m_sysNonBlock{false},
      m_userNonBlock{false},
      m_isClosed{false},
      m_fd{fd},
      m_recvTimeout{std::numeric_limits<uint64_t>::max()},
      m_sendTimeout{std::numeric_limits<uint64_t>::max()} {
    init();
}

FdCtx::~FdCtx() {}

bool FdCtx::init() {
    if (m_isInit) {
        return true;
    }
    m_recvTimeout = std::numeric_limits<uint64_t>::max();
    m_sendTimeout = std::numeric_limits<uint64_t>::max();

    struct stat fdStat;
    if (-1 == fstat(m_fd, &fdStat)) {
        m_isInit = false;
        m_isSocket = false;
    } else {
        m_isInit = true;
        m_isSocket = S_ISSOCK(fdStat.st_mode);
    }

    if (m_isSocket) {
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        if (!(flags & O_NONBLOCK)) {
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNonBlock = true;
    } else {
        m_sysNonBlock = false;
    }

    m_userNonBlock = false;
    m_isClosed = false;
    return m_isInit;
}

void FdCtx::setTimeout(int type, uint64_t val) {
    if (type == SO_RCVTIMEO) {
        m_recvTimeout = val;
    } else {
        m_sendTimeout = val;
    }
}

uint64_t FdCtx::getTimeout(int type) {
    if (type == SO_RCVTIMEO) {
        return m_recvTimeout;
    }

    return m_sendTimeout;
}

FdManager::FdManager() { m_datas.resize(64); }

FdCtx::ptr FdManager::get(int fd, bool autoCreate) {
    if (fd == -1) {
        return nullptr;
    }

    {
        RWMutexType::ReadLock rlock{m_mutex};
        if (static_cast<int>(m_datas.size()) <= fd) {
            if (autoCreate == false) {
                return nullptr;
            }
        } else {
            if (m_datas[fd] || !autoCreate) {
                return m_datas[fd];
            }
        }
    }

    RWMutexType::WriteLock wlock{m_mutex};
    FdCtx::ptr ctx{new FdCtx{fd}};
    if (fd >= static_cast<int>(m_datas.size())) {
        m_datas.resize(fd * 1.5);
    }
    m_datas[fd] = ctx;
    return ctx;
}

void FdManager::del(int fd) {
    RWMutexType::WriteLock wlock{m_mutex};
    if (static_cast<int>(m_datas.size()) <= fd) {
        return;
    }
    m_datas[fd].reset();
}

}  // namespace sylar