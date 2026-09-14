#pragma once

#include "ThreadPool.hpp"
#include <string>
#include <atomic>
#include <memory>

class Server {
public:
    Server(int port, int threads);
    ~Server();

    bool start();
    void stop();

private:
    int port_;
    int threads_count_;
    int listen_fd_ = -1;
    int epoll_fd_ = -1;
    std::atomic<bool> running_{false};
    std::unique_ptr<ThreadPool> thread_pool_;

    bool setNonBlocking(int fd);
    void handleClient(int client_fd);
};
