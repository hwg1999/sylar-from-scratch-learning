#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "sylar/sylar.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep() {
    SYLAR_LOG_INFO(g_logger) << "test_sleep begin";
    sylar::IOManager iom;

    iom.schedule([] {
        sleep(2);
        SYLAR_LOG_INFO(g_logger) << "sleep 2";
    });

    iom.schedule([] {
        sleep(3);
        SYLAR_LOG_INFO(g_logger) << "sleep 3";
    });

    SYLAR_LOG_INFO(g_logger) << "test_sleep end";
}

void test_sock() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    SYLAR_LOG_INFO(g_logger) << "begin connect";
    int ret = connect(sock, (const sockaddr *)&addr, sizeof(addr));
    SYLAR_LOG_INFO(g_logger) << "connect ret = " << ret << " errno = " << errno;
    if (ret) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    ret = send(sock, data, sizeof(data), 0);
    SYLAR_LOG_INFO(g_logger) << "send rt=" << ret << " errno=" << errno;
    if (ret <= 0) {
        return;
    }

    std::string buf;
    buf.resize(4096);
    ret = recv(sock, buf.data(), buf.size(), 0);
    SYLAR_LOG_INFO(g_logger) << "recv ret = " << ret << " errno = " << errno;
    if (ret <= 0) {
        return;
    }

    buf.resize(ret);
    SYLAR_LOG_INFO(g_logger) << buf;
}

int main(int argc, char *argv[]) {
    sylar::EnvMgr::GetInstance()->init(argc, argv);
    sylar::Config::LoadFromConfDir(sylar::EnvMgr::GetInstance()->getConfigPath());

    test_sleep();

    // sylar::IOManager iom;
    // iom.schedule(test_sock);

    SYLAR_LOG_INFO(g_logger) << "main end";
    return 0;
}