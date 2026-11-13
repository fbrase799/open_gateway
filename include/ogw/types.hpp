#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ogw {

enum class Quality { Good, Uncertain, Bad, Stale };
enum class DeviceStatus { Connected, Disconnected, Faulted };
enum class ValueType { Number, Bool, Enum, String };

struct EnumState {
    std::string id;
    std::string label;
    std::string tone;
};

struct Capability {
    std::string id;
    std::string kind;
    ValueType valueType = ValueType::Number;
    std::string displayName;
    std::optional<std::string> unit;
    std::string presentation;
    std::optional<double> rangeMin;
    std::optional<double> rangeMax;
    std::vector<EnumState> enumStates;
    std::int64_t staleAfterMs = 5000;
};

using Value = std::variant<double, bool, std::string>;

struct Reading {
    Value value;
    Quality quality = Quality::Good;
    std::int64_t sourceTimestampMs = 0;
    std::int64_t receivedAtMs = 0;
    std::optional<std::string> detail;
};

struct HistorySample {
    Value value;
    Quality quality = Quality::Good;
    std::int64_t sourceTimestampMs = 0;
    std::int64_t receivedAtMs = 0;
};

struct DeviceSnapshot {
    std::string deviceId;
    std::string pluginId;
    std::string localId;
    std::string displayName;
    DeviceStatus status = DeviceStatus::Connected;
    std::vector<Capability> capabilities;
    std::vector<std::pair<std::string, Reading>> readings;
};

inline bool is_id_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
           c == '-';
}

inline bool valid_id(std::string_view id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (char c : id) {
        if (!is_id_char(c)) {
            return false;
        }
    }
    return true;
}

inline std::string device_key(std::string_view plugin_id, std::string_view local_id) {
    std::string key;
    key.reserve(plugin_id.size() + local_id.size() + 1);
    key.append(plugin_id);
    key.push_back('/');
    key.append(local_id);
    return key;
}

std::string_view to_string(Quality quality);
std::string_view to_string(DeviceStatus status);
std::string_view to_string(ValueType type);
std::optional<Quality> parse_quality(std::string_view text);
std::optional<DeviceStatus> parse_device_status(std::string_view text);
std::optional<ValueType> parse_value_type(std::string_view text);
std::string format_time_utc(std::int64_t epoch_ms);

}  // namespace ogw
