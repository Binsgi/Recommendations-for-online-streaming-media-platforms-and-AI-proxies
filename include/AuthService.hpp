#pragma once

#include "json.hpp"
#include <string>
#include <optional>

class AuthService {
public:
    static AuthService& getInstance();

    // SHA-256 with salt
    static std::string hashPassword(const std::string& password, const std::string& salt = "demo_player_salt_2026");

    // 1. 发送验证码
    json sendVerificationCode(const std::string& target, const std::string& type = "register");

    // 2. 校验验证码
    json verifyCode(const std::string& target, const std::string& code);

    // 3. 用户注册
    json registerUser(const std::string& username, const std::string& password, const std::string& phone_email, const std::string& code, const std::string& nickname);

    // 4. 用户登录
    json login(const std::string& account, const std::string& password);

    // 5. 刷新登录 (Refresh Token)
    json refreshToken(const std::string& refresh_token);

    // 6. 修改/找回密码
    json resetPassword(const std::string& target, const std::string& code, const std::string& new_password);

    // 7. 获取用户信息
    json getUserProfile(int64_t user_id);

private:
    AuthService() = default;
    ~AuthService() = default;
    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;

    std::string generate6DigitCode();
};
