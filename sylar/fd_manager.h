#pragma once

#include <memory>
#include <vector>
#include "mutex.h"
#include "singleton.h"

namespace sylar {

class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    using ptr = std::shared_ptr<FdCtx>;

    FdCtx(int fd);

    ~FdCtx();

    bool isInit() const { return m_isInit; }

    bool isSocket() const { return m_isSocket; }

    bool isClose() const { return m_isClosed; }

    void setUserNonblock(bool val) { m_userNonBlock = val; }

    bool getUserNonBlock() const { return m_userNonBlock; }

    void setSysNonBlock(bool val) { m_sysNonBlock = val; }

    bool getSysNonBlock() const { return m_sysNonBlock; }

    void setTimeout(int type, uint64_t val);

    uint64_t getTimeout(int type);

private:
    bool init();

private:
    bool m_isInit : 1;
    bool m_isSocket : 1;
    bool m_sysNonBlock : 1;
    bool m_userNonBlock : 1;
    bool m_isClosed : 1;
    int m_fd;
    uint64_t m_recvTimeout;
    uint64_t m_sendTimeout;
};

class FdManager {
public:
    using RWMutexType = RWMutex;

    FdManager();

    FdCtx::ptr get(int fd, bool autoCreate = false);

    void del(int fd);

private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_datas;
};

using FdMgr = Singleton<FdManager>;

}  // namespace sylar