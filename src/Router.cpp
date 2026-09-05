#include "Router.hpp"
#include "Config.hpp"
#include "AuthService.hpp"
#include "MusicService.hpp"
#include "AgentService.hpp"
#include <iostream>
#include <fstream>

Router::Router() {
    registerRoutes();
}

Router& Router::getInstance() {
    static Router instance;
    return instance;
}

void Router::addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    routes_[method][path] = handler;
}

void Router::registerRoutes() {
    // ==================== 兼容旧版歌词接口 ====================
    addRoute("GET", "/song/lyric", [](const HttpRequest& req) {
        std::string id = req.getParam("id");
        if (id.empty()) return HttpResponse::badRequest("Missing song id parameter");
        return HttpResponse::json(MusicService::getInstance().getSongLyric(id));
    });

    // ==================== 用户认证与安全模块 ====================
    // 1. 发送验证码
    addRoute("POST", "/api/auth/send-code", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string target = body["target"].as_string();
        std::string type = body["type"].as_string("register");
        return HttpResponse::json(AuthService::getInstance().sendVerificationCode(target, type));
    });

    // 2. 校验验证码
    addRoute("POST", "/api/auth/verify-code", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string target = body["target"].as_string();
        std::string code = body["code"].as_string();
        return HttpResponse::json(AuthService::getInstance().verifyCode(target, code));
    });

    // 3. 用户注册
    addRoute("POST", "/api/auth/register", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string username = body["username"].as_string();
        std::string password = body["password"].as_string();
        std::string phone_email = body["phone_email"].as_string();
        std::string code = body["code"].as_string();
        std::string nickname = body["nickname"].as_string();
        return HttpResponse::json(AuthService::getInstance().registerUser(username, password, phone_email, code, nickname));
    });

    // 4. 用户登录
    addRoute("POST", "/api/auth/login", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string account = body["account"].as_string();
        std::string password = body["password"].as_string();
        return HttpResponse::json(AuthService::getInstance().login(account, password));
    });

    // 5. 刷新登录 (Refresh Token)
    addRoute("POST", "/api/auth/refresh-token", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string refresh_token = body["refresh_token"].as_string();
        return HttpResponse::json(AuthService::getInstance().refreshToken(refresh_token));
    });

    // 6. 重置/修改密码
    addRoute("POST", "/api/auth/reset-password", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string target = body["target"].as_string();
        std::string code = body["code"].as_string();
        std::string new_password = body["new_password"].as_string();
        return HttpResponse::json(AuthService::getInstance().resetPassword(target, code, new_password));
    });

    // 7. 获取用户信息
    addRoute("GET", "/api/user/profile", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        return HttpResponse::json(AuthService::getInstance().getUserProfile(req.user_id));
    });

    // ==================== 音乐生态与搜索 ====================
    // 8. 搜索建议
    addRoute("GET", "/api/search/suggest", [](const HttpRequest& req) {
        std::string keywords = req.getParam("keywords");
        if (keywords.empty()) return HttpResponse::badRequest("keywords is required");
        return HttpResponse::json(MusicService::getInstance().searchSuggest(keywords));
    });

    // 9. 搜索歌曲 (已自动剔除 url 为空的歌曲)
    addRoute("GET", "/api/song/search", [](const HttpRequest& req) {
        std::string keywords = req.getParam("keywords");
        if (keywords.empty()) return HttpResponse::badRequest("keywords is required");
        int limit = req.getParam("limit").empty() ? 30 : std::stoi(req.getParam("limit"));
        int offset = req.getParam("offset").empty() ? 0 : std::stoi(req.getParam("offset"));
        return HttpResponse::json(MusicService::getInstance().searchSongs(keywords, limit, offset));
    });

    // 10. 获取歌曲直链与音质
    addRoute("GET", "/api/song/url", [](const HttpRequest& req) {
        std::string id = req.getParam("id");
        if (id.empty()) return HttpResponse::badRequest("id is required");
        std::string level = req.getParam("level", "standard");
        return HttpResponse::json(MusicService::getInstance().getSongUrl(id, level));
    });

    // 11. 获取歌词
    addRoute("GET", "/api/song/lyric", [](const HttpRequest& req) {
        std::string id = req.getParam("id");
        if (id.empty()) return HttpResponse::badRequest("id is required");
        return HttpResponse::json(MusicService::getInstance().getSongLyric(id));
    });

    // 12. 获取歌单分类
    addRoute("GET", "/api/playlist/catlist", [](const HttpRequest&) {
        return HttpResponse::json(MusicService::getInstance().getPlaylistCatlist());
    });

    // 13. 获取热门歌单
    addRoute("GET", "/api/playlist/hot", [](const HttpRequest& req) {
        int limit = req.getParam("limit").empty() ? 20 : std::stoi(req.getParam("limit"));
        int offset = req.getParam("offset").empty() ? 0 : std::stoi(req.getParam("offset"));
        std::string cat = req.getParam("cat", "全部");
        return HttpResponse::json(MusicService::getInstance().getHotPlaylists(limit, offset, cat));
    });

    // 14. 获取歌单详情 (曲目已剔除不可播放歌曲)
    addRoute("GET", "/api/playlist/detail", [](const HttpRequest& req) {
        std::string id = req.getParam("id");
        if (id.empty()) return HttpResponse::badRequest("id is required");
        return HttpResponse::json(MusicService::getInstance().getPlaylistDetail(id));
    });

    // 15. 用户歌单列表
    addRoute("GET", "/api/user/playlists", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        return HttpResponse::json(MusicService::getInstance().getUserPlaylists(req.user_id));
    });

    // 16. 创建用户自建歌单
    addRoute("POST", "/api/user/playlists/create", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        json body = req.getJsonBody();
        std::string name = body["name"].as_string();
        std::string desc = body["description"].as_string();
        std::string cover = body["cover_url"].as_string();
        return HttpResponse::json(MusicService::getInstance().createUserPlaylist(req.user_id, name, desc, cover));
    });

    // 17. 获取歌单歌曲
    addRoute("GET", "/api/user/playlists/tracks", [](const HttpRequest& req) {
        std::string pid = req.getParam("playlist_id");
        if (pid.empty()) return HttpResponse::badRequest("playlist_id is required");
        return HttpResponse::json(MusicService::getInstance().getPlaylistTracks(std::stoll(pid)));
    });

    // ==================== 收藏与历史 ====================
    // 18. 收藏/取消收藏
    addRoute("POST", "/api/favorite/toggle", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        json body = req.getJsonBody();
        int64_t song_id = body["song_id"].as_int64();
        std::string name = body["name"].as_string();
        std::string artist = body["artist"].as_string();
        std::string album = body["album"].as_string();
        std::string cover = body["cover_url"].as_string();
        int duration = body["duration"].as_int();
        return HttpResponse::json(MusicService::getInstance().toggleFavorite(req.user_id, song_id, name, artist, album, cover, duration));
    });

    // 19. 获取收藏列表
    addRoute("GET", "/api/favorite/list", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        return HttpResponse::json(MusicService::getInstance().getFavorites(req.user_id));
    });

    // 20. 检查是否收藏
    addRoute("GET", "/api/favorite/check", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        std::string sid = req.getParam("song_id");
        if (sid.empty()) return HttpResponse::badRequest("song_id is required");
        return HttpResponse::json(MusicService::getInstance().checkFavorite(req.user_id, std::stoll(sid)));
    });

    // 21. 记录播放历史
    addRoute("POST", "/api/history/record", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        json body = req.getJsonBody();
        int64_t song_id = body["song_id"].as_int64();
        std::string name = body["name"].as_string();
        std::string artist = body["artist"].as_string();
        std::string cover = body["cover_url"].as_string();
        int duration = body["duration"].as_int();
        return HttpResponse::json(MusicService::getInstance().recordHistory(req.user_id, song_id, name, artist, cover, duration));
    });

    // 22. 获取播放历史
    addRoute("GET", "/api/history/list", [](const HttpRequest& req) {
        if (req.user_id <= 0) return HttpResponse::unauthorized("Please login first");
        int limit = req.getParam("limit").empty() ? 50 : std::stoi(req.getParam("limit"));
        return HttpResponse::json(MusicService::getInstance().getHistory(req.user_id, limit));
    });

    // ==================== 🤖 AI Agent 个性化推荐 ====================
    addRoute("POST", "/api/agent/recommend", [](const HttpRequest& req) {
        json body = req.getJsonBody();
        std::string prompt = body["prompt"].as_string();
        std::string mood = body["mood"].as_string();
        std::string genre = body["genre"].as_string();
        return HttpResponse::json(AgentService::getInstance().generateRecommendation(req.user_id, prompt, mood, genre));
    });
}

HttpResponse Router::handleStatic(const HttpRequest& req) {
    std::string root = Config::getInstance().web_root;
    std::string path = req.path;

    if (path == "/" || path.empty()) {
        path = "/index.html";
    }

    std::string full_path = root + path;
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        // Fallback check: if requested /MusicPlayer.html or index.html
        if (path == "/MusicPlayer.html") {
            full_path = root + "/MusicPlayer.html";
            std::ifstream file2(full_path, std::ios::binary);
            if (file2.is_open()) {
                return HttpResponse::file(full_path);
            }
        }
        return HttpResponse::notFound("The requested resource was not found: " + path);
    }

    return HttpResponse::file(full_path);
}

HttpResponse Router::dispatch(const HttpRequest& req) {
    // 1. CORS OPTIONS Preflight
    if (req.method == "OPTIONS") {
        HttpResponse res;
        res.status_code = 204;
        res.status_message = "No Content";
        res.headers["Access-Control-Allow-Origin"] = "*";
        res.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With";
        return res;
    }

    // 2. Check registered routes
    auto method_it = routes_.find(req.method);
    if (method_it != routes_.end()) {
        auto path_it = method_it->second.find(req.path);
        if (path_it != method_it->second.end()) {
            return path_it->second(req);
        }
    }

    // 3. Static files
    if (req.method == "GET") {
        return handleStatic(req);
    }

    return HttpResponse::notFound();
}
