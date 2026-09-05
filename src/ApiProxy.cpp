#include "ApiProxy.hpp"
#include "Config.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>

static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

ApiProxy::ApiProxy() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    const auto& conf = Config::getInstance();
    base_api_url_ = conf.netease_api;
    timeout_sec_ = conf.api_timeout_sec;
}

ApiProxy::~ApiProxy() {
    curl_global_cleanup();
}

ApiProxy& ApiProxy::getInstance() {
    static ApiProxy instance;
    return instance;
}

std::string ApiProxy::urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }

    return escaped.str();
}

std::string ApiProxy::httpGet(const std::string& url, long timeout_sec) {
    CURL* curl = curl_easy_init();
    std::string read_buffer;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) DemoPlayer/2.0");

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "[ApiProxy] httpGet failed for " << url << ": " << curl_easy_strerror(res) << "\n";
        }
        curl_easy_cleanup(curl);
    }

    return read_buffer;
}

std::string ApiProxy::httpPost(const std::string& url, const std::string& post_fields, long timeout_sec) {
    CURL* curl = curl_easy_init();
    std::string read_buffer;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) DemoPlayer/2.0");

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json;charset=utf-8");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "[ApiProxy] httpPost failed for " << url << ": " << curl_easy_strerror(res) << "\n";
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return read_buffer;
}

// ==================== Business Implementations ====================

json ApiProxy::searchSuggest(const std::string& keywords) {
    std::string url = base_api_url_ + "/search/suggest?keywords=" + urlEncode(keywords) + "&type=mobile";
    std::string raw = httpGet(url, timeout_sec_);
    if (raw.empty()) {
        json resp = json::object();
        resp["code"] = 500;
        resp["result"] = json::object();
        return resp;
    }
    return json::parse(raw);
}

json ApiProxy::batchGetSongUrls(const std::vector<std::string>& song_ids, const std::string& level) {
    if (song_ids.empty()) {
        json resp = json::object();
        resp["data"] = json::array();
        return resp;
    }

    std::string ids_str;
    for (size_t i = 0; i < song_ids.size(); ++i) {
        ids_str += song_ids[i];
        if (i + 1 < song_ids.size()) ids_str += ",";
    }

    std::string url = base_api_url_ + "/song/url/v1?id=" + ids_str + "&level=" + level;
    std::string raw = httpGet(url, timeout_sec_);
    if (raw.empty()) {
        // Fallback to legacy /song/url
        std::string fallback_url = base_api_url_ + "/song/url?id=" + ids_str;
        raw = httpGet(fallback_url, timeout_sec_);
    }

    return json::parse(raw);
}

json ApiProxy::filterPlayableSongs(const json& raw_songs, const std::string& level) {
    if (!raw_songs.is_array() || raw_songs.size() == 0) {
        return json::array();
    }

    std::vector<std::string> song_ids;
    for (size_t i = 0; i < raw_songs.size(); ++i) {
        int64_t id = raw_songs[i]["id"].as_int64();
        if (id > 0) {
            song_ids.push_back(std::to_string(id));
        }
    }

    json urls_resp = batchGetSongUrls(song_ids, level);
    std::map<int64_t, std::string> playable_url_map;

    if (urls_resp.contains("data") && urls_resp["data"].is_array()) {
        const auto& data_arr = urls_resp["data"];
        for (size_t i = 0; i < data_arr.size(); ++i) {
            const auto& item = data_arr[i];
            int64_t sid = item["id"].as_int64();
            std::string url = item["url"].as_string();
            // Valid URL check
            if (!url.empty() && url != "null" && url.rfind("http", 0) == 0) {
                playable_url_map[sid] = url;
            }
        }
    }

    json filtered = json::array();
    for (size_t i = 0; i < raw_songs.size(); ++i) {
        json song = raw_songs[i];
        int64_t sid = song["id"].as_int64();

        auto it = playable_url_map.find(sid);
        // 核心要求 4：把不能播放的音乐（url为空的歌曲）从剔除后再展示到前端
        if (it != playable_url_map.end() && !it->second.empty()) {
            song["play_url"] = it->second;
            filtered.push_back(song);
        }
    }

    return filtered;
}

json ApiProxy::searchSongs(const std::string& keywords, int limit, int offset) {
    std::string url = base_api_url_ + "/cloudsearch?keywords=" + urlEncode(keywords) +
                      "&type=1&limit=" + std::to_string(limit) +
                      "&offset=" + std::to_string(offset);

    std::string raw = httpGet(url, timeout_sec_);
    if (raw.empty()) {
        std::string fallback = base_api_url_ + "/search?keywords=" + urlEncode(keywords) +
                               "&limit=" + std::to_string(limit) +
                               "&offset=" + std::to_string(offset);
        raw = httpGet(fallback, timeout_sec_);
    }

    json parsed = json::parse(raw);
    json result = json::object();
    result["code"] = 200;

    json raw_song_list = json::array();
    if (parsed.contains("result") && parsed["result"].contains("songs") && parsed["result"]["songs"].is_array()) {
        raw_song_list = parsed["result"]["songs"];
    }

    // 剔除不可播放的歌曲
    result["songs"] = filterPlayableSongs(raw_song_list);
    result["total"] = static_cast<int>(result["songs"].size());

    return result;
}

json ApiProxy::getSongUrl(const std::string& song_id, const std::string& level) {
    std::vector<std::string> ids = {song_id};
    json batch = batchGetSongUrls(ids, level);
    if (batch.contains("data") && batch["data"].is_array() && batch["data"].size() > 0) {
        return batch["data"][0];
    }
    return json::object();
}

json ApiProxy::getSongLyric(const std::string& song_id) {
    std::string url = base_api_url_ + "/lyric?id=" + song_id;
    std::string raw = httpGet(url, timeout_sec_);
    if (raw.empty()) {
        std::string fallback = base_api_url_ + "/lyric/new?id=" + song_id;
        raw = httpGet(fallback, timeout_sec_);
    }
    return json::parse(raw);
}

json ApiProxy::getPlaylistCatlist() {
    std::string url = base_api_url_ + "/playlist/catlist";
    std::string raw = httpGet(url, timeout_sec_);
    return json::parse(raw);
}

json ApiProxy::getHotPlaylists(int limit, int offset, const std::string& cat) {
    std::string url = base_api_url_ + "/top/playlist?limit=" + std::to_string(limit) +
                      "&offset=" + std::to_string(offset) +
                      "&cat=" + urlEncode(cat);
    std::string raw = httpGet(url, timeout_sec_);
    return json::parse(raw);
}

json ApiProxy::getPlaylistDetail(const std::string& playlist_id) {
    std::string url = base_api_url_ + "/playlist/detail?id=" + playlist_id;
    std::string raw = httpGet(url, timeout_sec_);
    json parsed = json::parse(raw);

    if (parsed.contains("playlist") && parsed["playlist"].contains("tracks") && parsed["playlist"]["tracks"].is_array()) {
        json tracks = parsed["playlist"]["tracks"];
        // 过滤不可播放歌曲
        parsed["playlist"]["tracks"] = filterPlayableSongs(tracks);
    }

    return parsed;
}
