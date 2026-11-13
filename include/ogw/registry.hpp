#pragma once

#include "ogw/types.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ogw {

struct DeviceDraft {
    std::string localId;
    std::optional<std::string> displayName;
    std::optional<DeviceStatus> status;
    std::optional<std::vector<Capability>> capabilities;
};

struct MutationResult {
    bool ok = false;
    std::string error;
    std::string deviceId;
};

class DeviceRegistry {
public:
    MutationResult upsert(std::string_view plugin_id, const DeviceDraft& draft, bool require_full);
    MutationResult remove(std::string_view plugin_id, std::string_view local_id);

    struct PublishResult {
        bool accepted = false;
        std::string reason;
        Reading stored;
    };

    PublishResult publish(const std::string& device_id, const std::string& capability_id, Reading reading,
                          std::int64_t received_at_ms);

    std::vector<DeviceSnapshot> mark_plugin_disconnected(std::string_view plugin_id);
    std::vector<std::pair<std::string, Reading>> mark_stale(std::int64_t now_ms);

    std::optional<DeviceSnapshot> find(std::string_view device_id) const;
    std::vector<DeviceSnapshot> list() const;
    std::size_t size() const;

private:
    struct Record {
        std::string pluginId;
        std::string localId;
        std::string displayName;
        DeviceStatus status = DeviceStatus::Connected;
        std::vector<Capability> capabilities;
        std::vector<std::pair<std::string, Reading>> readings;
    };

    static DeviceSnapshot to_snapshot(const Record& record);
    static std::string validate_capability(const Capability& capability);

    mutable std::mutex mu_;
    std::vector<Record> devices_;
};

}  // namespace ogw
