#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

std::string Config::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool Config::load(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[Config] Warning: Could not open " << config_file << ", using default settings.\n";
        return false;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            current_section = trim(current_section);
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string val = trim(line.substr(eq_pos + 1));

            if (current_section == "server") {
                if (key == "port") server_port = std::stoi(val);
                else if (key == "threads") server_threads = std::stoi(val);
                else if (key == "web_root") web_root = val;
            } else if (current_section == "mysql") {
                if (key == "host") mysql_host = val;
                else if (key == "port") mysql_port = std::stoi(val);
                else if (key == "user") mysql_user = val;
                else if (key == "password") mysql_password = val;
                else if (key == "database") mysql_database = val;
                else if (key == "pool_size") mysql_pool_size = std::stoi(val);
            } else if (current_section == "api") {
                if (key == "netease_api") netease_api = val;
                else if (key == "timeout_sec") api_timeout_sec = std::stoi(val);
            } else if (current_section == "cache") {
                if (key == "lru_capacity") lru_capacity = std::stoi(val);
                else if (key == "code_ttl") code_ttl = std::stoi(val);
                else if (key == "token_ttl") token_ttl = std::stoi(val);
                else if (key == "refresh_token_ttl") refresh_token_ttl = std::stoi(val);
            }
        }
    }

    std::cout << "[Config] Successfully loaded configuration from " << config_file << std::endl;
    return true;
}
