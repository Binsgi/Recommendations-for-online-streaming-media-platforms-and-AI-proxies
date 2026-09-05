#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <optional>

struct TokenInfo {
    int64_t user_id = 0;
    std::string username;
    std::chrono::steady_clock::time_point expire_at;
};

struct CodeInfo {
    std::string code;
    std::chrono::steady_clock::time_point expire_at;
};

template <typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    std::optional<Value> get(const Key& key) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }
        // Move to front
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    void put(const Key& key, const Value& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }

        if (list_.size() >= capacity_) {
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
        }

        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    void remove(const Key& key) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
    }

    void clear() {
        std::unique_lock<std::mutex> lock(mutex_);
        list_.clear();
        map_.clear();
    }

    size_t size() {
        std::unique_lock<std::mutex> lock(mutex_);
        return list_.size();
    }

private:
    size_t capacity_;
    std::list<std::pair<Key, Value>> list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> map_;
    std::mutex mutex_;
};

class CacheManager {
public:
    static CacheManager& getInstance();

    // 1. LRU 内存歌词/元数据缓存 (避免磁盘膨胀)
    void cacheLyric(const std::string& song_id, const std::string& lyric_json);
    std::optional<std::string> getCachedLyric(const std::string& song_id);

    // 2. 验证码系统 (带 TTL)
    void storeVerificationCode(const std::string& target, const std::string& code, int ttl_sec);
    bool verifyCode(const std::string& target, const std::string& code, bool consume = true);

    // 3. Token 与 Refresh Token 管理
    std::string generateToken(int64_t user_id, const std::string& username, int ttl_sec);
    std::string generateRefreshToken(int64_t user_id, const std::string& username, int ttl_sec);
    std::optional<TokenInfo> validateToken(const std::string& token);
    std::optional<TokenInfo> validateRefreshToken(const std::string& refresh_token);
    void revokeToken(const std::string& token);

private:
    CacheManager();
    ~CacheManager() = default;
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    LRUCache<std::string, std::string> lyric_lru_;

    std::unordered_map<std::string, CodeInfo> codes_;
    std::mutex code_mutex_;

    std::unordered_map<std::string, TokenInfo> access_tokens_;
    std::unordered_map<std::string, TokenInfo> refresh_tokens_;
    std::shared_mutex token_mutex_;

    std::string generateRandomHex(size_t length);
};
