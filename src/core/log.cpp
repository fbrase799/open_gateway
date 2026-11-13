#include "ogw/log.hpp"

#include "ogw/clock.hpp"
#include "ogw/types.hpp"

#include <iostream>
#include <mutex>

namespace ogw {
namespace {

std::mutex& log_mutex() {
    static std::mutex mu;
    return mu;
}

LogLevel& level_slot() {
    static LogLevel level = LogLevel::Info;
    return level;
}

int rank(LogLevel level) {
    switch (level) {
        case LogLevel::Error:
            return 0;
        case LogLevel::Warn:
            return 1;
        case LogLevel::Info:
            return 2;
        case LogLevel::Debug:
            return 3;
    }
    return 2;
}

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Error:
            return "error";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Info:
            return "info";
        case LogLevel::Debug:
            return "debug";
    }
    return "info";
}

std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                if (c < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0x0f]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

}  // namespace

void set_log_level(LogLevel level) { level_slot() = level; }

LogLevel log_level() { return level_slot(); }

void log_message(LogLevel level, std::string_view component, std::string_view message,
                 const std::vector<std::pair<std::string, std::string>>& fields) {
    if (rank(level) > rank(level_slot())) {
        return;
    }
    SystemClock clock;
    std::string line = "{\"ts\":\"" + format_time_utc(clock.wall_time_ms()) + "\",\"level\":\"" + level_name(level) +
                       "\",\"component\":\"" + json_escape(component) + "\",\"msg\":\"" + json_escape(message) + "\"";
    for (const auto& [key, value] : fields) {
        line += ",\"";
        line += json_escape(key);
        line += "\":\"";
        line += json_escape(value);
        line += "\"";
    }
    line += "}";
    std::lock_guard lock(log_mutex());
    std::cout << line << '\n';
}

}  // namespace ogw
