#pragma once

#include "json.hpp"
#include <string>
#include <vector>

class AgentService {
public:
    static AgentService& getInstance();

    // 智能个性化推荐入口
    json generateRecommendation(int64_t user_id, const std::string& prompt, const std::string& mood = "", const std::string& preferred_genre = "");

private:
    AgentService() = default;
    ~AgentService() = default;
    AgentService(const AgentService&) = delete;
    AgentService& operator=(const AgentService&) = delete;

    struct ExtractedIntent {
        std::string search_keyword;
        std::string mood;
        std::vector<std::string> tags;
        std::string summary;
    };

    ExtractedIntent analyzeIntent(const std::string& prompt, const std::string& mood, const std::string& genre);
    std::string generateSongReason(const std::string& song_name, const std::string& artist, const ExtractedIntent& intent, int score);
};
