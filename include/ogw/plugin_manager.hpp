#pragma once

#include "ogw/clock.hpp"
#include "ogw/pipeline.hpp"
#include "ogw/registry.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ogw {

enum class PluginState { Discovered, Loaded, Active, Inactive, Failed };

struct SettingField {
    std::string name;
    std::string valueType;
    bool hasMin = false;
    bool hasMax = false;
    double min = 0;
    double max = 0;
    std::string value;
};

struct PluginInfo {
    std::string id;
    std::string displayName;
    std::string version;
    std::string file;
    PluginState state = PluginState::Discovered;
    std::string error;
    std::vector<SettingField> settings;
};

struct CommandStatus {
    bool ok = false;
    std::string error;
    std::string id;
};

enum class RuntimeEventKind { Reading, DeviceUpsert, DeviceRemoved, PluginState, Support };

struct RuntimeEvent {
    RuntimeEventKind kind = RuntimeEventKind::Reading;
    std::string deviceId;
    std::string capabilityId;
    std::string pluginId;
    std::optional<PluginInfo> plugin;
    std::string payload;
};

std::string_view to_string(PluginState state);

class PluginManager {
public:
    using Listener = std::function<void(const RuntimeEvent&)>;

    PluginManager(DeviceRegistry& registry, TelemetryPipeline& pipeline, Clock& clock, std::string plugin_dir,
                  std::size_t telemetry_capacity = 4096);
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    void set_listener(Listener listener);
    CommandStatus load_configured(const std::string& file, const std::string& expected_id, const std::string& config_json,
                                 bool activate, std::optional<bool> simulated = std::nullopt);
    CommandStatus scan();
    CommandStatus activate(const std::string& id);
    CommandStatus deactivate(const std::string& id);
    CommandStatus set_export(const std::string& id, bool enabled);
    CommandStatus configure_source(const std::string& id, const std::string& state, const std::string& settings_json);
    CommandStatus invoke(const std::string& device_id, const std::string& capability_id, const std::string& json);
    bool export_enabled(const std::string& id);
    std::string support_json();
    std::vector<PluginInfo> list();
    bool real_device_connected();
    void evaluate_staleness();
    void shutdown();
    std::uint64_t telemetry_dropped() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace ogw
