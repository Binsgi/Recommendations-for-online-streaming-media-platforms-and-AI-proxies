#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>

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

    // Business Methods
    // Users
    std::optional<UserRecord> findUserByUsername(const std::string& username);
    std::optional<UserRecord> findUserById(int64_t user_id);
    std::optional<UserRecord> findUserByPhoneEmail(const std::string& phone_email);
    int64_t createUser(const std::string& username, const std::string& phone_email, const std::string& password_hash, const std::string& nickname);
    bool updatePasswordByTarget(const std::string& target, const std::string& new_password_hash);
    bool updatePassword(int64_t user_id, const std::string& new_password_hash);

    // Favorites
    bool addFavorite(int64_t user_id, int64_t song_id, const std::string& song_name, const std::string& artist_name, const std::string& album_name, const std::string& cover_url, int duration);
    bool removeFavorite(int64_t user_id, int64_t song_id);
    bool isFavorite(int64_t user_id, int64_t song_id);
    std::vector<FavoriteRecord> getFavorites(int64_t user_id);

    // User Playlists
    std::vector<PlaylistRecord> getUserPlaylists(int64_t user_id);
    int64_t createPlaylist(int64_t user_id, const std::string& name, const std::string& desc, const std::string& cover);
    bool addSongToPlaylist(int64_t playlist_id, int64_t song_id, const std::string& song_name, const std::string& artist, const std::string& album, const std::string& cover, int duration);
    std::vector<PlaylistTrackRecord> getPlaylistTracks(int64_t playlist_id);

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
    static std::string escapeString(MYSQL* conn, const std::string& str);

private:
    DatabasePool() = default;
    ~DatabasePool();
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    MYSQL* createRawConnection();

    std::queue<MYSQL*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool is_initialized_ = false;
};
