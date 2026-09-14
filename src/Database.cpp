#include "Database.hpp"
#include "Config.hpp"
#include <iostream>
#include <sstream>

ScopedConnection::ScopedConnection(DatabasePool& pool, MYSQL* conn)
    : pool_(pool), conn_(conn) {}

ScopedConnection::~ScopedConnection() {
    if (conn_) {
        pool_.releaseConnection(conn_);
    }
}

DatabasePool& DatabasePool::getInstance() {
    static DatabasePool instance;
    return instance;
}

DatabasePool::~DatabasePool() {
    close();
}

MYSQL* DatabasePool::createRawConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "[Database] mysql_init failed!\n";
        return nullptr;
    }

    const auto& conf = Config::getInstance();
    unsigned int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(conn,
                            conf.mysql_host.c_str(),
                            conf.mysql_user.c_str(),
                            conf.mysql_password.c_str(),
                            conf.mysql_database.c_str(),
                            conf.mysql_port,
                            nullptr,
                            0)) {
        std::cerr << "[Database] mysql_real_connect failed: " << mysql_error(conn) << "\n";
        mysql_close(conn);
        return nullptr;
    }

    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}

bool DatabasePool::init() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (is_initialized_) return true;

    const auto& conf = Config::getInstance();
    std::cout << "[Database] Initializing MySQL connection pool (Size: " << conf.mysql_pool_size << ")...\n";

    for (int i = 0; i < conf.mysql_pool_size; ++i) {
        MYSQL* conn = createRawConnection();
        if (conn) {
            pool_.push(conn);
        } else {
            std::cerr << "[Database] Warning: Failed to create connection " << i + 1 << "\n";
        }
    }

    if (pool_.empty()) {
        std::cerr << "[Database] Error: Could not connect to MySQL database! Please check config.ini and MySQL service.\n";
        return false;
    }

    is_initialized_ = true;
    std::cout << "[Database] Connection pool initialized successfully. Active connections: " << pool_.size() << "\n";
    return true;
}

void DatabasePool::close() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        MYSQL* conn = pool_.front();
        pool_.pop();
        if (conn) {
            mysql_close(conn);
        }
    }
    is_initialized_ = false;
}

std::unique_ptr<ScopedConnection> DatabasePool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() {
        return !pool_.empty() || !is_initialized_;
    });

    if (!is_initialized_ || pool_.empty()) {
        return nullptr;
    }

    MYSQL* conn = pool_.front();
    pool_.pop();

    // Health check (ping)
    if (mysql_ping(conn) != 0) {
        std::cerr << "[Database] Connection lost, reconnecting...\n";
        mysql_close(conn);
        conn = createRawConnection();
    }

    return std::make_unique<ScopedConnection>(*this, conn);
}

void DatabasePool::releaseConnection(MYSQL* conn) {
    if (!conn) return;
    std::unique_lock<std::mutex> lock(mutex_);
    pool_.push(conn);
    cv_.notify_one();
}

std::string DatabasePool::escapeString(MYSQL* conn, const std::string& str) {
    if (!conn || str.empty()) return str;
    std::vector<char> buffer(str.length() * 2 + 1);
    unsigned long length = mysql_real_escape_string(conn, buffer.data(), str.c_str(), str.length());
    return std::string(buffer.data(), length);
}

// ==================== User Operations ====================

std::optional<UserRecord> DatabasePool::findUserByUsername(const std::string& username) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return std::nullopt;

    std::string safe_user = escapeString(sc->get(), username);
    std::string sql = "SELECT id, username, phone_email, password_hash, nickname, avatar_url, created_at FROM users WHERE username = '" + safe_user + "' LIMIT 1;";

    if (mysql_query(sc->get(), sql.c_str()) != 0) {
        std::cerr << "[Database] findUserByUsername error: " << mysql_error(sc->get()) << "\n";
        return std::nullopt;
    }

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return std::nullopt;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return std::nullopt;
    }

    UserRecord u;
    u.id = row[0] ? std::stoll(row[0]) : 0;
    u.username = row[1] ? row[1] : "";
    u.phone_email = row[2] ? row[2] : "";
    u.password_hash = row[3] ? row[3] : "";
    u.nickname = row[4] ? row[4] : "";
    u.avatar_url = row[5] ? row[5] : "";
    u.created_at = row[6] ? row[6] : "";

    mysql_free_result(res);
    return u;
}

std::optional<UserRecord> DatabasePool::findUserById(int64_t user_id) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return std::nullopt;

    std::string sql = "SELECT id, username, phone_email, password_hash, nickname, avatar_url, created_at FROM users WHERE id = " + std::to_string(user_id) + " LIMIT 1;";

    if (mysql_query(sc->get(), sql.c_str()) != 0) {
        return std::nullopt;
    }

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return std::nullopt;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return std::nullopt;
    }

    UserRecord u;
    u.id = row[0] ? std::stoll(row[0]) : 0;
    u.username = row[1] ? row[1] : "";
    u.phone_email = row[2] ? row[2] : "";
    u.password_hash = row[3] ? row[3] : "";
    u.nickname = row[4] ? row[4] : "";
    u.avatar_url = row[5] ? row[5] : "";
    u.created_at = row[6] ? row[6] : "";

    mysql_free_result(res);
    return u;
}

std::optional<UserRecord> DatabasePool::findUserByPhoneEmail(const std::string& phone_email) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return std::nullopt;

    std::string safe_target = escapeString(sc->get(), phone_email);
    std::string sql = "SELECT id, username, phone_email, password_hash, nickname, avatar_url, created_at FROM users WHERE phone_email = '" + safe_target + "' OR username = '" + safe_target + "' LIMIT 1;";

    if (mysql_query(sc->get(), sql.c_str()) != 0) {
        return std::nullopt;
    }

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return std::nullopt;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return std::nullopt;
    }

    UserRecord u;
    u.id = row[0] ? std::stoll(row[0]) : 0;
    u.username = row[1] ? row[1] : "";
    u.phone_email = row[2] ? row[2] : "";
    u.password_hash = row[3] ? row[3] : "";
    u.nickname = row[4] ? row[4] : "";
    u.avatar_url = row[5] ? row[5] : "";
    u.created_at = row[6] ? row[6] : "";

    mysql_free_result(res);
    return u;
}

int64_t DatabasePool::createUser(const std::string& username, const std::string& phone_email, const std::string& password_hash, const std::string& nickname) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return -1;

    std::string safe_user = escapeString(sc->get(), username);
    std::string safe_target = escapeString(sc->get(), phone_email);
    std::string safe_pass = escapeString(sc->get(), password_hash);
    std::string safe_nick = escapeString(sc->get(), nickname.empty() ? username : nickname);

    std::ostringstream ss;
    ss << "INSERT INTO users (username, phone_email, password_hash, nickname) VALUES ('"
       << safe_user << "', '" << safe_target << "', '" << safe_pass << "', '" << safe_nick << "');";

    if (mysql_query(sc->get(), ss.str().c_str()) != 0) {
        std::cerr << "[Database] createUser error: " << mysql_error(sc->get()) << "\n";
        return -1;
    }

    return static_cast<int64_t>(mysql_insert_id(sc->get()));
}

bool DatabasePool::updatePasswordByTarget(const std::string& target, const std::string& new_password_hash) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_target = escapeString(sc->get(), target);
    std::string safe_pass = escapeString(sc->get(), new_password_hash);

    std::string sql = "UPDATE users SET password_hash = '" + safe_pass + "' WHERE phone_email = '" + safe_target + "' OR username = '" + safe_target + "';";
    return mysql_query(sc->get(), sql.c_str()) == 0;
}

bool DatabasePool::updatePassword(int64_t user_id, const std::string& new_password_hash) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_pass = escapeString(sc->get(), new_password_hash);
    std::string sql = "UPDATE users SET password_hash = '" + safe_pass + "' WHERE id = " + std::to_string(user_id) + ";";
    return mysql_query(sc->get(), sql.c_str()) == 0;
}

// ==================== Favorites Operations ====================

bool DatabasePool::addFavorite(int64_t user_id, int64_t song_id, const std::string& song_name, const std::string& artist_name, const std::string& album_name, const std::string& cover_url, int duration) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_name = escapeString(sc->get(), song_name);
    std::string safe_artist = escapeString(sc->get(), artist_name);
    std::string safe_album = escapeString(sc->get(), album_name);
    std::string safe_cover = escapeString(sc->get(), cover_url);

    std::ostringstream ss;
    ss << "INSERT INTO favorites (user_id, song_id, song_name, artist_name, album_name, cover_url, duration) VALUES ("
       << user_id << ", " << song_id << ", '" << safe_name << "', '" << safe_artist << "', '" << safe_album << "', '" << safe_cover << "', " << duration
       << ") ON DUPLICATE KEY UPDATE song_name=VALUES(song_name), artist_name=VALUES(artist_name), cover_url=VALUES(cover_url);";

    return mysql_query(sc->get(), ss.str().c_str()) == 0;
}

bool DatabasePool::removeFavorite(int64_t user_id, int64_t song_id) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string sql = "DELETE FROM favorites WHERE user_id = " + std::to_string(user_id) + " AND song_id = " + std::to_string(song_id) + ";";
    return mysql_query(sc->get(), sql.c_str()) == 0;
}

bool DatabasePool::isFavorite(int64_t user_id, int64_t song_id) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string sql = "SELECT id FROM favorites WHERE user_id = " + std::to_string(user_id) + " AND song_id = " + std::to_string(song_id) + " LIMIT 1;";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return false;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return false;

    bool found = (mysql_num_rows(res) > 0);
    mysql_free_result(res);
    return found;
}

std::vector<FavoriteRecord> DatabasePool::getFavorites(int64_t user_id) {
    std::vector<FavoriteRecord> list;
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return list;

    std::string sql = "SELECT id, user_id, song_id, song_name, artist_name, album_name, cover_url, duration, created_at FROM favorites WHERE user_id = " + std::to_string(user_id) + " ORDER BY id DESC;";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return list;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return list;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        FavoriteRecord r;
        r.id = row[0] ? std::stoll(row[0]) : 0;
        r.user_id = row[1] ? std::stoll(row[1]) : 0;
        r.song_id = row[2] ? std::stoll(row[2]) : 0;
        r.song_name = row[3] ? row[3] : "";
        r.artist_name = row[4] ? row[4] : "";
        r.album_name = row[5] ? row[5] : "";
        r.cover_url = row[6] ? row[6] : "";
        r.duration = row[7] ? std::stoi(row[7]) : 0;
        r.created_at = row[8] ? row[8] : "";
        list.push_back(r);
    }

    mysql_free_result(res);
    return list;
}

// ==================== User Playlists ====================

std::vector<PlaylistRecord> DatabasePool::getUserPlaylists(int64_t user_id) {
    std::vector<PlaylistRecord> list;
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return list;

    std::string sql = "SELECT id, user_id, name, cover_url, description, play_count, is_public, created_at FROM user_playlists WHERE user_id = " + std::to_string(user_id) + " ORDER BY id DESC;";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return list;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return list;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        PlaylistRecord r;
        r.id = row[0] ? std::stoll(row[0]) : 0;
        r.user_id = row[1] ? std::stoll(row[1]) : 0;
        r.name = row[2] ? row[2] : "";
        r.cover_url = row[3] ? row[3] : "";
        r.description = row[4] ? row[4] : "";
        r.play_count = row[5] ? std::stoi(row[5]) : 0;
        r.is_public = row[6] ? std::stoi(row[6]) : 1;
        r.created_at = row[7] ? row[7] : "";
        list.push_back(r);
    }

    mysql_free_result(res);
    return list;
}

int64_t DatabasePool::createPlaylist(int64_t user_id, const std::string& name, const std::string& desc, const std::string& cover) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return -1;

    std::string safe_name = escapeString(sc->get(), name);
    std::string safe_desc = escapeString(sc->get(), desc);
    std::string safe_cover = escapeString(sc->get(), cover.empty() ? "images/cover.png" : cover);

    std::ostringstream ss;
    ss << "INSERT INTO user_playlists (user_id, name, description, cover_url) VALUES ("
       << user_id << ", '" << safe_name << "', '" << safe_desc << "', '" << safe_cover << "');";

    if (mysql_query(sc->get(), ss.str().c_str()) != 0) return -1;
    return static_cast<int64_t>(mysql_insert_id(sc->get()));
}

bool DatabasePool::addSongToPlaylist(int64_t playlist_id, int64_t song_id, const std::string& song_name, const std::string& artist, const std::string& album, const std::string& cover, int duration) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_name = escapeString(sc->get(), song_name);
    std::string safe_artist = escapeString(sc->get(), artist);
    std::string safe_album = escapeString(sc->get(), album);
    std::string safe_cover = escapeString(sc->get(), cover);

    std::ostringstream ss;
    ss << "INSERT INTO playlist_tracks (playlist_id, song_id, song_name, artist_name, album_name, cover_url, duration) VALUES ("
       << playlist_id << ", " << song_id << ", '" << safe_name << "', '" << safe_artist << "', '" << safe_album << "', '" << safe_cover << "', " << duration
       << ") ON DUPLICATE KEY UPDATE song_name=VALUES(song_name);";

    return mysql_query(sc->get(), ss.str().c_str()) == 0;
}

std::vector<PlaylistTrackRecord> DatabasePool::getPlaylistTracks(int64_t playlist_id) {
    std::vector<PlaylistTrackRecord> list;
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return list;

    std::string sql = "SELECT id, playlist_id, song_id, song_name, artist_name, album_name, cover_url, duration, added_at FROM playlist_tracks WHERE playlist_id = " + std::to_string(playlist_id) + " ORDER BY id ASC;";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return list;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return list;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        PlaylistTrackRecord r;
        r.id = row[0] ? std::stoll(row[0]) : 0;
        r.playlist_id = row[1] ? std::stoll(row[1]) : 0;
        r.song_id = row[2] ? std::stoll(row[2]) : 0;
        r.song_name = row[3] ? row[3] : "";
        r.artist_name = row[4] ? row[4] : "";
        r.album_name = row[5] ? row[5] : "";
        r.cover_url = row[6] ? row[6] : "";
        r.duration = row[7] ? std::stoi(row[7]) : 0;
        r.added_at = row[8] ? row[8] : "";
        list.push_back(r);
    }

    mysql_free_result(res);
    return list;
}

// ==================== Lyrics Cache ====================

std::optional<std::string> DatabasePool::getLyricFromDb(int64_t song_id) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return std::nullopt;

    std::string sql = "SELECT lyric_text FROM lyrics_cache WHERE song_id = " + std::to_string(song_id) + " LIMIT 1;";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return std::nullopt;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return std::nullopt;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return std::nullopt;
    }

    std::string text = row[0] ? row[0] : "";
    mysql_free_result(res);
    return text;
}

bool DatabasePool::saveLyricToDb(int64_t song_id, const std::string& lyric_text, const std::string& tlyric_text) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_lrc = escapeString(sc->get(), lyric_text);
    std::string safe_tlrc = escapeString(sc->get(), tlyric_text);

    std::ostringstream ss;
    ss << "REPLACE INTO lyrics_cache (song_id, lyric_text, tlyric_text) VALUES ("
       << song_id << ", '" << safe_lrc << "', '" << safe_tlrc << "');";

    return mysql_query(sc->get(), ss.str().c_str()) == 0;
}

// ==================== History ====================

bool DatabasePool::addPlayHistory(int64_t user_id, int64_t song_id, const std::string& song_name, const std::string& artist_name, const std::string& cover_url, int duration) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_name = escapeString(sc->get(), song_name);
    std::string safe_artist = escapeString(sc->get(), artist_name);
    std::string safe_cover = escapeString(sc->get(), cover_url);

    std::ostringstream ss;
    ss << "INSERT INTO play_history (user_id, song_id, song_name, artist_name, cover_url, duration) VALUES ("
       << user_id << ", " << song_id << ", '" << safe_name << "', '" << safe_artist << "', '" << safe_cover << "', " << duration << ");";

    return mysql_query(sc->get(), ss.str().c_str()) == 0;
}

std::vector<HistoryRecord> DatabasePool::getPlayHistory(int64_t user_id, int limit) {
    std::vector<HistoryRecord> list;
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return list;

    std::string sql = "SELECT id, user_id, song_id, song_name, artist_name, cover_url, duration, played_at FROM play_history WHERE user_id = " + std::to_string(user_id) + " ORDER BY id DESC LIMIT " + std::to_string(limit) + ";";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return list;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return list;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        HistoryRecord r;
        r.id = row[0] ? std::stoll(row[0]) : 0;
        r.user_id = row[1] ? std::stoll(row[1]) : 0;
        r.song_id = row[2] ? std::stoll(row[2]) : 0;
        r.song_name = row[3] ? row[3] : "";
        r.artist_name = row[4] ? row[4] : "";
        r.cover_url = row[5] ? row[5] : "";
        r.duration = row[6] ? std::stoi(row[6]) : 0;
        r.played_at = row[7] ? row[7] : "";
        list.push_back(r);
    }

    mysql_free_result(res);
    return list;
}

// ==================== User Preferences ====================

std::optional<UserPreferenceRecord> DatabasePool::getUserPreference(int64_t user_id) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return std::nullopt;

    std::string sql = "SELECT user_id, fav_genres, fav_artists, mood_tags, updated_at FROM user_preferences WHERE user_id = " + std::to_string(user_id) + " LIMIT 1;";
    if (mysql_query(sc->get(), sql.c_str()) != 0) return std::nullopt;

    MYSQL_RES* res = mysql_store_result(sc->get());
    if (!res) return std::nullopt;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return std::nullopt;
    }

    UserPreferenceRecord r;
    r.user_id = row[0] ? std::stoll(row[0]) : 0;
    r.fav_genres = row[1] ? row[1] : "";
    r.fav_artists = row[2] ? row[2] : "";
    r.mood_tags = row[3] ? row[3] : "";
    r.updated_at = row[4] ? row[4] : "";

    mysql_free_result(res);
    return r;
}

bool DatabasePool::updateUserPreference(int64_t user_id, const std::string& genres, const std::string& artists, const std::string& mood) {
    auto sc = getConnection();
    if (!sc || !sc->isValid()) return false;

    std::string safe_g = escapeString(sc->get(), genres);
    std::string safe_a = escapeString(sc->get(), artists);
    std::string safe_m = escapeString(sc->get(), mood);

    std::ostringstream ss;
    ss << "INSERT INTO user_preferences (user_id, fav_genres, fav_artists, mood_tags) VALUES ("
       << user_id << ", '" << safe_g << "', '" << safe_a << "', '" << safe_m << "') "
       << "ON DUPLICATE KEY UPDATE fav_genres=VALUES(fav_genres), fav_artists=VALUES(fav_artists), mood_tags=VALUES(mood_tags);";

    return mysql_query(sc->get(), ss.str().c_str()) == 0;
}
