#include "Server.hpp"
#include "Http.hpp"
#include "Router.hpp"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <vector>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 65536

Server::Server(int port, int threads)
    : port_(port), threads_count_(threads) {
    thread_pool_ = std::make_unique<ThreadPool>(threads_count_);
}

Server::~Server() {
    stop();
}

bool Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

void Server::handleClient(int client_fd) {
    std::string raw_request;
    char buffer[4096];

    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            raw_request.append(buffer, bytes_read);
            if (raw_request.find("\r\n\r\n") != std::string::npos) {
                // If Content-Length present, check if full body received
                size_t cl_pos = raw_request.find("Content-Length: ");
                if (cl_pos == std::string::npos) {
                    cl_pos = raw_request.find("content-length: ");
                }
                if (cl_pos != std::string::npos) {
                    size_t cl_end = raw_request.find("\r\n", cl_pos);
                    int content_len = std::stoi(raw_request.substr(cl_pos + 16, cl_end - cl_pos - 16));
                    size_t body_start = raw_request.find("\r\n\r\n") + 4;
                    if (raw_request.length() - body_start >= static_cast<size_t>(content_len)) {
                        break;
                    }
                } else {
                    break;
                }
            }
        } else if (bytes_read == 0) {
            // Client closed connection
            close(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            close(client_fd);
            return;
        }
    }

    if (!raw_request.empty()) {
        HttpRequest req = HttpRequest::parse(raw_request);
        HttpResponse res = Router::getInstance().dispatch(req);
        std::string serialized = res.serialize();

        size_t total_sent = 0;
        while (total_sent < serialized.length()) {
            ssize_t sent = write(client_fd, serialized.c_str() + total_sent, serialized.length() - total_sent);
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(1000);
                    continue;
                }
                break;
            }
            total_sent += sent;
        }
    }

    close(client_fd);
}

bool Server::start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::cerr << "[Server] Failed to create socket\n";
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(listen_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "[Server] Failed to bind to port " << port_ << "\n";
        close(listen_fd_);
        return false;
    }

    if (listen(listen_fd_, 1024) < 0) {
        std::cerr << "[Server] Failed to listen on socket\n";
        close(listen_fd_);
        return false;
    }

    setNonBlocking(listen_fd_);

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "[Server] Failed to create epoll instance\n";
        close(listen_fd_);
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
        std::cerr << "[Server] Failed to add listen_fd to epoll\n";
        close(listen_fd_);
        close(epoll_fd_);
        return false;
    }

    running_ = true;
    std::cout << "[Server] ==============================================\n";
    std::cout << "[Server] 🚀 DemoPlayer Server is running on port " << port_ << "\n";
    std::cout << "[Server] 🌐 Web Interface: http://localhost:" << port_ << "\n";
    std::cout << "[Server] ⚡ Epoll Reactor with " << threads_count_ << " worker threads\n";
    std::cout << "[Server] ==============================================\n";

    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[Server] epoll_wait error\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == listen_fd_) {
                // New incoming connection
                while (true) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int conn_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
                    if (conn_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // All incoming connections drained
                        }
                        break;
                    }

                    setNonBlocking(conn_fd);

                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                    client_ev.data.fd = conn_fd;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, conn_fd, &client_ev);
                }
            } else if (events[i].events & EPOLLIN) {
                // Remove from epoll to prevent duplicate triggers
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                // Dispatch to ThreadPool
                thread_pool_->enqueue([this, fd]() {
                    this->handleClient(fd);
                });
            }
        }
    }

    return true;
}

void Server::stop() {
    if (running_) {
        running_ = false;
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
        if (listen_fd_ >= 0) {
            close(listen_fd_);
            listen_fd_ = -1;
        }
        std::cout << "[Server] Server stopped.\n";
    }
}
