#pragma once

#include "json.hpp"
#include <string>
#include <vector>

class MusicService {
public:
    static MusicService& getInstance();

    // 1. 搜索建议
    json searchSuggest(const std::string& keywords);

    // 2. 搜索歌曲 (已过滤不可播放歌曲)
    json searchSongs(const std::string& keywords, int limit = 30, int offset = 0);

    // 3. 获取歌曲直链与音质
    json getSongUrl(const std::string& song_id, const std::string& level = "standard");

    // 4. 获取歌词 (LRU 内存 -> MySQL -> 外部 API 代理，并持久化)
    json getSongLyric(const std::string& song_id);

    // 5. 获取歌单分类
    json getPlaylistCatlist();

    // 6. 获取热门歌单
    json getHotPlaylists(int limit = 20, int offset = 0, const std::string& cat = "全部");

    // 7. 获取歌单详情 (已过滤不可播放歌曲)
    json getPlaylistDetail(const std::string& playlist_id);

    // 8. 收藏相关
    json toggleFavorite(int64_t user_id, int64_t song_id, const std::string& name, const std::string& artist, const std::string& album, const std::string& cover, int duration);
    json getFavorites(int64_t user_id);
    json checkFavorite(int64_t user_id, int64_t song_id);

    // 9. 用户自建歌单
    json getUserPlaylists(int64_t user_id);
    json createUserPlaylist(int64_t user_id, const std::string& name, const std::string& desc, const std::string& cover);
    json addSongToPlaylist(int64_t playlist_id, int64_t song_id, const std::string& name, const std::string& artist, const std::string& album, const std::string& cover, int duration);
    json getPlaylistTracks(int64_t playlist_id);

    // 10. 播放历史
    json recordHistory(int64_t user_id, int64_t song_id, const std::string& name, const std::string& artist, const std::string& cover, int duration);
    json getHistory(int64_t user_id, int limit = 50);

private:
    MusicService() = default;
    ~MusicService() = default;
    MusicService(const MusicService&) = delete;
    MusicService& operator=(const MusicService&) = delete;
};
