#pragma once

#include "Http.hpp"
#include <functional>
#include <map>
#include <string>

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class Router {
public:
    static Router& getInstance();

    void registerRoutes();

    HttpResponse dispatch(const HttpRequest& req);

private:
    Router();
    ~Router() = default;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    std::map<std::string, std::map<std::string, RouteHandler>> routes_; // method -> (path -> handler)

    void addRoute(const std::string& method, const std::string& path, RouteHandler handler);
    HttpResponse handleStatic(const HttpRequest& req);
};
