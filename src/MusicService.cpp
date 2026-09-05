#include "MusicService.hpp"
#include "ApiProxy.hpp"
#include "Database.hpp"
#include "Cache.hpp"
#include <iostream>

MusicService& MusicService::getInstance() {
    static MusicService instance;
    return instance;
}

json MusicService::searchSuggest(const std::string& keywords) {
    return ApiProxy::getInstance().searchSuggest(keywords);
}

json MusicService::searchSongs(const std::string& keywords, int limit, int offset) {
    return ApiProxy::getInstance().searchSongs(keywords, limit, offset);
}

json MusicService::getSongUrl(const std::string& song_id, const std::string& level) {
    return ApiProxy::getInstance().getSongUrl(song_id, level);
}

json MusicService::getSongLyric(const std::string& song_id) {
    if (song_id.empty()) {
        json resp = json::object();
        resp["code"] = 400;
        return resp;
    }

    // 1. 优先查 LRU 内存缓存
    auto cached_mem = CacheManager::getInstance().getCachedLyric(song_id);
    if (cached_mem.has_value()) {
        return json::parse(cached_mem.value());
    }

    // 2. 其次查 MySQL 数据库
    int64_t sid = 0;
    try {
        sid = std::stoll(song_id);
    } catch (...) {}

    if (sid > 0) {
        auto db_lyric = DatabasePool::getInstance().getLyricFromDb(sid);
        if (db_lyric.has_value() && !db_lyric.value().empty()) {
            // 回填到 LRU 内存
            CacheManager::getInstance().cacheLyric(song_id, db_lyric.value());
            return json::parse(db_lyric.value());
        }
    }

    // 3. 缓存未命中，调用外部网易云 API 代理
    json api_res = ApiProxy::getInstance().getSongLyric(song_id);
    std::string dump_str = api_res.dump();

    if (api_res.contains("lrc") && api_res["lrc"].contains("lyric")) {
        std::string lrc_text = api_res["lrc"]["lyric"].as_string();
        std::string tlrc_text = "";
        if (api_res.contains("tlyric") && api_res["tlyric"].contains("lyric")) {
            tlrc_text = api_res["tlyric"]["lyric"].as_string();
        }

        // 异步或同步写入 MySQL & LRU 内存
        if (sid > 0) {
            DatabasePool::getInstance().saveLyricToDb(sid, lrc_text, tlrc_text);
        }
        CacheManager::getInstance().cacheLyric(song_id, dump_str);
    }

    return api_res;
}

json MusicService::getPlaylistCatlist() {
    return ApiProxy::getInstance().getPlaylistCatlist();
}

json MusicService::getHotPlaylists(int limit, int offset, const std::string& cat) {
    return ApiProxy::getInstance().getHotPlaylists(limit, offset, cat);
}

json MusicService::getPlaylistDetail(const std::string& playlist_id) {
    return ApiProxy::getInstance().getPlaylistDetail(playlist_id);
}

json MusicService::toggleFavorite(int64_t user_id, int64_t song_id, const std::string& name, const std::string& artist, const std::string& album, const std::string& cover, int duration) {
    json res = json::object();
    bool is_fav = DatabasePool::getInstance().isFavorite(user_id, song_id);

    if (is_fav) {
        DatabasePool::getInstance().removeFavorite(user_id, song_id);
        res["code"] = 200;
        res["message"] = "已取消收藏";
        res["is_favorite"] = false;
    } else {
        DatabasePool::getInstance().addFavorite(user_id, song_id, name, artist, album, cover, duration);
        res["code"] = 200;
        res["message"] = "收藏成功";
        res["is_favorite"] = true;
    }

    return res;
}

json MusicService::getFavorites(int64_t user_id) {
    json res = json::object();
    auto list = DatabasePool::getInstance().getFavorites(user_id);

    json arr = json::array();
    for (const auto& item : list) {
        json obj = json::object();
        obj["id"] = item.song_id;
        obj["name"] = item.song_name;
        obj["artist"] = item.artist_name;
        obj["album"] = item.album_name;
        obj["picUrl"] = item.cover_url;
        obj["duration"] = item.duration;
        obj["created_at"] = item.created_at;
        arr.push_back(obj);
    }

    res["code"] = 200;
    res["favorites"] = arr;
    res["total"] = static_cast<int>(arr.size());
    return res;
}

json MusicService::checkFavorite(int64_t user_id, int64_t song_id) {
    json res = json::object();
    res["code"] = 200;
    res["is_favorite"] = DatabasePool::getInstance().isFavorite(user_id, song_id);
    return res;
}

json MusicService::getUserPlaylists(int64_t user_id) {
    json res = json::object();
    auto list = DatabasePool::getInstance().getUserPlaylists(user_id);

    json arr = json::array();
    for (const auto& item : list) {
        json obj = json::object();
        obj["id"] = item.id;
        obj["name"] = item.name;
        obj["coverImgUrl"] = item.cover_url;
        obj["description"] = item.description;
        obj["playCount"] = item.play_count;
        obj["trackCount"] = item.track_count;
        obj["is_public"] = item.is_public;
        obj["created_at"] = item.created_at;
        arr.push_back(obj);
    }

    res["code"] = 200;
    res["playlists"] = arr;
    return res;
}

json MusicService::createUserPlaylist(int64_t user_id, const std::string& name, const std::string& desc, const std::string& cover) {
    json res = json::object();
    if (name.empty()) {
        res["code"] = 400;
        res["message"] = "歌单名称不能为空";
        return res;
    }

    int64_t pid = DatabasePool::getInstance().createPlaylist(user_id, name, desc, cover);
    if (pid <= 0) {
        res["code"] = 500;
        res["message"] = "创建歌单失败";
        return res;
    }

    res["code"] = 200;
    res["message"] = "创建歌单成功";
    res["playlist_id"] = pid;
    return res;
}

json MusicService::addSongToPlaylist(int64_t playlist_id, int64_t song_id, const std::string& name, const std::string& artist, const std::string& album, const std::string& cover, int duration) {
    json res = json::object();
    bool ok = DatabasePool::getInstance().addSongToPlaylist(playlist_id, song_id, name, artist, album, cover, duration);
    res["code"] = ok ? 200 : 500;
    res["message"] = ok ? "添加歌曲成功" : "添加歌曲失败";
    return res;
}

json MusicService::getPlaylistTracks(int64_t playlist_id) {
    json res = json::object();
    auto tracks = DatabasePool::getInstance().getPlaylistTracks(playlist_id);

    json arr = json::array();
    for (const auto& item : tracks) {
        json obj = json::object();
        obj["id"] = item.song_id;
        obj["name"] = item.song_name;
        obj["artist"] = item.artist_name;
        obj["album"] = item.album_name;
        obj["picUrl"] = item.cover_url;
        obj["duration"] = item.duration;
        arr.push_back(obj);
    }

    res["code"] = 200;
    res["tracks"] = arr;
    return res;
}

json MusicService::recordHistory(int64_t user_id, int64_t song_id, const std::string& name, const std::string& artist, const std::string& cover, int duration) {
    json res = json::object();
    bool ok = DatabasePool::getInstance().addPlayHistory(user_id, song_id, name, artist, cover, duration);
    res["code"] = ok ? 200 : 500;
    return res;
}

json MusicService::getHistory(int64_t user_id, int limit) {
    json res = json::object();
    auto list = DatabasePool::getInstance().getPlayHistory(user_id, limit);

    json arr = json::array();
    for (const auto& item : list) {
        json obj = json::object();
        obj["id"] = item.song_id;
        obj["name"] = item.song_name;
        obj["artist"] = item.artist_name;
        obj["picUrl"] = item.cover_url;
        obj["duration"] = item.duration;
        obj["played_at"] = item.played_at;
        arr.push_back(obj);
    }

    res["code"] = 200;
    res["history"] = arr;
    return res;
}
