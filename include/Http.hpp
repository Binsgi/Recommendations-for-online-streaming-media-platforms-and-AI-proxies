#pragma once

#include "json.hpp"
#include <string>
#include <map>
#include <vector>
#include <optional>

struct HttpRequest {
    std::string method;
    std::string raw_path;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string token;
    int64_t user_id = 0;
    std::string username;

    std::string getParam(const std::string& key, const std::string& default_val = "") const;
    std::string getHeader(const std::string& key) const;
    json getJsonBody() const;

    static HttpRequest parse(const std::string& raw_request);
};

struct HttpResponse {
    int status_code = 200;
    std::string status_message = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    bool is_file = false;
    std::string file_path;

    static HttpResponse json(const ::json& j, int status = 200);
    static HttpResponse text(const std::string& text, int status = 200);
    static HttpResponse file(const std::string& path, const std::string& mime_type = "");
    static HttpResponse notFound(const std::string& msg = "404 Not Found");
    static HttpResponse badRequest(const std::string& msg = "400 Bad Request");
    static HttpResponse unauthorized(const std::string& msg = "401 Unauthorized");
    static HttpResponse error(const std::string& msg = "500 Internal Server Error");

    std::string serialize() const;
};

class HttpUtils {
public:
    static std::string urlDecode(const std::string& src);
    static std::string getMimeType(const std::string& path);
    static std::string getStatusText(int code);
};
