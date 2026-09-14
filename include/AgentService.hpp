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

struct RecommendedTrack {
    std::string name;
    std::string artist;
    std::string reason;
};

struct ExtractedIntent {
    std::string summary;
    std::vector<std::string> tags;
    std::vector<std::string> search_keywords;
    std::vector<RecommendedTrack> specific_tracks;
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

    ExtractedIntent analyzeWithLLM(const std::string& prompt, const std::string& mood, const std::string& genre, const LLMConfig& config);
    ExtractedIntent analyzeWithKnowledgeBase(const std::string& prompt, const std::string& mood, const std::string& genre);
    
    // 根据具体推荐曲目列表 + 关键词检索并聚合网易云音乐信息
    json fetchSongsForIntent(const ExtractedIntent& intent);
};
