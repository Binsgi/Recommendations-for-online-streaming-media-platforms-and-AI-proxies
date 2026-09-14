#include "Config.hpp"
#include "Database.hpp"
#include "Server.hpp"
#include <iostream>
#include <csignal>
#include <memory>

static std::unique_ptr<Server> g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down gracefully...\n";
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string config_path = (argc > 1) ? argv[1] : "config.ini";

    std::cout << "========================================================\n";
    std::cout << "       🎵 Demo Player C++17 Server (Ubuntu 22.04)        \n";
    std::cout << "========================================================\n";

    // 1. 加载配置文件
    Config& config = Config::getInstance();
    config.load(config_path);

    // 2. 初始化 MySQL 数据库连接池
    if (!DatabasePool::getInstance().init()) {
        std::cerr << "[Main] Warning: Failed to connect to MySQL! Check database service or run sql/init.sql.\n";
    }

    // 3. 注册系统信号
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 4. 启动 Epoll Reactor 服务器
    g_server = std::make_unique<Server>(config.server_port, config.server_threads);
    if (!g_server->start()) {
        std::cerr << "[Main] Server terminated with errors.\n";
        return 1;
    }

    std::cout << "[Main] Server exited cleanly.\n";
    return 0;
}
