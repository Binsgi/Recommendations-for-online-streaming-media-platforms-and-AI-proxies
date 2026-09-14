#pragma once

#include <string>
#include <map>

class Config {
public:
    static Config& getInstance();

    bool load(const std::string& config_file = "config.ini");

    // Server
    int server_port = 8080;
    int server_threads = 8;
    std::string web_root = "./web";

    // MySQL
    std::string mysql_host = "127.0.0.1";
    int mysql_port = 3306;
    std::string mysql_user = "root";
    std::string mysql_password = "root";
    std::string mysql_database = "demo_player";
    int mysql_pool_size = 10;

    // Netease API
    std::string netease_api = "http://10.95.6.100:3000";
    int api_timeout_sec = 5;

    // Cache & Security
    int lru_capacity = 1000;
    int code_ttl = 300;               // 5 minutes
    int token_ttl = 86400;            // 24 hours
    int refresh_token_ttl = 604800;   // 7 days

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::string trim(const std::string& str);
};
