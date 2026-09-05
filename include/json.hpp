#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <memory>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iomanip>

namespace simple_json {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    JsonValue() : type(JsonType::Null) {}
    JsonValue(std::nullptr_t) : type(JsonType::Null) {}
    JsonValue(bool b) : type(JsonType::Boolean), bool_val(b) {}
    JsonValue(int n) : type(JsonType::Number), num_val(n) {}
    JsonValue(long n) : type(JsonType::Number), num_val(n) {}
    JsonValue(long long n) : type(JsonType::Number), num_val(n) {}
    JsonValue(unsigned int n) : type(JsonType::Number), num_val(n) {}
    JsonValue(unsigned long n) : type(JsonType::Number), num_val(n) {}
    JsonValue(unsigned long long n) : type(JsonType::Number), num_val(n) {}
    JsonValue(double d) : type(JsonType::Number), num_val(d) {}
    JsonValue(const char* s) : type(JsonType::String), str_val(s ? s : "") {}
    JsonValue(const std::string& s) : type(JsonType::String), str_val(s) {}
    JsonValue(std::string_view s) : type(JsonType::String), str_val(s) {}

    static JsonValue array() {
        JsonValue v;
        v.type = JsonType::Array;
        return v;
    }

    static JsonValue object() {
        JsonValue v;
        v.type = JsonType::Object;
        return v;
    }

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Boolean; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    bool as_bool(bool default_val = false) const {
        return is_bool() ? bool_val : default_val;
    }

    int as_int(int default_val = 0) const {
        return is_number() ? static_cast<int>(num_val) : default_val;
    }

    int64_t as_int64(int64_t default_val = 0) const {
        return is_number() ? static_cast<int64_t>(num_val) : default_val;
    }

    double as_double(double default_val = 0.0) const {
        return is_number() ? num_val : default_val;
    }

    const std::string& as_string(const std::string& default_val = "") const {
        return is_string() ? str_val : default_val;
    }

    void push_back(const JsonValue& val) {
        if (type != JsonType::Array) {
            type = JsonType::Array;
        }
        arr_val.push_back(val);
    }

    bool contains(const std::string& key) const {
        if (type != JsonType::Object) return false;
        return obj_val.find(key) != obj_val.end();
    }

    JsonValue& operator[](const std::string& key) {
        if (type != JsonType::Object) {
            type = JsonType::Object;
        }
        return obj_val[key];
    }

    const JsonValue& operator[](const std::string& key) const {
        static const JsonValue null_val;
        if (type != JsonType::Object) return null_val;
        auto it = obj_val.find(key);
        return it != obj_val.end() ? it->second : null_val;
    }

    JsonValue& operator[](size_t index) {
        if (type != JsonType::Array) {
            type = JsonType::Array;
        }
        if (index >= arr_val.size()) {
            arr_val.resize(index + 1);
        }
        return arr_val[index];
    }

    const JsonValue& operator[](size_t index) const {
        static const JsonValue null_val;
        if (type != JsonType::Array || index >= arr_val.size()) return null_val;
        return arr_val[index];
    }

    size_t size() const {
        if (type == JsonType::Array) return arr_val.size();
        if (type == JsonType::Object) return obj_val.size();
        return 0;
    }

    std::string dump(int indent = -1, int current_indent = 0) const {
        std::ostringstream ss;
        std::string indent_str = (indent >= 0) ? std::string(current_indent, ' ') : "";
        std::string next_indent_str = (indent >= 0) ? std::string(current_indent + indent, ' ') : "";
        std::string nl = (indent >= 0) ? "\n" : "";
        std::string sp = (indent >= 0) ? " " : "";

        switch (type) {
            case JsonType::Null:
                ss << "null";
                break;
            case JsonType::Boolean:
                ss << (bool_val ? "true" : "false");
                break;
            case JsonType::Number:
                if (num_val == static_cast<int64_t>(num_val)) {
                    ss << static_cast<int64_t>(num_val);
                } else {
                    ss << std::setprecision(10) << num_val;
                }
                break;
            case JsonType::String: {
                ss << "\"";
                for (char c : str_val) {
                    switch (c) {
                        case '"': ss << "\\\""; break;
                        case '\\': ss << "\\\\"; break;
                        case '\b': ss << "\\b"; break;
                        case '\f': ss << "\\f"; break;
                        case '\n': ss << "\\n"; break;
                        case '\r': ss << "\\r"; break;
                        case '\t': ss << "\\t"; break;
                        default:
                            if (static_cast<unsigned char>(c) < 0x20) {
                                ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                            } else {
                                ss << c;
                            }
                            break;
                    }
                }
                ss << "\"";
                break;
            }
            case JsonType::Array: {
                if (arr_val.empty()) {
                    ss << "[]";
                } else {
                    ss << "[" << nl;
                    for (size_t i = 0; i < arr_val.size(); ++i) {
                        ss << next_indent_str << arr_val[i].dump(indent, current_indent + (indent >= 0 ? indent : 0));
                        if (i + 1 < arr_val.size()) ss << ",";
                        ss << nl;
                    }
                    ss << indent_str << "]";
                }
                break;
            }
            case JsonType::Object: {
                if (obj_val.empty()) {
                    ss << "{}";
                } else {
                    ss << "{" << nl;
                    size_t i = 0;
                    for (const auto& [k, v] : obj_val) {
                        ss << next_indent_str << "\"" << k << "\":" << sp << v.dump(indent, current_indent + (indent >= 0 ? indent : 0));
                        if (++i < obj_val.size()) ss << ",";
                        ss << nl;
                    }
                    ss << indent_str << "}";
                }
                break;
            }
        }
        return ss.str();
    }

    static JsonValue parse(const std::string& json_str) {
        size_t pos = 0;
        return parse_value(json_str, pos);
    }

private:
    static void skip_whitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && (std::isspace(s[pos]) || s[pos] == '\r' || s[pos] == '\n' || s[pos] == '\t')) {
            ++pos;
        }
    }

    static JsonValue parse_value(const std::string& s, size_t& pos) {
        skip_whitespace(s, pos);
        if (pos >= s.size()) return JsonValue();

        char c = s[pos];
        if (c == '{') return parse_object(s, pos);
        if (c == '[') return parse_array(s, pos);
        if (c == '"') return parse_string(s, pos);
        if (c == 't' || c == 'f') return parse_bool(s, pos);
        if (c == 'n') return parse_null(s, pos);
        if (c == '-' || std::isdigit(c)) return parse_number(s, pos);

        return JsonValue();
    }

    static JsonValue parse_null(const std::string& s, size_t& pos) {
        if (s.compare(pos, 4, "null") == 0) {
            pos += 4;
            return JsonValue();
        }
        return JsonValue();
    }

    static JsonValue parse_bool(const std::string& s, size_t& pos) {
        if (s.compare(pos, 4, "true") == 0) {
            pos += 4;
            return JsonValue(true);
        }
        if (s.compare(pos, 5, "false") == 0) {
            pos += 5;
            return JsonValue(false);
        }
        return JsonValue();
    }

    static JsonValue parse_number(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) {
            if ((s[pos] == '+' || s[pos] == '-') && (s[pos-1] != 'e' && s[pos-1] != 'E')) break;
            ++pos;
        }
        std::string num_str = s.substr(start, pos - start);
        try {
            double val = std::stod(num_str);
            return JsonValue(val);
        } catch (...) {
            return JsonValue(0);
        }
    }

    static std::string parse_string_raw(const std::string& s, size_t& pos) {
        std::string result;
        if (pos >= s.size() || s[pos] != '"') return result;
        ++pos;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < s.size()) {
                char next = s[pos++];
                switch (next) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos + 4 <= s.size()) {
                            std::string hex_str = s.substr(pos, 4);
                            pos += 4;
                            try {
                                uint32_t codepoint = std::stoul(hex_str, nullptr, 16);
                                if (codepoint <= 0x7F) {
                                    result += static_cast<char>(codepoint);
                                } else if (codepoint <= 0x7FF) {
                                    result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                                } else {
                                    result += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                                }
                            } catch (...) {}
                        }
                        break;
                    }
                    default: result += next; break;
                }
            } else {
                result += c;
            }
        }
        return result;
    }

    static JsonValue parse_string(const std::string& s, size_t& pos) {
        return JsonValue(parse_string_raw(s, pos));
    }

    static JsonValue parse_array(const std::string& s, size_t& pos) {
        JsonValue arr = JsonValue::array();
        ++pos; // skip '['
        skip_whitespace(s, pos);
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            return arr;
        }

        while (pos < s.size()) {
            arr.push_back(parse_value(s, pos));
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') {
                ++pos;
            } else if (pos < s.size() && s[pos] == ']') {
                ++pos;
                break;
            } else {
                ++pos;
            }
        }
        return arr;
    }

    static JsonValue parse_object(const std::string& s, size_t& pos) {
        JsonValue obj = JsonValue::object();
        ++pos; // skip '{'
        skip_whitespace(s, pos);
        if (pos < s.size() && s[pos] == '}') {
            ++pos;
            return obj;
        }

        while (pos < s.size()) {
            skip_whitespace(s, pos);
            if (pos >= s.size() || s[pos] != '"') break;
            std::string key = parse_string_raw(s, pos);
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ':') {
                ++pos;
            }
            JsonValue val = parse_value(s, pos);
            obj[key] = val;

            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') {
                ++pos;
            } else if (pos < s.size() && s[pos] == '}') {
                ++pos;
                break;
            } else {
                ++pos;
            }
        }
        return obj;
    }
};

} // namespace simple_json

using json = simple_json::JsonValue;
