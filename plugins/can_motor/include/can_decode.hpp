#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct CanSignal {
    std::string capabilityId;
    std::string valueType;
    std::string unit;
    int startByte = 0;
    int length = 1;
    double scale = 1;
    bool isSigned = false;
};

struct CanFrameMap {
    std::uint32_t id = 0;
    std::string deviceLocalId;
    std::string displayName;
    std::vector<CanSignal> signals;
};

struct DecodedSignal {
    std::string capabilityId;
    double value = 0;
};

inline std::optional<double> decode_signal(const std::uint8_t* data, std::size_t size, const CanSignal& signal) {
    if (data == nullptr || signal.length <= 0 || signal.startByte < 0 ||
        static_cast<std::size_t>(signal.startByte + signal.length) > size || signal.length > 4) {
        return std::nullopt;
    }
    std::uint32_t raw = 0;
    for (int index = 0; index < signal.length; ++index) {
        raw |= static_cast<std::uint32_t>(data[signal.startByte + index]) << (8 * index);
    }
    if (signal.isSigned) {
        const int shift = (4 - signal.length) * 8;
        const auto extended = static_cast<std::int32_t>(raw << shift) >> shift;
        return static_cast<double>(extended) * signal.scale;
    }
    return static_cast<double>(raw) * signal.scale;
}

inline std::vector<DecodedSignal> decode_frame(const CanFrameMap& frame, const std::uint8_t* data, std::size_t size) {
    std::vector<DecodedSignal> decoded;
    for (const auto& signal : frame.signals) {
        if (auto value = decode_signal(data, size, signal)) {
            decoded.push_back({signal.capabilityId, *value});
        }
    }
    return decoded;
}
