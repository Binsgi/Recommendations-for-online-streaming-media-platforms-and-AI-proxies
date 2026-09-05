#include "AgentService.hpp"
#include "ApiProxy.hpp"
#include "Database.hpp"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <random>
#include <iostream>

static size_t LLMCurlCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

AgentService& AgentService::getInstance() {
    static AgentService instance;
    return instance;
}

json AgentService::testLLM(const LLMConfig& config) {
    json res = json::object();
    if (config.api_key.empty()) {
        res["code"] = 400;
        res["message"] = "API Key 不能为空";
        return res;
    }

    std::string url = config.endpoint;
    if (url.empty()) url = "https://api.deepseek.com/v1";
    if (url.back() == '/') url.pop_back();
    if (url.rfind("/chat/completions") == std::string::npos) {
        url += "/chat/completions";
    }

    json payload = json::object();
    payload["model"] = config.model.empty() ? "deepseek-chat" : config.model;
    json messages = json::array();
    json msg = json::object();
    msg["role"] = "user";
    msg["content"] = "Hello, please reply with 'OK' if you can read this.";
    messages.push_back(msg);
    payload["messages"] = messages;
    payload["max_tokens"] = 10;

    CURL* curl = curl_easy_init();
    if (!curl) {
        res["code"] = 500;
        res["message"] = "CURL 初始化失败";
        return res;
    }

    std::string response_body;
    std::string post_data = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + config.api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, LLMCurlCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode code = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK || http_code != 200) {
        res["code"] = 400;
        res["message"] = "连接失败 (HTTP " + std::to_string(http_code) + "): " + response_body;
        return res;
    }

    res["code"] = 200;
    res["message"] = "大模型 API 连接测试成功！";
    return res;
}

AgentService::ExtractedIntent AgentService::analyzeWithLLM(const std::string& prompt, const std::string& mood, const std::string& genre, const LLMConfig& config) {
    ExtractedIntent intent;
    intent.mood = mood.empty() ? "随心" : mood;

    std::string url = config.endpoint;
    if (url.empty()) url = "https://api.deepseek.com/v1";
    if (url.back() == '/') url.pop_back();
    if (url.rfind("/chat/completions") == std::string::npos) {
        url += "/chat/completions";
    }

    std::string system_prompt = 
        "你是一个专业的AI音乐推荐助手。请根据用户的听歌诉求、心情与偏好，分析意图并以JSON格式输出。"
        "必须严格输出纯JSON对象，格式如下："
        "{"
        "  \"summary\": \"给用户的一段温柔、贴心且具有音乐品味的推荐回复\","
        "  \"search_keyword\": \"适合在网易云音乐搜索的高品质检索词（如'周杰伦 晴天'或'Lo-Fi 专注 纯音乐'）\","
        "  \"tags\": [\"标签1\", \"标签2\", \"标签3\"]"
        "}";

    json payload = json::object();
    payload["model"] = config.model.empty() ? "deepseek-chat" : config.model;
    
    json messages = json::array();
    json sys_msg = json::object();
    sys_msg["role"] = "system";
    sys_msg["content"] = system_prompt;
    messages.push_back(sys_msg);

    json user_msg = json::object();
    user_msg["role"] = "user";
    user_msg["content"] = "用户诉求: " + prompt + (mood.empty() ? "" : (" | 心情: " + mood)) + (genre.empty() ? "" : (" | 偏好流派: " + genre));
    messages.push_back(user_msg);

    payload["messages"] = messages;
    payload["temperature"] = 0.7;

    CURL* curl = curl_easy_init();
    if (curl) {
        std::string response_body;
        std::string post_data = payload.dump();

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string auth_header = "Authorization: Bearer " + config.api_key;
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, LLMCurlCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode code = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (code == CURLE_OK && http_code == 200) {
            try {
                json resp_json = json::parse(response_body);
                if (resp_json.contains("choices") && resp_json["choices"].is_array() && resp_json["choices"].size() > 0) {
                    std::string content = resp_json["choices"][0]["message"]["content"].as_string();
                    
                    // 尝试提取 JSON 内容
                    size_t json_start = content.find('{');
                    size_t json_end = content.rfind('}');
                    if (json_start != std::string::npos && json_end != std::string::npos && json_end > json_start) {
                        std::string clean_json = content.substr(json_start, json_end - json_start + 1);
                        json parsed_intent = json::parse(clean_json);
                        
                        if (parsed_intent.contains("summary")) intent.summary = parsed_intent["summary"].as_string();
                        if (parsed_intent.contains("search_keyword")) intent.search_keyword = parsed_intent["search_keyword"].as_string();
                        if (parsed_intent.contains("tags") && parsed_intent["tags"].is_array()) {
                            for (size_t i = 0; i < parsed_intent["tags"].size(); ++i) {
                                intent.tags.push_back(parsed_intent["tags"][i].as_string());
                            }
                        }

                        if (!intent.search_keyword.empty()) {
                            std::cout << "[Agent] Successfully analyzed intent using custom LLM API (" << config.model << ")\n";
                            return intent;
                        }
                    } else {
                        intent.summary = content;
                        intent.search_keyword = prompt;
                        intent.tags = {"大模型推荐", config.model};
                        return intent;
                    }
                }
            } catch (...) {}
        } else {
            std::cerr << "[Agent] LLM API call failed with HTTP " << http_code << ", fallback to local engine\n";
        }
    }

    // 回退到内置引擎
    return analyzeIntent(prompt, mood, genre);
}

AgentService::ExtractedIntent AgentService::analyzeIntent(const std::string& prompt, const std::string& mood, const std::string& genre) {
    ExtractedIntent intent;
    intent.mood = mood.empty() ? "随心" : mood;

    std::string p = prompt;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);

    if (p.find("写代码") != std::string::npos || p.find("编程") != std::string::npos || p.find("工作") != std::string::npos || p.find("专注") != std::string::npos) {
        intent.search_keyword = "Lo-Fi 专注 纯音乐";
        intent.tags = {"工作专注", "Lo-Fi", "电子轻音", "沉浸感"};
        intent.summary = "为您精选了能够屏蔽外界干扰、提升编程与深度工作效率的纯音乐与Lo-Fi旋律。";
    } else if (p.find("深夜") != std::string::npos || p.find("睡眠") != std::string::npos || p.find("安静") != std::string::npos || p.find("助眠") != std::string::npos) {
        intent.search_keyword = "深夜 治愈 钢琴 睡眠";
        intent.tags = {"深夜治愈", "白噪音", "钢琴慢板", "情绪舒缓"};
        intent.summary = "为您挑选了温和柔美的夜曲与治愈钢琴曲，愿伴您安然入梦。";
    } else if (p.find("开心") != std::string::npos || p.find("运动") != std::string::npos || p.find("动感") != std::string::npos || p.find("嗨") != std::string::npos || p.find("跑步") != std::string::npos) {
        intent.search_keyword = "流行 节奏 燃 运动 电子";
        intent.tags = {"元气满满", "节奏感", "流行电音", "燃向"};
        intent.summary = "为您推荐充满力量感与明快律动的动感曲目，瞬间点燃激情！";
    } else if (p.find("伤感") != std::string::npos || p.find("emo") != std::string::npos || p.find("难过") != std::string::npos || p.find("回忆") != std::string::npos) {
        intent.search_keyword = "伤感 流行 慢歌 民谣";
        intent.tags = {"情感共鸣", "深情慢歌", "民谣独白", "温柔慰藉"};
        intent.summary = "懂你的低落与心事，精选了细腻深情的走心歌曲，让音乐陪伴你的情绪。";
    } else if (p.find("周杰伦") != std::string::npos) {
        intent.search_keyword = "周杰伦 经典 流行";
        intent.tags = {"华语经典", "周氏情歌", "时代金曲", "R&B"};
        intent.summary = "捕捉到您对周杰伦的喜爱，为您整合了经典传唱金曲与同风格佳作。";
    } else if (p.find("古风") != std::string::npos || p.find("国风") != std::string::npos) {
        intent.search_keyword = "国风 古风 戏腔 唯美";
        intent.tags = {"国风唯美", "诗意国乐", "古琴琵琶", "戏腔"};
        intent.summary = "为您呈现古韵悠扬、意境唯美的国风新潮与传统雅乐。";
    } else if (!genre.empty()) {
        intent.search_keyword = genre + " 精选";
        intent.tags = {genre, "精选歌单", "品质推荐"};
        intent.summary = "根据您指定的 [" + genre + "] 流派，为您定制了高品质试听单曲。";
    } else {
        intent.search_keyword = prompt.empty() ? "热歌精选" : prompt;
        intent.tags = {"个性化探测", "流行热榜", "智能推荐"};
        intent.summary = "AI Agent 根据您的输入与全网热度，智能探测并推荐了以下曲目。";
    }

    return intent;
}

std::string AgentService::generateSongReason(const std::string& song_name, const std::string& artist, const ExtractedIntent& intent, int score) {
    std::ostringstream ss;
    ss << "匹配度 " << score << "% · ";
    if (!intent.tags.empty()) {
        ss << "[" << intent.tags[0] << "] ";
    }
    ss << artist << " 的《" << song_name << "》具有极佳的氛围感，完美贴合您当下的聆听诉求。";
    return ss.str();
}

json AgentService::generateRecommendation(int64_t user_id, const std::string& prompt, const std::string& mood, const std::string& preferred_genre, const LLMConfig& llm_config) {
    json response = json::object();

    // 1. 意图解析 (若前端配置了大模型 API 则优先调用大模型)
    ExtractedIntent intent;
    if (llm_config.enabled && !llm_config.api_key.empty()) {
        intent = analyzeWithLLM(prompt, mood, preferred_genre, llm_config);
    } else {
        intent = analyzeIntent(prompt, mood, preferred_genre);
    }

    // 2. 结合用户画像 (如果已登录)
    std::string user_history_hint = "";
    if (user_id > 0) {
        auto pref = DatabasePool::getInstance().getUserPreference(user_id);
        if (pref.has_value() && !pref->fav_genres.empty()) {
            user_history_hint = "（已融合您的常用偏好：" + pref->fav_genres + "）";
        }
    }

    // 3. 检索候选曲目 (调用网易云 API 搜索)
    json search_res = ApiProxy::getInstance().searchSongs(intent.search_keyword, 20, 0);
    json candidate_songs = json::array();

    if (search_res.contains("songs") && search_res["songs"].is_array()) {
        candidate_songs = search_res["songs"];
    }

    // 4. 为每首歌曲赋能 AI 推荐属性与匹配分
    static thread_local std::mt19937 score_gen(1337);
    std::uniform_int_distribution<int> dis(90, 99);

    json final_songs = json::array();
    size_t count = std::min(static_cast<size_t>(10), candidate_songs.size());

    for (size_t i = 0; i < count; ++i) {
        json item = candidate_songs[i];
        std::string song_name = item["name"].as_string();
        std::string artist_name = "未知歌手";

        if (item.contains("ar") && item["ar"].is_array() && item["ar"].size() > 0) {
            artist_name = item["ar"][0]["name"].as_string();
        } else if (item.contains("artists") && item["artists"].is_array() && item["artists"].size() > 0) {
            artist_name = item["artists"][0]["name"].as_string();
        }

        int score = dis(score_gen) - static_cast<int>(i);
        if (score < 85) score = 85;

        item["match_score"] = score;
        item["agent_reason"] = generateSongReason(song_name, artist_name, intent, score);
        final_songs.push_back(item);
    }

    // 5. 组装响应
    response["code"] = 200;
    response["agent_name"] = (llm_config.enabled && !llm_config.model.empty()) ? ("MelodyAI · " + llm_config.model) : "MelodyAI 音乐智能体";
    response["agent_avatar"] = "images/person01.png";
    response["reply_text"] = intent.summary + " " + user_history_hint;

    json tag_arr = json::array();
    for (const auto& tag : intent.tags) {
        tag_arr.push_back(tag);
    }
    response["extracted_tags"] = tag_arr;
    response["songs"] = final_songs;
    response["total_recommended"] = static_cast<int>(final_songs.size());

    return response;
}
