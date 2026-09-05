#pragma once

#include "json.hpp"
#include <string>
#include <vector>

struct LLMConfig {
    std::string endpoint = "https://api.deepseek.com/v1";
    std::string api_key = "";
    std::string model = "deepseek-chat";
    bool enabled = false;
};

class AgentService {
public:
    static AgentService& getInstance();

    // 智能个性化推荐入口 (支持传入前端配置的大模型 API)
    json generateRecommendation(int64_t user_id, 
                                 const std::string& prompt, 
                                 const std::string& mood = "", 
                                 const std::string& preferred_genre = "",
                                 const LLMConfig& llm_config = {});

    // 测试前端输入的 LLM API 是否有效
    json testLLM(const LLMConfig& config);

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

    ExtractedIntent analyzeWithLLM(const std::string& prompt, const std::string& mood, const std::string& genre, const LLMConfig& config);
    ExtractedIntent analyzeIntent(const std::string& prompt, const std::string& mood, const std::string& genre);
    std::string generateSongReason(const std::string& song_name, const std::string& artist, const ExtractedIntent& intent, int score);
};
