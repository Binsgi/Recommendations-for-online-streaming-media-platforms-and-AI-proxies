#include "Cache.hpp"
#include "Config.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

CacheManager::CacheManager() 
    : lyric_lru_(Config::getInstance().lru_capacity) {}

CacheManager& CacheManager::getInstance() {
    static CacheManager instance;
    return instance;
}

std::string CacheManager::generateRandomHex(size_t length) {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream ss;
    while (ss.str().length() < length) {
        ss << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
    }
    return ss.str().substr(0, length);
}

void CacheManager::cacheLyric(const std::string& song_id, const std::string& lyric_json) {
    lyric_lru_.put(song_id, lyric_json);
}

std::optional<std::string> CacheManager::getCachedLyric(const std::string& song_id) {
    return lyric_lru_.get(song_id);
}

void CacheManager::storeVerificationCode(const std::string& target, const std::string& code, int ttl_sec) {
    std::unique_lock<std::mutex> lock(code_mutex_);
    auto expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_sec);
    codes_[target] = {code, expire_at};
}

bool CacheManager::verifyCode(const std::string& target, const std::string& code, bool consume) {
    std::unique_lock<std::mutex> lock(code_mutex_);
    auto it = codes_.find(target);
    if (it == codes_.end()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    if (now > it->second.expire_at) {
        codes_.erase(it);
        return false;
    }

    if (it->second.code == code) {
        if (consume) {
            codes_.erase(it);
        }
        return true;
    }
    return false;
}

std::string CacheManager::generateToken(int64_t user_id, const std::string& username, int ttl_sec) {
    std::unique_lock<std::shared_mutex> lock(token_mutex_);
    std::string token = "tk_" + generateRandomHex(32);
    auto expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_sec);
    access_tokens_[token] = {user_id, username, expire_at};
    return token;
}

std::string CacheManager::generateRefreshToken(int64_t user_id, const std::string& username, int ttl_sec) {
    std::unique_lock<std::shared_mutex> lock(token_mutex_);
    std::string refresh_token = "rf_" + generateRandomHex(32);
    auto expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_sec);
    refresh_tokens_[refresh_token] = {user_id, username, expire_at};
    return refresh_token;
}

std::optional<TokenInfo> CacheManager::validateToken(const std::string& token) {
    std::shared_lock<std::shared_mutex> lock(token_mutex_);
    auto it = access_tokens_.find(token);
    if (it == access_tokens_.end()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    if (now > it->second.expire_at) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<TokenInfo> CacheManager::validateRefreshToken(const std::string& refresh_token) {
    std::shared_lock<std::shared_mutex> lock(token_mutex_);
    auto it = refresh_tokens_.find(refresh_token);
    if (it == refresh_tokens_.end()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    if (now > it->second.expire_at) {
        return std::nullopt;
    }
    return it->second;
}

void CacheManager::revokeToken(const std::string& token) {
    std::unique_lock<std::shared_mutex> lock(token_mutex_);
    access_tokens_.erase(token);
    refresh_tokens_.erase(token);
}
