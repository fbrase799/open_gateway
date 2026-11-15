#include "ogw/registry.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ogw {
namespace {

const Capability* find_capability(const std::vector<Capability>& capabilities, const std::string& id) {
    for (const auto& capability : capabilities) {
        if (capability.id == id) {
            return &capability;
        }
    }
    return nullptr;
}

Reading* find_reading(std::vector<std::pair<std::string, Reading>>& readings, const std::string& id) {
    for (auto& entry : readings) {
        if (entry.first == id) {
            return &entry.second;
        }
    }
    return nullptr;
}

const Reading* find_reading_const(const std::vector<std::pair<std::string, Reading>>& readings, const std::string& id) {
    for (const auto& entry : readings) {
        if (entry.first == id) {
            return &entry.second;
        }
    }
    return nullptr;
}

bool value_matches(ValueType type, const Value& value) {
    switch (type) {
        case ValueType::Number:
            return std::holds_alternative<double>(value);
        case ValueType::Bool:
            return std::holds_alternative<bool>(value);
        case ValueType::Enum:
        case ValueType::String:
            return std::holds_alternative<std::string>(value);
    }
    return false;
}

}  // namespace

std::string DeviceRegistry::validate_capability(const Capability& capability) {
    if (!valid_id(capability.id)) {
        return "capability id is invalid";
    }
    if (capability.kind != "measurement" && capability.kind != "command") {
        return "capability kind is invalid";
    }
    if (capability.displayName.empty() || capability.displayName.size() > 80) {
        return "capability displayName is invalid";
    }
    if (capability.staleAfterMs <= 0) {
        return "staleAfterMs must be positive";
    }
    if (capability.valueType == ValueType::Enum && capability.enumStates.empty()) {
        return "enum capability requires enumStates";
    }
    for (const auto& state : capability.enumStates) {
        if (!valid_id(state.id) || state.label.empty()) {
            return "enum state is invalid";
        }
        if (state.tone != "neutral" && state.tone != "ok" && state.tone != "warn" && state.tone != "fault") {
            return "enum tone is invalid";
        }
    }
    if (capability.rangeMin && capability.rangeMax && *capability.rangeMin > *capability.rangeMax) {
        return "range min is greater than max";
    }
    return {};
}

DeviceSnapshot DeviceRegistry::to_snapshot(const Record& record) {
    DeviceSnapshot snapshot;
    snapshot.deviceId = device_key(record.pluginId, record.localId);
    snapshot.pluginId = record.pluginId;
    snapshot.localId = record.localId;
    snapshot.displayName = record.displayName;
    snapshot.status = record.status;
    snapshot.capabilities = record.capabilities;
    snapshot.readings = record.readings;
    return snapshot;
}

MutationResult DeviceRegistry::upsert(std::string_view plugin_id, const DeviceDraft& draft, bool require_full) {
    MutationResult result;
    if (!valid_id(plugin_id)) {
        result.error = "plugin id is invalid";
        return result;
    }
    if (!valid_id(draft.localId)) {
        result.error = "localId is invalid";
        return result;
    }
    if (require_full) {
        if (!draft.displayName || draft.displayName->empty()) {
            result.error = "displayName is required";
            return result;
        }
        if (!draft.capabilities || draft.capabilities->empty()) {
            result.error = "capabilities are required";
            return result;
        }
    }
    if (draft.displayName && (draft.displayName->empty() || draft.displayName->size() > 80)) {
        result.error = "displayName is invalid";
        return result;
    }
    if (draft.capabilities) {
        std::vector<std::string> seen;
        for (const auto& capability : *draft.capabilities) {
            if (auto error = validate_capability(capability); !error.empty()) {
                result.error = error;
                return result;
            }
            if (std::find(seen.begin(), seen.end(), capability.id) != seen.end()) {
                result.error = "duplicate capability id";
                return result;
            }
            seen.push_back(capability.id);
        }
    }

    std::lock_guard lock(mu_);
    Record* record = nullptr;
    for (auto& existing : devices_) {
        if (existing.pluginId == plugin_id && existing.localId == draft.localId) {
            record = &existing;
            break;
        }
    }
    if (record == nullptr) {
        if (!draft.displayName || !draft.capabilities) {
            result.error = "unknown device";
            return result;
        }
        devices_.push_back(Record{});
        record = &devices_.back();
        record->pluginId = std::string(plugin_id);
        record->localId = draft.localId;
    }
    if (draft.displayName) {
        record->displayName = *draft.displayName;
    }
    if (draft.status) {
        record->status = *draft.status;
    }
    if (draft.capabilities) {
        record->capabilities = *draft.capabilities;
        std::vector<std::pair<std::string, Reading>> kept;
        for (const auto& capability : record->capabilities) {
            if (const Reading* existing = find_reading_const(record->readings, capability.id)) {
                kept.emplace_back(capability.id, *existing);
            }
        }
        record->readings = std::move(kept);
    }
    result.ok = true;
    result.deviceId = device_key(record->pluginId, record->localId);
    return result;
}

MutationResult DeviceRegistry::remove(std::string_view plugin_id, std::string_view local_id) {
    MutationResult result;
    std::lock_guard lock(mu_);
    const auto key = device_key(plugin_id, local_id);
    const auto it = std::remove_if(devices_.begin(), devices_.end(), [&](const Record& record) {
        return record.pluginId == plugin_id && record.localId == local_id;
    });
    if (it == devices_.end()) {
        result.error = "unknown device";
        return result;
    }
    devices_.erase(it, devices_.end());
    result.ok = true;
    result.deviceId = key;
    return result;
}

DeviceRegistry::PublishResult DeviceRegistry::publish(const std::string& device_id, const std::string& capability_id,
                                                      Reading reading, std::int64_t received_at_ms) {
    PublishResult result;
    if (reading.detail && reading.detail->size() > 128) {
        result.reason = "detail_too_long";
        return result;
    }
    std::lock_guard lock(mu_);
    Record* record = nullptr;
    for (auto& existing : devices_) {
        if (device_key(existing.pluginId, existing.localId) == device_id) {
            record = &existing;
            break;
        }
    }
    if (record == nullptr) {
        result.reason = "unknown_device";
        return result;
    }
    const Capability* capability = find_capability(record->capabilities, capability_id);
    if (capability == nullptr) {
        result.reason = "unknown_capability";
        return result;
    }
    if (capability->kind != "measurement") {
        result.reason = "command_not_measurement";
        return result;
    }
    if (!value_matches(capability->valueType, reading.value)) {
        result.reason = "type_mismatch";
        return result;
    }
    if (std::holds_alternative<double>(reading.value) && !std::isfinite(std::get<double>(reading.value))) {
        result.reason = "non_finite";
        return result;
    }
    if (std::holds_alternative<std::string>(reading.value) && std::get<std::string>(reading.value).size() > 256) {
        result.reason = "value_too_long";
        return result;
    }
    if (capability->valueType == ValueType::Enum) {
        const auto& token = std::get<std::string>(reading.value);
        const bool known = std::any_of(capability->enumStates.begin(), capability->enumStates.end(),
                                        [&](const EnumState& state) { return state.id == token; });
        if (!known) {
            result.reason = "bad_enum";
            return result;
        }
    }

    reading.receivedAtMs = received_at_ms;
    if (Reading* slot = find_reading(record->readings, capability_id)) {
        *slot = reading;
        result.stored = *slot;
    } else {
        record->readings.emplace_back(capability_id, reading);
        result.stored = reading;
    }
    result.accepted = true;
    return result;
}

std::vector<DeviceSnapshot> DeviceRegistry::mark_plugin_disconnected(std::string_view plugin_id) {
    std::lock_guard lock(mu_);
    std::vector<DeviceSnapshot> changed;
    for (auto& record : devices_) {
        if (record.pluginId != plugin_id) {
            continue;
        }
        record.status = DeviceStatus::Disconnected;
        for (auto& entry : record.readings) {
            entry.second.quality = Quality::Stale;
            entry.second.detail = "disconnected";
        }
        changed.push_back(to_snapshot(record));
    }
    return changed;
}

std::vector<std::pair<std::string, Reading>> DeviceRegistry::mark_stale(std::int64_t now_ms) {
    std::lock_guard lock(mu_);
    std::vector<std::pair<std::string, Reading>> changed;
    for (auto& record : devices_) {
        if (record.status == DeviceStatus::Disconnected) {
            continue;
        }
        for (auto& entry : record.readings) {
            const Capability* capability = find_capability(record.capabilities, entry.first);
            if (capability == nullptr) {
                continue;
            }
            if (entry.second.quality != Quality::Good && entry.second.quality != Quality::Uncertain) {
                continue;
            }
            if (now_ms - entry.second.receivedAtMs <= capability->staleAfterMs) {
                continue;
            }
            entry.second.quality = Quality::Stale;
            entry.second.detail = "stale";
            changed.emplace_back(device_key(record.pluginId, record.localId) + "\n" + entry.first, entry.second);
        }
    }
    return changed;
}

std::optional<DeviceSnapshot> DeviceRegistry::find(std::string_view device_id) const {
    std::lock_guard lock(mu_);
    for (const auto& record : devices_) {
        if (device_key(record.pluginId, record.localId) == device_id) {
            return to_snapshot(record);
        }
    }
    return std::nullopt;
}

std::vector<DeviceSnapshot> DeviceRegistry::list() const {
    std::lock_guard lock(mu_);
    std::vector<DeviceSnapshot> snapshots;
    snapshots.reserve(devices_.size());
    for (const auto& record : devices_) {
        snapshots.push_back(to_snapshot(record));
    }
    return snapshots;
}

std::size_t DeviceRegistry::size() const {
    std::lock_guard lock(mu_);
    return devices_.size();
}

}  // namespace ogw
