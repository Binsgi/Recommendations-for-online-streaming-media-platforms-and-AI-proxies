#include "AuthService.hpp"
#include "Database.hpp"
#include "Cache.hpp"
#include "Config.hpp"
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

AuthService& AuthService::getInstance() {
    static AuthService instance;
    return instance;
}

std::string AuthService::hashPassword(const std::string& password, const std::string& salt) {
    std::string combined = password + "::" + salt;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string AuthService::generate6DigitCode() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<int> dis(100000, 999999);
    return std::to_string(dis(gen));
}

json AuthService::sendVerificationCode(const std::string& target, const std::string& type) {
    json res = json::object();
    if (target.empty()) {
        res["code"] = 400;
        res["message"] = "手机号或邮箱不能为空";
        return res;
    }

    std::string code = generate6DigitCode();
    int ttl = Config::getInstance().code_ttl;
    CacheManager::getInstance().storeVerificationCode(target, code, ttl);

    std::cout << "[Auth] Generated verification code for [" << target << "] (type: " << type 
              << "): " << code << " (valid for " << ttl << "s)\n";

    res["code"] = 200;
    res["message"] = "验证码发送成功";
    json data = json::object();
    data["target"] = target;
    data["ttl"] = ttl;
    data["debug_code"] = code; // 提供调试方便前端直接填入或短信模拟
    res["data"] = data;
    return res;
}

json AuthService::verifyCode(const std::string& target, const std::string& code) {
    json res = json::object();
    if (target.empty() || code.empty()) {
        res["code"] = 400;
        res["message"] = "参数不完整";
        return res;
    }

    bool ok = CacheManager::getInstance().verifyCode(target, code, false);
    if (!ok) {
        res["code"] = 400;
        res["message"] = "验证码错误或已过期";
        return res;
    }

    res["code"] = 200;
    res["message"] = "验证码校验通过";
    return res;
}

json AuthService::registerUser(const std::string& username, const std::string& password, const std::string& phone_email, const std::string& code, const std::string& nickname) {
    json res = json::object();

    if (username.empty() || password.empty()) {
        res["code"] = 400;
        res["message"] = "用户名和密码不能为空";
        return res;
    }

    // 如果传入了验证码目标，则校验验证码
    if (!phone_email.empty() && !code.empty()) {
        if (!CacheManager::getInstance().verifyCode(phone_email, code, true)) {
            res["code"] = 400;
            res["message"] = "验证码不正确或已过期";
            return res;
        }
    }

    // 检查用户名是否已存在
    auto existing = DatabasePool::getInstance().findUserByUsername(username);
    if (existing.has_value()) {
        res["code"] = 409;
        res["message"] = "用户名已被注册，请更换用户名";
        return res;
    }

    std::string pass_hash = hashPassword(password);
    int64_t uid = DatabasePool::getInstance().createUser(username, phone_email, pass_hash, nickname);
    if (uid <= 0) {
        res["code"] = 500;
        res["message"] = "创建用户失败，请检查数据库服务";
        return res;
    }

    res["code"] = 200;
    res["message"] = "注册成功";
    json data = json::object();
    data["user_id"] = uid;
    data["username"] = username;
    data["nickname"] = nickname.empty() ? username : nickname;
    res["data"] = data;
    return res;
}

json AuthService::login(const std::string& account, const std::string& password) {
    json res = json::object();
    if (account.empty() || password.empty()) {
        res["code"] = 400;
        res["message"] = "账号和密码不能为空";
        return res;
    }

    auto user_opt = DatabasePool::getInstance().findUserByPhoneEmail(account);
    if (!user_opt.has_value()) {
        res["code"] = 404;
        res["message"] = "用户不存在";
        return res;
    }

    const auto& user = user_opt.value();
    std::string pass_hash = hashPassword(password);
    if (user.password_hash != pass_hash) {
        res["code"] = 401;
        res["message"] = "密码错误";
        return res;
    }

    const auto& conf = Config::getInstance();
    std::string token = CacheManager::getInstance().generateToken(user.id, user.username, conf.token_ttl);
    std::string refresh_token = CacheManager::getInstance().generateRefreshToken(user.id, user.username, conf.refresh_token_ttl);

    res["code"] = 200;
    res["message"] = "登录成功";
    json data = json::object();
    data["token"] = token;
    data["refresh_token"] = refresh_token;
    data["expires_in"] = conf.token_ttl;

    json uinfo = json::object();
    uinfo["id"] = user.id;
    uinfo["username"] = user.username;
    uinfo["nickname"] = user.nickname;
    uinfo["phone_email"] = user.phone_email;
    uinfo["avatar_url"] = user.avatar_url;
    data["user"] = uinfo;

    res["data"] = data;
    return res;
}

json AuthService::refreshToken(const std::string& refresh_token) {
    json res = json::object();
    if (refresh_token.empty()) {
        res["code"] = 400;
        res["message"] = "Refresh Token 不能为空";
        return res;
    }

    auto token_info = CacheManager::getInstance().validateRefreshToken(refresh_token);
    if (!token_info.has_value()) {
        res["code"] = 401;
        res["message"] = "Refresh Token 无效或已过期，请重新登录";
        return res;
    }

    const auto& conf = Config::getInstance();
    std::string new_token = CacheManager::getInstance().generateToken(token_info->user_id, token_info->username, conf.token_ttl);

    res["code"] = 200;
    res["message"] = "Token 刷新成功";
    json data = json::object();
    data["token"] = new_token;
    data["expires_in"] = conf.token_ttl;
    res["data"] = data;
    return res;
}

json AuthService::resetPassword(const std::string& target, const std::string& code, const std::string& new_password) {
    json res = json::object();
    if (target.empty() || code.empty() || new_password.empty()) {
        res["code"] = 400;
        res["message"] = "参数不完整";
        return res;
    }

    if (!CacheManager::getInstance().verifyCode(target, code, true)) {
        res["code"] = 400;
        res["message"] = "验证码错误或已过期";
        return res;
    }

    std::string new_hash = hashPassword(new_password);
    bool ok = DatabasePool::getInstance().updatePasswordByTarget(target, new_hash);
    if (!ok) {
        res["code"] = 500;
        res["message"] = "修改密码失败，目标用户可能不存在";
        return res;
    }

    res["code"] = 200;
    res["message"] = "密码重置成功，请使用新密码登录";
    return res;
}

json AuthService::getUserProfile(int64_t user_id) {
    json res = json::object();
    auto user_opt = DatabasePool::getInstance().findUserById(user_id);
    if (!user_opt.has_value()) {
        res["code"] = 404;
        res["message"] = "用户不存在";
        return res;
    }

    const auto& u = user_opt.value();
    res["code"] = 200;
    json data = json::object();
    data["id"] = u.id;
    data["username"] = u.username;
    data["nickname"] = u.nickname;
    data["phone_email"] = u.phone_email;
    data["avatar_url"] = u.avatar_url;
    data["created_at"] = u.created_at;
    res["data"] = data;
    return res;
}
