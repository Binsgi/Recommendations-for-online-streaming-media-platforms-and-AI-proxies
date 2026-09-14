#pragma once

#include "json.hpp"
#include <string>
#include <vector>
#include <optional>

class ApiProxy {
public:
    static ApiProxy& getInstance();

    // Raw HTTP Requests
    std::string httpGet(const std::string& url, long timeout_sec = 5);
    std::string httpPost(const std::string& url, const std::string& post_fields, long timeout_sec = 5);

    // Helpers
    static std::string urlEncode(const std::string& value);

    // Business Proxy Methods
    // 1. 搜索建议
    json searchSuggest(const std::string& keywords);

    // 2. 搜索歌曲 (自动过滤掉不可播放/url为空的歌曲)
    json searchSongs(const std::string& keywords, int limit = 30, int offset = 0);

    // 3. 获取歌曲播放直链与音质
    json getSongUrl(const std::string& song_id, const std::string& level = "standard");

    // 4. 批量获取歌曲可播放直链
    json batchGetSongUrls(const std::vector<std::string>& song_ids, const std::string& level = "standard");

    // 5. 获取歌词
    json getSongLyric(const std::string& song_id);

    // 6. 获取歌单分类
    json getPlaylistCatlist();

    // 7. 获取热门/分类歌单
    json getHotPlaylists(int limit = 20, int offset = 0, const std::string& cat = "全部");

    // 8. 获取歌单详情 (曲目自动过滤无效音频)
    json getPlaylistDetail(const std::string& playlist_id);

    // 9. 智能过滤不可播放歌曲 (url 为空或无版权)
    json filterPlayableSongs(const json& raw_songs, const std::string& level = "standard");

private:
    ApiProxy();
    ~ApiProxy();
    ApiProxy(const ApiProxy&) = delete;
    ApiProxy& operator=(const ApiProxy&) = delete;

    std::string base_api_url_;
    int timeout_sec_;
};
