#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>

<<<<<<< HEAD
struct UserRecord {
    int64_t id = 0;
    std::string username;
    std::string phone_email;
    std::string password_hash;
    std::string nickname;
    std::string avatar_url;
    std::string created_at;
};

struct FavoriteRecord {
    int64_t id = 0;
    int64_t user_id = 0;
    int64_t song_id = 0;
    std::string song_name;
    std::string artist_name;
    std::string album_name;
    std::string cover_url;
    int duration = 0;
    std::string created_at;
};

struct PlaylistRecord {
    int64_t id = 0;
    int64_t user_id = 0;
    std::string name;
    std::string cover_url;
    std::string description;
    int play_count = 0;
    int track_count = 0;
    int is_public = 1;
    std::string created_at;
};

struct PlaylistTrackRecord {
    int64_t id = 0;
    int64_t playlist_id = 0;
    int64_t song_id = 0;
    std::string song_name;
    std::string artist_name;
    std::string album_name;
    std::string cover_url;
    int duration = 0;
    std::string added_at;
};

struct HistoryRecord {
    int64_t id = 0;
    int64_t user_id = 0;
    int64_t song_id = 0;
    std::string song_name;
    std::string artist_name;
    std::string cover_url;
    int duration = 0;
    std::string played_at;
};

struct UserPreferenceRecord {
    int64_t user_id = 0;
    std::string fav_genres;
    std::string fav_artists;
    std::string mood_tags;
=======
// 用户账号信息
struct UserRecord {
    /** 用户主键ID */
    int64_t id = 0;
    /** 用户名（唯一登录账号） */
    std::string username;
    /** 绑定的手机号或电子邮箱 */
    std::string phone_email;
    /** 加盐SHA256哈希后的密码密文 */
    std::string password_hash;
    /** 用户昵称 */
    std::string nickname;
    /** 头像图片URL或静态资源路径 */
    std::string avatar_url;
    /** 账号注册创建时间 */
    std::string created_at;
};
// 用户收藏歌曲
struct FavoriteRecord {
    /** 收藏记录主键ID */
    int64_t id = 0;
    /** 收藏者的用户ID */
    int64_t user_id = 0;
    /** 歌曲唯一ID（对应网易云歌曲ID） */
    int64_t song_id = 0;
    /** 歌曲名称 */
    std::string song_name;
    /** 歌手名称 */
    std::string artist_name;
    /** 所属专辑名称 */
    std::string album_name;
    /** 歌曲封面图片URL */
    std::string cover_url;
    /** 歌曲时长（单位：秒） */
    int duration = 0;
    /** 收藏时间 */
    std::string created_at;
};
// 用户自建歌单
struct PlaylistRecord {
    /** 歌单主键ID */
    int64_t id = 0;
    /** 创建者的用户ID */
    int64_t user_id = 0;
    /** 歌单标题名称 */
    std::string name;
    /** 歌单封面图片URL */
    std::string cover_url;
    /** 歌单描述简介 */
    std::string description;
    /** 歌单累计播放次数 */
    int play_count = 0;
    /** 歌单内包含的歌曲总数量 */
    int track_count = 0;
    /** 是否公开（1: 公开, 0: 私密） */
    int is_public = 1;
    /** 歌单创建时间 */
    std::string created_at;
};
// 歌单歌曲明细关联
struct PlaylistTrackRecord {
    /** 关联记录主键ID */
    int64_t id = 0;
    /** 所属歌单ID */
    int64_t playlist_id = 0;
    /** 歌曲唯一ID（对应网易云歌曲ID） */
    int64_t song_id = 0;
    /** 歌曲名称 */
    std::string song_name;
    /** 歌手名称 */
    std::string artist_name;
    /** 所属专辑名称 */
    std::string album_name;
    /** 歌曲封面图片URL */
    std::string cover_url;
    /** 歌曲时长（单位：秒） */
    int duration = 0;
    /** 添加到该歌单的时间 */
    std::string added_at;
};
// 用户播放历史
struct HistoryRecord {
    /** 播放历史记录主键ID */
    int64_t id = 0;
    /** 播放者的用户ID */
    int64_t user_id = 0;
    /** 歌曲唯一ID（对应网易云歌曲ID） */
    int64_t song_id = 0;
    /** 歌曲名称 */
    std::string song_name;
    /** 歌手名称 */
    std::string artist_name;
    /** 歌曲封面图片URL */
    std::string cover_url;
    /** 歌曲时长（单位：秒） */
    int duration = 0;
    /** 最近一次播放的时间 */
    std::string played_at;
};
// 用户画像偏好（用于 AI Agent 智能推荐）
struct UserPreferenceRecord {
    /** 关联的用户ID（主键） */
    int64_t user_id = 0;
    /** 偏好的音乐流派（逗号分隔，如：流行,民谣,轻音乐） */
    std::string fav_genres;
    /** 常听或偏好的歌手名称（逗号分隔） */
    std::string fav_artists;
    /** 心情/场景标签（如：放松,专注,运动） */
    std::string mood_tags;
    /** 偏好画像最后更新时间 */
>>>>>>> origin/master
    std::string updated_at;
};

class DatabasePool;

class ScopedConnection {
public:
    ScopedConnection(DatabasePool& pool, MYSQL* conn);
    ~ScopedConnection();
    MYSQL* get() const { return conn_; }
    MYSQL* operator->() const { return conn_; }
    bool isValid() const { return conn_ != nullptr; }

private:
    DatabasePool& pool_;
    MYSQL* conn_;
};

class DatabasePool {
public:
    static DatabasePool& getInstance();

    bool init();
    void close();

    std::unique_ptr<ScopedConnection> getConnection();
    void releaseConnection(MYSQL* conn);

<<<<<<< HEAD
    // Business Methods
    // Users
=======
    // 用户
>>>>>>> origin/master
    std::optional<UserRecord> findUserByUsername(const std::string& username);
    std::optional<UserRecord> findUserById(int64_t user_id);
    std::optional<UserRecord> findUserByPhoneEmail(const std::string& phone_email);
    int64_t createUser(const std::string& username, const std::string& phone_email, const std::string& password_hash, const std::string& nickname);
    bool updatePasswordByTarget(const std::string& target, const std::string& new_password_hash);
    bool updatePassword(int64_t user_id, const std::string& new_password_hash);

<<<<<<< HEAD
    // Favorites
=======
    // 收藏
>>>>>>> origin/master
    bool addFavorite(int64_t user_id, int64_t song_id, const std::string& song_name, const std::string& artist_name, const std::string& album_name, const std::string& cover_url, int duration);
    bool removeFavorite(int64_t user_id, int64_t song_id);
    bool isFavorite(int64_t user_id, int64_t song_id);
    std::vector<FavoriteRecord> getFavorites(int64_t user_id);

<<<<<<< HEAD
    // User Playlists
=======
    // 播放列表
>>>>>>> origin/master
    std::vector<PlaylistRecord> getUserPlaylists(int64_t user_id);
    int64_t createPlaylist(int64_t user_id, const std::string& name, const std::string& desc, const std::string& cover);
    bool addSongToPlaylist(int64_t playlist_id, int64_t song_id, const std::string& song_name, const std::string& artist, const std::string& album, const std::string& cover, int duration);
    std::vector<PlaylistTrackRecord> getPlaylistTracks(int64_t playlist_id);

<<<<<<< HEAD
    // Lyrics
    std::optional<std::string> getLyricFromDb(int64_t song_id);
    bool saveLyricToDb(int64_t song_id, const std::string& lyric_text, const std::string& tlyric_text = "");

    // History
    bool addPlayHistory(int64_t user_id, int64_t song_id, const std::string& song_name, const std::string& artist_name, const std::string& cover_url, int duration);
    std::vector<HistoryRecord> getPlayHistory(int64_t user_id, int limit = 50);

    // Preferences (For AI Agent)
    std::optional<UserPreferenceRecord> getUserPreference(int64_t user_id);
    bool updateUserPreference(int64_t user_id, const std::string& genres, const std::string& artists, const std::string& mood);

    // Escape helper
=======
    // 歌词
    std::optional<std::string> getLyricFromDb(int64_t song_id);
    bool saveLyricToDb(int64_t song_id, const std::string& lyric_text, const std::string& tlyric_text = "");

    // 播放历史
    bool addPlayHistory(int64_t user_id, int64_t song_id, const std::string& song_name, const std::string& artist_name, const std::string& cover_url, int duration);
    std::vector<HistoryRecord> getPlayHistory(int64_t user_id, int limit = 50);

    // Agent偏好 (For AI Agent)
    std::optional<UserPreferenceRecord> getUserPreference(int64_t user_id);
    bool updateUserPreference(int64_t user_id, const std::string& genres, const std::string& artists, const std::string& mood);

    // 防止 SQL 注入
>>>>>>> origin/master
    static std::string escapeString(MYSQL* conn, const std::string& str);

private:
    DatabasePool() = default;
    ~DatabasePool();
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    MYSQL* createRawConnection();

<<<<<<< HEAD
    std::queue<MYSQL*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
=======
    /* 数据库连接池 */
    std::queue<MYSQL*> pool_;
    /* 互斥锁 */
    std::mutex mutex_;
    std::condition_variable cv_;
    /* 初始化标志位 */
>>>>>>> origin/master
    bool is_initialized_ = false;
};
