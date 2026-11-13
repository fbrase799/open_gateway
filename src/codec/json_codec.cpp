#include "ogw/json_codec.hpp"

#include "ogw/clock.hpp"
#include "ogw/version.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ogw {
namespace {

nlohmann::json value_json(const Value& value) {
    return std::visit([](const auto& item) { return nlohmann::json(item); }, value);
}

nlohmann::json reading_json(const Reading& reading) {
    nlohmann::json document = {{"value", value_json(reading.value)},
                               {"quality", std::string(to_string(reading.quality))},
                               {"sourceTimestamp", format_time_utc(reading.sourceTimestampMs)},
                               {"receivedAt", format_time_utc(reading.receivedAtMs)}};
    if (reading.detail) {
        document["detail"] = *reading.detail;
    } else {
        document["detail"] = nullptr;
    }
    return document;
}

const Reading* find_reading(const DeviceSnapshot& device, const std::string& capability_id) {
    for (const auto& entry : device.readings) {
        if (entry.first == capability_id) {
            return &entry.second;
        }
    }
    return nullptr;
}

nlohmann::json capability_json(const DeviceSnapshot& device, const Capability& capability,
                               const TelemetryPipeline& pipeline, std::size_t history_limit) {
    nlohmann::json document = {{"id", capability.id},
                               {"kind", capability.kind},
                               {"valueType", std::string(to_string(capability.valueType))},
                               {"displayName", capability.displayName},
                               {"staleAfterMs", capability.staleAfterMs}};
    if (capability.unit) {
        document["unit"] = *capability.unit;
    }
    if (!capability.presentation.empty()) {
        document["presentation"] = capability.presentation;
    }
    if (capability.rangeMin && capability.rangeMax) {
        document["range"] = {{"min", *capability.rangeMin}, {"max", *capability.rangeMax}};
    }
    if (!capability.enumStates.empty()) {
        document["enumStates"] = nlohmann::json::array();
        for (const auto& state : capability.enumStates) {
            document["enumStates"].push_back({{"id", state.id}, {"label", state.label}, {"tone", state.tone}});
        }
    }
    if (const Reading* current = find_reading(device, capability.id)) {
        document["current"] = reading_json(*current);
    } else {
        document["current"] = nullptr;
    }
    document["history"] = nlohmann::json::array();
    for (const auto& sample : pipeline.recent(device.deviceId, capability.id, history_limit)) {
        document["history"].push_back({{"value", value_json(sample.value)},
                                        {"quality", std::string(to_string(sample.quality))},
                                        {"sourceTimestamp", format_time_utc(sample.sourceTimestampMs)}});
    }
    return document;
}

nlohmann::json device_document(const DeviceSnapshot& device, const TelemetryPipeline& pipeline,
                               std::size_t history_limit) {
    nlohmann::json document = {{"deviceId", device.deviceId},
                               {"pluginId", device.pluginId},
                               {"displayName", device.displayName},
                               {"status", std::string(to_string(device.status))},
                               {"capabilities", nlohmann::json::array()}};
    for (const auto& capability : device.capabilities) {
        document["capabilities"].push_back(capability_json(device, capability, pipeline, history_limit));
    }
    return document;
}

nlohmann::json plugin_document(const PluginInfo& plugin, const DeviceRegistry& registry) {
    std::size_t devices = 0;
    for (const auto& device : registry.list()) {
        if (device.pluginId == plugin.id) {
            ++devices;
        }
    }
    return {{"id", plugin.id},
            {"displayName", plugin.displayName},
            {"version", plugin.version},
            {"file", plugin.file},
            {"state", std::string(to_string(plugin.state))},
            {"error", plugin.error},
            {"deviceCount", devices}};
}

}  // namespace

std::string health_json(std::string_view version, std::uint32_t abi_version, std::int64_t uptime_ms) {
    nlohmann::json document = {{"status", "ok"},
                               {"version", std::string(version)},
                               {"abiVersion", abi_version},
                               {"uptimeMs", uptime_ms}};
    return document.dump();
}

std::string metrics_json(const PipelineCounters& counters, std::uint64_t dropped, std::uint64_t coalesced,
                         std::size_t active_devices, std::size_t active_plugins) {
    nlohmann::json document = {{"readingsAccepted", counters.accepted},
                               {"readingsRejected", counters.rejected},
                               {"telemetryDropped", dropped},
                               {"wsCoalesced", coalesced},
                               {"activeDevices", active_devices},
                               {"activePlugins", active_plugins}};
    return document.dump();
}

std::string devices_json(const std::vector<DeviceSnapshot>& devices, const TelemetryPipeline& pipeline,
                         std::size_t history_limit) {
    nlohmann::json document = {{"devices", nlohmann::json::array()}};
    for (const auto& device : devices) {
        document["devices"].push_back(device_document(device, pipeline, history_limit));
    }
    return document.dump();
}

std::string device_json(const DeviceSnapshot& device, const TelemetryPipeline& pipeline, std::size_t history_limit) {
    return device_document(device, pipeline, history_limit).dump();
}

std::string plugins_json(const std::vector<PluginInfo>& plugins, const DeviceRegistry& registry) {
    nlohmann::json document = {{"plugins", nlohmann::json::array()}};
    for (const auto& plugin : plugins) {
        document["plugins"].push_back(plugin_document(plugin, registry));
    }
    return document.dump();
}

std::string current_json(const std::vector<DeviceSnapshot>& devices) {
    nlohmann::json document = {{"readings", nlohmann::json::array()}};
    for (const auto& device : devices) {
        for (const auto& entry : device.readings) {
            auto reading = reading_json(entry.second);
            reading["deviceId"] = device.deviceId;
            reading["capabilityId"] = entry.first;
            document["readings"].push_back(std::move(reading));
        }
    }
    return document.dump();
}

std::string history_json(const std::vector<HistorySample>& samples) {
    nlohmann::json document = {{"samples", nlohmann::json::array()}};
    for (const auto& sample : samples) {
        document["samples"].push_back({{"value", value_json(sample.value)},
                                        {"quality", std::string(to_string(sample.quality))},
                                        {"sourceTimestamp", format_time_utc(sample.sourceTimestampMs)},
                                        {"receivedAt", format_time_utc(sample.receivedAtMs)}});
    }
    return document.dump();
}

std::string event_json(const RuntimeEvent& event, const DeviceRegistry& registry, const TelemetryPipeline& pipeline) {
    nlohmann::json document;
    switch (event.kind) {
        case RuntimeEventKind::Reading: {
            document["type"] = "reading";
            document["deviceId"] = event.deviceId;
            document["capabilityId"] = event.capabilityId;
            if (auto device = registry.find(event.deviceId)) {
                if (const Reading* reading = find_reading(*device, event.capabilityId)) {
                    document.update(reading_json(*reading));
                }
            }
            break;
        }
        case RuntimeEventKind::DeviceUpsert: {
            document["type"] = "device_upsert";
            if (auto device = registry.find(event.deviceId)) {
                document["device"] = device_document(*device, pipeline, 60);
            } else {
                document["device"] = nullptr;
            }
            break;
        }
        case RuntimeEventKind::DeviceRemoved:
            document = {{"type", "device_removed"}, {"deviceId", event.deviceId}};
            break;
        case RuntimeEventKind::PluginState:
            document["type"] = "plugin_state";
            if (event.plugin) {
                document["plugin"] = {{"id", event.plugin->id},
                                      {"displayName", event.plugin->displayName},
                                      {"version", event.plugin->version},
                                      {"file", event.plugin->file},
                                      {"state", std::string(to_string(event.plugin->state))},
                                      {"error", event.plugin->error}};
            }
            break;
        case RuntimeEventKind::Support:
            if (!event.payload.empty()) {
                return event.payload;
            }
            document["type"] = "support";
            break;
    }
    return document.dump();
}

std::string snapshot_json(const DeviceRegistry& registry, const TelemetryPipeline& pipeline,
                          const std::vector<PluginInfo>& plugins, const std::string& support_object) {
    SystemClock clock;
    nlohmann::json document = {{"type", "snapshot"},
                               {"time", format_time_utc(clock.wall_time_ms())},
                               {"devices", nlohmann::json::array()},
                               {"plugins", nlohmann::json::array()}};
    for (const auto& device : registry.list()) {
        document["devices"].push_back(device_document(device, pipeline, 60));
    }
    for (const auto& plugin : plugins) {
        document["plugins"].push_back(plugin_document(plugin, registry));
    }
    const auto support = nlohmann::json::parse(support_object.empty() ? "{}" : support_object, nullptr, false);
    document["support"] = support.is_discarded() ? nlohmann::json::object() : support;
    return document.dump();
}

std::string command_json(const CommandStatus& status, std::string_view state) {
    nlohmann::json document = {{"ok", status.ok}, {"id", status.id}, {"error", status.error}};
    if (!state.empty()) {
        document["state"] = std::string(state);
    }
    return document.dump();
}

}  // namespace ogw
