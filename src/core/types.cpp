#include "ogw/types.hpp"

#include <cstdio>
#include <ctime>

namespace ogw {

std::string_view to_string(Quality quality) {
    switch (quality) {
        case Quality::Good:
            return "good";
        case Quality::Uncertain:
            return "uncertain";
        case Quality::Bad:
            return "bad";
        case Quality::Stale:
            return "stale";
    }
    return "uncertain";
}

std::string_view to_string(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::Connected:
            return "connected";
        case DeviceStatus::Disconnected:
            return "disconnected";
        case DeviceStatus::Faulted:
            return "faulted";
    }
    return "disconnected";
}

std::string_view to_string(ValueType type) {
    switch (type) {
        case ValueType::Number:
            return "number";
        case ValueType::Bool:
            return "bool";
        case ValueType::Enum:
            return "enum";
        case ValueType::String:
            return "string";
    }
    return "string";
}

std::optional<Quality> parse_quality(std::string_view text) {
    if (text == "good") return Quality::Good;
    if (text == "uncertain") return Quality::Uncertain;
    if (text == "bad") return Quality::Bad;
    if (text == "stale") return Quality::Stale;
    return std::nullopt;
}

std::optional<DeviceStatus> parse_device_status(std::string_view text) {
    if (text == "connected") return DeviceStatus::Connected;
    if (text == "disconnected") return DeviceStatus::Disconnected;
    if (text == "faulted") return DeviceStatus::Faulted;
    return std::nullopt;
}

std::optional<ValueType> parse_value_type(std::string_view text) {
    if (text == "number") return ValueType::Number;
    if (text == "bool") return ValueType::Bool;
    if (text == "enum") return ValueType::Enum;
    if (text == "string") return ValueType::String;
    return std::nullopt;
}

std::string format_time_utc(std::int64_t epoch_ms) {
    std::int64_t sec = epoch_ms / 1000;
    int milli = static_cast<int>(epoch_ms % 1000);
    if (milli < 0) {
        milli += 1000;
        --sec;
    }
    std::time_t seconds = static_cast<std::time_t>(sec);
    std::tm tm{};
    gmtime_r(&seconds, &tm);
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1,
                  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, milli);
    return buffer;
}

}  // namespace ogw
