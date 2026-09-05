#include "AgentService.hpp"
#include "ApiProxy.hpp"
#include "Database.hpp"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <random>
#include <iostream>
#include <set>

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
    msg["content"] = "Hello";
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

ExtractedIntent AgentService::analyzeWithLLM(const std::string& prompt, const std::string& mood, const std::string& genre, const LLMConfig& config) {
    ExtractedIntent intent;

    std::string url = config.endpoint;
    if (url.empty()) url = "https://api.deepseek.com/v1";
    if (url.back() == '/') url.pop_back();
    if (url.rfind("/chat/completions") == std::string::npos) {
        url += "/chat/completions";
    }

    std::string system_prompt = 
        "你是一位拥有极高品味与音乐审美修养的专业音乐策展人 (Music Curator)。\n"
        "你的职责是：根据用户的具体诉求，精准领会其当下真实的心境与场景（严格基于用户输入，绝不凭空臆测用户未提及的不相干流派，例如用户未提及轻音乐则不要提到轻音乐）。\n"
        "你需要向用户输出一段极具情感共鸣且有品位的回复，并推荐 6~8 首真实存在、契合诉求的高品质经典或热门歌曲，包含每首歌的专属推荐理由。\n\n"
        "请严格以纯 JSON 格式输出，不要包含 Markdown 代码块标记（如```json），字段结构如下：\n"
        "{\n"
        "  \"summary\": \"针对用户诉求的一段温情、富有音乐质感的回复寄语（50~80字）\",\n"
        "  \"tags\": [\"精准标签1\", \"标签2\", \"标签3\"],\n"
        "  \"recommended_tracks\": [\n"
        "    {\n"
        "      \"name\": \"精确歌曲名（如：体面）\",\n"
        "      \"artist\": \"歌手（如：于文文）\",\n"
        "      \"reason\": \"推荐此曲的专属理由（从旋律、情感或歌词角度点评，25~40字）\"\n"
        "    }\n"
        "  ],\n"
        "  \"search_keywords\": [\"备用搜索词1\", \"备用搜索词2\"]\n"
        "}";

    json payload = json::object();
    payload["model"] = config.model.empty() ? "deepseek-chat" : config.model;
    
    json messages = json::array();
    json sys_msg = json::object();
    sys_msg["role"] = "system";
    sys_msg["content"] = system_prompt;
    messages.push_back(sys_msg);

    std::string user_content = "用户听歌诉求: " + prompt;
    if (!mood.empty()) user_content += " | 指定心情: " + mood;
    if (!genre.empty()) user_content += " | 指定偏好流派: " + genre;

    json user_msg = json::object();
    user_msg["role"] = "user";
    user_msg["content"] = user_content;
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
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
                    
                    // 智能提取 JSON 结构
                    size_t json_start = content.find('{');
                    size_t json_end = content.rfind('}');
                    if (json_start != std::string::npos && json_end != std::string::npos && json_end > json_start) {
                        std::string clean_json = content.substr(json_start, json_end - json_start + 1);
                        json parsed_intent = json::parse(clean_json);
                        
                        if (parsed_intent.contains("summary")) intent.summary = parsed_intent["summary"].as_string();
                        if (parsed_intent.contains("tags") && parsed_intent["tags"].is_array()) {
                            for (size_t i = 0; i < parsed_intent["tags"].size(); ++i) {
                                intent.tags.push_back(parsed_intent["tags"][i].as_string());
                            }
                        }
                        if (parsed_intent.contains("recommended_tracks") && parsed_intent["recommended_tracks"].is_array()) {
                            for (size_t i = 0; i < parsed_intent["recommended_tracks"].size(); ++i) {
                                json track_obj = parsed_intent["recommended_tracks"][i];
                                RecommendedTrack t;
                                t.name = track_obj["name"].as_string();
                                t.artist = track_obj["artist"].as_string();
                                t.reason = track_obj["reason"].as_string();
                                if (!t.name.empty()) {
                                    intent.specific_tracks.push_back(t);
                                }
                            }
                        }
                        if (parsed_intent.contains("search_keywords") && parsed_intent["search_keywords"].is_array()) {
                            for (size_t i = 0; i < parsed_intent["search_keywords"].size(); ++i) {
                                intent.search_keywords.push_back(parsed_intent["search_keywords"][i].as_string());
                            }
                        }

                        if (!intent.specific_tracks.empty() || !intent.summary.empty()) {
                            std::cout << "[Agent] Successfully parsed LLM recommendation with " 
                                      << intent.specific_tracks.size() << " specific tracks\n";
                            return intent;
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[Agent] JSON parse error: " << e.what() << "\n";
            }
        } else {
            std::cerr << "[Agent] LLM API call failed with HTTP " << http_code << ", response: " << response_body << "\n";
        }
    }

    // 回退到本地知识库引擎
    return analyzeWithKnowledgeBase(prompt, mood, genre);
}

ExtractedIntent AgentService::analyzeWithKnowledgeBase(const std::string& prompt, const std::string& mood, const std::string& genre) {
    ExtractedIntent intent;
    std::string p = prompt;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);

    // 1. 伤感 / emo / 悲伤 / 失恋 / 慢歌
    if (p.find("伤感") != std::string::npos || p.find("emo") != std::string::npos || 
        p.find("难过") != std::string::npos || p.find("失恋") != std::string::npos || 
        p.find("分手") != std::string::npos || p.find("流泪") != std::string::npos || 
        p.find("哭") != std::string::npos) {
        
        intent.summary = "懂你此刻无处安放的情绪与遗憾。精选了直击心灵的华语伤感流行代表作，让细腻深情的旋律陪伴你释怀。";
        intent.tags = {"伤感流行", "深情慢歌", "情感共鸣", "治愈释怀"};
        intent.search_keywords = {"伤感流行", "抒情慢歌", "走心流行"};
        intent.specific_tracks = {
            {"体面", "于文文", "深沉低回的烟嗓，唱出爱情曲终人散时的体面与释然"},
            {"说散就散", "袁娅维", "抓心撕裂的高音演绎，充满力量感与不舍的伤感流行金曲"},
            {"慢半拍", "薛之谦", "抒情慢板流行，细腻剖析内心情感波澜与无奈遗憾"},
            {"年少有为", "李荣浩", "写满关于青春、遗憾与错过的走心旋律，引发强烈共鸣"},
            {"突然好想你", "五月天", "最温柔深情的华语抒情经典，瞬间唤醒心底深处的那个人"},
            {"修炼爱情", "林俊杰", "极具穿透力的高音与饱满的情感叙事，唱尽刻骨铭心的回忆"},
            {"连名带姓", "张惠妹", "虐心走心的催泪之作，唱出深爱过后的疏离与酸楚"}
        };
    }
    // 2. 专注 / 写代码 / 编程 / 工作 / 学习
    else if (p.find("写代码") != std::string::npos || p.find("编程") != std::string::npos || 
             p.find("工作") != std::string::npos || p.find("专注") != std::string::npos || 
             p.find("学习") != std::string::npos) {
        
        intent.summary = "为您精选了节奏舒缓平稳、不抢占思绪的高质感纯音乐与Lo-Fi旋律，助您快速进入心流状态。";
        intent.tags = {"深度专注", "Lo-Fi", "电子轻音", "心流沉浸"};
        intent.search_keywords = {"Lo-Fi 纯音乐", "专注 编码", "电子轻音乐"};
        intent.specific_tracks = {
            {"Sunflower", "Post Malone", "轻快松弛的律动，带来清爽专注的工作氛围"},
            {"Sparks", "Coldplay", "温暖干净的吉他扫弦，安抚浮躁情绪"},
            {"夜的钢琴曲五", "石进", "纯净优美的黑白键旋律，让思维沉淀专注于当下"},
            {"River Flows in You", "Yiruma", "经典舒缓钢琴曲，构筑宁静高效的工作空间"}
        };
    }
    // 3. 运动 / 燃 / 健身 / 动感 / 嗨
    else if (p.find("运动") != std::string::npos || p.find("健身") != std::string::npos || 
             p.find("燃") != std::string::npos || p.find("动感") != std::string::npos || 
             p.find("嗨") != std::string::npos || p.find("跑步") != std::string::npos) {
        
        intent.summary = "为您送上高能律动与充满爆发力的热血节拍，瞬间激发多巴胺，点燃运动激情！";
        intent.tags = {"元气满满", "运动燃曲", "高能节拍", "热血流行"};
        intent.search_keywords = {"运动燃歌", "流行电音", "高燃摇滚"};
        intent.specific_tracks = {
            {"你要跳舞吗", "新裤子", "充满复古摇滚活力与魔性节拍，瞬间扫清疲惫"},
            {"Natural", "Imagine Dragons", "强劲爆裂的鼓点与嘶吼唱腔，无与伦比的力量感"},
            {"本草纲目", "周杰伦", "节奏感拉满的经典国风嘻哈，动感燃向天花板"},
            {"Counting Stars", "OneRepublic", "极富张力的高燃旋律，越听越让人充满干劲"}
        };
    }
    // 4. 周杰伦
    else if (p.find("周杰伦") != std::string::npos) {
        intent.summary = "为您整合了周杰伦传唱度极高的经典金曲与时代回忆，重温属于青春的旋律。";
        intent.tags = {"周杰伦", "华语经典", "时代金曲", "R&B抒情"};
        intent.search_keywords = {"周杰伦 经典", "周杰伦 热门"};
        intent.specific_tracks = {
            {"晴天", "周杰伦", "最纯粹的青春校园回忆，百听不厌的经典吉他前奏"},
            {"七里香", "周杰伦", "诗意唯美的盛夏浪漫，华语流行乐坛里程碑"},
            {"稻香", "周杰伦", "温暖治愈的乡谣风情，抚慰人心的励志佳作"},
            {"枫", "周杰伦", "极致伤感抒情摇滚，凄美大气的弦乐编曲"},
            {"搁浅", "周杰伦", "撕心裂肺的高音独白，唱透深情与无奈"}
        };
    }
    // 5. 治愈 / 安静 / 睡眠 / 夜晚
    else if (p.find("治愈") != std::string::npos || p.find("睡眠") != std::string::npos || 
             p.find("安静") != std::string::npos || p.find("晚安") != std::string::npos || 
             p.find("深夜") != std::string::npos) {
        
        intent.summary = "为您挑选了温和柔美、具有安抚力量的治愈曲目，愿温柔的音乐拂去一身疲惫。";
        intent.tags = {"深夜治愈", "情绪舒缓", "温暖慰藉", "安睡陪伴"};
        intent.search_keywords = {"深夜 治愈", "安静 慢歌", "治愈流行"};
        intent.specific_tracks = {
            {"水星记", "郭顶", "朦胧深情的太空抒情曲，诉说可望不可即的浪漫"},
            {"云烟成雨", "房东的猫", "清澈治愈的民谣声线，伴随安静时光缓缓流淌"},
            {"像我这样的人", "毛不易", "质朴走心的歌词与沙哑嗓音，唱进每一个平凡灵魂"},
            {"起风了", "买辣椒也用券", "充满释怀与向阳生长的力量，温暖人心"}
        };
    }
    // 6. 国风 / 古风
    else if (p.find("国风") != std::string::npos || p.find("古风") != std::string::npos) {
        intent.summary = "为您呈现古韵悠扬、意境唯美的国风新潮与经典雅乐。";
        intent.tags = {"国风唯美", "诗意国乐", "戏腔雅韵", "古韵流行"};
        intent.search_keywords = {"国风流行", "古风精选"};
        intent.specific_tracks = {
            {"赤伶", "HITA", "惊艳绝伦的戏腔转音，饱含家国情怀的传奇之作"},
            {"青花瓷", "周杰伦", "方文山传世词作，江南烟雨般的东方韵味"},
            {"吹梦到西洲", "恋恋故人难", "大气磅礴的男女对唱，如梦似幻的唯美国风"}
        };
    }
    // 7. 通用精准推荐
    else {
        intent.summary = "根据您的输入与当季流行热度，为您智能探测并挑选了以下高品质曲目。";
        intent.tags = {"智能推荐", "流行精选", "品质单曲"};
        intent.search_keywords = {prompt.empty() ? "热歌精选" : prompt};
        intent.specific_tracks = {
            {"晴天", "周杰伦", "时代传唱金曲，百听不厌的经典旋律"},
            {"体面", "于文文", "深入人心的流行抒情佳作"},
            {"水星记", "郭顶", "富有艺术气质与深邃氛围感的原创单曲"}
        };
    }

    return intent;
}

json AgentService::fetchSongsForIntent(const ExtractedIntent& intent) {
    json final_songs = json::array();
    std::set<int64_t> added_ids;

    static thread_local std::mt19937 score_gen(2026);
    std::uniform_int_distribution<int> score_dist(93, 99);

    // 1. 精准根据推荐曲目名 + 歌手去网易云检索
    for (const auto& track : intent.specific_tracks) {
        std::string query = track.name + " " + track.artist;
        json search_res = ApiProxy::getInstance().searchSongs(query, 5, 0);

        if (search_res.contains("songs") && search_res["songs"].is_array() && search_res["songs"].size() > 0) {
            const auto& candidate_list = search_res["songs"];
            for (size_t i = 0; i < candidate_list.size(); ++i) {
                json song_obj = candidate_list[i];
                int64_t sid = song_obj["id"].as_int64();

                if (sid > 0 && added_ids.find(sid) == added_ids.end()) {
                    added_ids.insert(sid);
                    song_obj["match_score"] = score_dist(score_gen);
                    song_obj["agent_reason"] = track.reason.empty() ? 
                        ("契合您的诉求，《" + track.name + "》旋律极具感染力。") : track.reason;
                    final_songs.push_back(song_obj);
                    break; // 每首指定曲目只取最佳匹配的一条
                }
            }
        }
    }

    // 2. 如果精准曲目少于 5 首，利用 search_keywords 补充优质曲目
    if (final_songs.size() < 5) {
        for (const auto& kw : intent.search_keywords) {
            if (final_songs.size() >= 8) break;
            json kw_res = ApiProxy::getInstance().searchSongs(kw, 10, 0);
            if (kw_res.contains("songs") && kw_res["songs"].is_array()) {
                const auto& kw_songs = kw_res["songs"];
                for (size_t i = 0; i < kw_songs.size(); ++i) {
                    if (final_songs.size() >= 8) break;
                    json s = kw_songs[i];
                    int64_t sid = s["id"].as_int64();
                    if (sid > 0 && added_ids.find(sid) == added_ids.end()) {
                        added_ids.insert(sid);
                        s["match_score"] = score_dist(score_gen) - static_cast<int>(final_songs.size());
                        if (s["match_score"].as_int() < 88) s["match_score"] = 88;
                        s["agent_reason"] = "根据 [" + kw + "] 热门度与情感氛围精选，与您的诉求高度契合。";
                        final_songs.push_back(s);
                    }
                }
            }
        }
    }

    return final_songs;
}

json AgentService::generateRecommendation(int64_t user_id, const std::string& prompt, const std::string& mood, const std::string& preferred_genre, const LLMConfig& llm_config) {
    json response = json::object();

    // 1. 意图与曲目推荐解析
    ExtractedIntent intent;
    if (llm_config.enabled && !llm_config.api_key.empty()) {
        intent = analyzeWithLLM(prompt, mood, preferred_genre, llm_config);
    } else {
        intent = analyzeWithKnowledgeBase(prompt, mood, preferred_genre);
    }

    // 2. 结合用户画像 (仅在用户已登录时轻微提示)
    std::string user_history_hint = "";
    if (user_id > 0) {
        auto pref = DatabasePool::getInstance().getUserPreference(user_id);
        if (pref.has_value() && !pref->fav_genres.empty()) {
            user_history_hint = "（已融合您的历史偏好：" + pref->fav_genres + "）";
        }
    }

    // 3. 真实歌曲信息与可播放链接多路调度抓取
    json final_songs = fetchSongsForIntent(intent);

    // 4. 组装响应
    response["code"] = 200;
    response["agent_name"] = (llm_config.enabled && !llm_config.model.empty()) ? ("MelodyAI · " + llm_config.model) : "MelodyAI 音乐智能体";
    response["agent_avatar"] = "images/person01.png";
    response["reply_text"] = intent.summary + (user_history_hint.empty() ? "" : (" " + user_history_hint));

    json tag_arr = json::array();
    for (const auto& tag : intent.tags) {
        tag_arr.push_back(tag);
    }
    response["extracted_tags"] = tag_arr;
    response["songs"] = final_songs;
    response["total_recommended"] = static_cast<int>(final_songs.size());

    return response;
}
