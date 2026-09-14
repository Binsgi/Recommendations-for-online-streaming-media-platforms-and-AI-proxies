#include "Http.hpp"
#include "Cache.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

std::string HttpUtils::urlDecode(const std::string& src) {
    std::string dst;
    char a, b;
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '%' && i + 2 < src.length() &&
            std::isxdigit(src[i + 1]) && std::isxdigit(src[i + 2])) {
            a = src[++i];
            b = src[++i];
            auto hex2int = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            dst += static_cast<char>((hex2int(a) << 4) | hex2int(b));
        } else if (src[i] == '+') {
            dst += ' ';
        } else {
            dst += src[i];
        }
    }
    return dst;
}

std::string HttpUtils::getMimeType(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".txt") return "text/plain; charset=utf-8";

    return "application/octet-stream";
}

std::string HttpUtils::getStatusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 409: return "Conflict";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

// ==================== HttpRequest ====================

std::string HttpRequest::getParam(const std::string& key, const std::string& default_val) const {
    auto it = query_params.find(key);
    return (it != query_params.end()) ? it->second : default_val;
}

std::string HttpRequest::getHeader(const std::string& key) const {
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
    auto it = headers.find(lower_key);
    return (it != headers.end()) ? it->second : "";
}

json HttpRequest::getJsonBody() const {
    if (body.empty()) return json::object();
    return json::parse(body);
}

HttpRequest HttpRequest::parse(const std::string& raw_request) {
    HttpRequest req;
    std::istringstream stream(raw_request);
    std::string line;

    // 1. Request Line
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream line_stream(line);
        std::string http_ver;
        line_stream >> req.method >> req.raw_path >> http_ver;

        // Parse path & query
        size_t q_pos = req.raw_path.find('?');
        if (q_pos != std::string::npos) {
            req.path = req.raw_path.substr(0, q_pos);
            req.query_string = req.raw_path.substr(q_pos + 1);

            // Split key-value params
            std::istringstream q_stream(req.query_string);
            std::string pair;
            while (std::getline(q_stream, pair, '&')) {
                size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    std::string k = HttpUtils::urlDecode(pair.substr(0, eq));
                    std::string v = HttpUtils::urlDecode(pair.substr(eq + 1));
                    req.query_params[k] = v;
                }
            }
        } else {
            req.path = req.raw_path;
        }
    }

    // 2. Headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break; // Body delimiter

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string k = line.substr(0, colon);
            std::string v = line.substr(colon + 1);

            // Trim
            size_t first = v.find_first_not_of(" \t");
            if (first != std::string::npos) v = v.substr(first);

            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
            req.headers[k] = v;
        }
    }

    // 3. Body
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    req.body = body_stream.str();

    // 4. Token Check
    std::string auth_header = req.getHeader("authorization");
    if (auth_header.rfind("Bearer ", 0) == 0) {
        req.token = auth_header.substr(7);
    } else if (req.query_params.find("token") != req.query_params.end()) {
        req.token = req.query_params["token"];
    }

    if (!req.token.empty()) {
        auto token_info = CacheManager::getInstance().validateToken(req.token);
        if (token_info.has_value()) {
            req.user_id = token_info->user_id;
            req.username = token_info->username;
        }
    }

    return req;
}

// ==================== HttpResponse ====================

HttpResponse HttpResponse::json(const ::json& j, int status) {
    HttpResponse res;
    res.status_code = status;
    res.status_message = HttpUtils::getStatusText(status);
    res.headers["Content-Type"] = "application/json; charset=utf-8";
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With";
    res.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    res.body = j.dump();
    return res;
}

HttpResponse HttpResponse::text(const std::string& text, int status) {
    HttpResponse res;
    res.status_code = status;
    res.status_message = HttpUtils::getStatusText(status);
    res.headers["Content-Type"] = "text/plain; charset=utf-8";
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.body = text;
    return res;
}

HttpResponse HttpResponse::file(const std::string& path, const std::string& mime_type) {
    HttpResponse res;
    res.is_file = true;
    res.file_path = path;
    res.status_code = 200;
    res.status_message = "OK";
    res.headers["Content-Type"] = mime_type.empty() ? HttpUtils::getMimeType(path) : mime_type;
    res.headers["Access-Control-Allow-Origin"] = "*";

    // Read content
    std::ifstream ifs(path, std::ios::binary);
    if (ifs) {
        std::ostringstream ss;
        ss << ifs.rdbuf();
        res.body = ss.str();
    } else {
        return notFound("File not found");
    }
    return res;
}

HttpResponse HttpResponse::notFound(const std::string& msg) {
    HttpResponse res;
    res.status_code = 404;
    res.status_message = "Not Found";
    res.headers["Content-Type"] = "text/html; charset=utf-8";
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.body = "<h1>404 Not Found</h1><p>" + msg + "</p>";
    return res;
}

HttpResponse HttpResponse::badRequest(const std::string& msg) {
    ::json err = ::json::object();
    err["code"] = 400;
    err["message"] = msg;
    return json(err, 400);
}

HttpResponse HttpResponse::unauthorized(const std::string& msg) {
    ::json err = ::json::object();
    err["code"] = 401;
    err["message"] = msg;
    return json(err, 401);
}

HttpResponse HttpResponse::error(const std::string& msg) {
    ::json err = ::json::object();
    err["code"] = 500;
    err["message"] = msg;
    return json(err, 500);
}

std::string HttpResponse::serialize() const {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";

    for (const auto& [k, v] : headers) {
        ss << k << ": " << v << "\r\n";
    }

    ss << "Content-Length: " << body.length() << "\r\n";
    ss << "Connection: close\r\n";
    ss << "\r\n";
    ss << body;

    return ss.str();
}
