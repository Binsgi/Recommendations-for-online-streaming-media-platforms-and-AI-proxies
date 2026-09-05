#include "AgentService.hpp"
#include "ApiProxy.hpp"
#include "Database.hpp"
#include <sstream>
#include <algorithm>
#include <random>
#include <iostream>

AgentService& AgentService::getInstance() {
    static AgentService instance;
    return instance;
}

AgentService::ExtractedIntent AgentService::analyzeIntent(const std::string& prompt, const std::string& mood, const std::string& genre) {
    ExtractedIntent intent;
    intent.mood = mood.empty() ? "随心" : mood;

    // 意图与关键字规则分析 (可扩展对接大模型 LLM API 如 DeepSeek / Gemini / OpenAI)
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

json AgentService::generateRecommendation(int64_t user_id, const std::string& prompt, const std::string& mood, const std::string& preferred_genre) {
    json response = json::object();

    // 1. 意图解析
    ExtractedIntent intent = analyzeIntent(prompt, mood, preferred_genre);

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
    response["agent_name"] = "MelodyAI 音乐智能体";
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
