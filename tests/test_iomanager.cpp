#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int sockfd;
void watch_io_read();

void do_io_write() {
    SYLAR_LOG_INFO(g_logger) << "write callback";
    int soErr;
    socklen_t len = sizeof(socklen_t);
    ::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &soErr, &len);
    if (soErr) {
        SYLAR_LOG_INFO(g_logger) << "connect failed";
        return;
    }
    SYLAR_LOG_INFO(g_logger) << "connect success";
}

void do_io_read() {
    SYLAR_LOG_INFO(g_logger) << "read callback";
    char buf[1024] = {0};
    int readLen = ::read(sockfd, buf, sizeof buf);
    if (readLen > 0) {
        buf[readLen] = '\0';
        SYLAR_LOG_INFO(g_logger) << "read " << readLen << " bytes, read: " << buf;
    } else if (readLen == 0) {
        SYLAR_LOG_INFO(g_logger) << "peer closed";
        close(sockfd);
        return;
    } else {
        SYLAR_LOG_ERROR(g_logger) << "err, errno=" << errno << ", errstr=" << strerror(errno);
        close(sockfd);
        return;
    }

    sylar::IOManager::GetThis()->schedule(watch_io_read);
}

void watch_io_read() {
    SYLAR_LOG_INFO(g_logger) << "watch_io_read";
    sylar::IOManager::GetThis()->addEvent(sockfd, sylar::IOManager::READ, do_io_read);
}

void test_io() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    SYLAR_ASSERT(sockfd > 0);
    ::fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in servaddr;
    std::memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1234);
    ::inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr.s_addr);

    int ret = ::connect(sockfd, (const sockaddr *)&servaddr, sizeof servaddr);
    if (ret != 0) {
        if (errno == EINPROGRESS) {
            SYLAR_LOG_INFO(g_logger) << "EINPROGRESS";
            sylar::IOManager::GetThis()->addEvent(sockfd, sylar::IOManager::WRITE, do_io_write);
            sylar::IOManager::GetThis()->addEvent(sockfd, sylar::IOManager::READ, do_io_read);
        } else {
            SYLAR_LOG_ERROR(g_logger) << "connect error, errno:" << errno << ", errstr:" << strerror(errno);
        }
    } else {
        SYLAR_LOG_ERROR(g_logger) << "else, errno:" << errno << ", errstr:" << strerror(errno);
    }
}

void test_iomanager() {
    sylar::IOManager iom;
    iom.schedule(test_io);
}

int main(int argc, char *argv[]) {
    sylar::EnvMgr::GetInstance()->init(argc, argv);
    sylar::Config::LoadFromConfDir(sylar::EnvMgr::GetInstance()->getConfigPath());

    test_iomanager();

    return 0;
}