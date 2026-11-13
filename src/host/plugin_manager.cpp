#include "ogw/plugin_manager.hpp"

#include "ogw/descriptor.hpp"
#include "ogw/log.hpp"
#include "ogw/plugin_abi.h"
#include "ogw/source_policy.hpp"

#include <dlfcn.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <future>
#include <list>
#include <mutex>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

namespace ogw {
namespace {

std::string basename_only(const std::string& file) {
    if (file.empty() || file.find('/') != std::string::npos || file.find('\\') != std::string::npos ||
        file.find("..") != std::string::npos) {
        return {};
    }
    if (file.size() < 4 || file.substr(file.size() - 3) != ".so") {
        return {};
    }
    return file;
}

void copy_error(char* err, std::size_t len, std::string_view message) {
    if (err == nullptr || len == 0) {
        return;
    }
    const std::size_t count = std::min(len - 1, message.size());
    std::memcpy(err, message.data(), count);
    err[count] = '\0';
}

Quality map_quality(int32_t quality) {
    switch (quality) {
        case OGW_QUALITY_GOOD:
            return Quality::Good;
        case OGW_QUALITY_UNCERTAIN:
            return Quality::Uncertain;
        case OGW_QUALITY_BAD:
            return Quality::Bad;
        case OGW_QUALITY_STALE:
            return Quality::Stale;
        default:
            return Quality::Uncertain;
    }
}

}  // namespace

std::string_view to_string(PluginState state) {
    switch (state) {
        case PluginState::Discovered:
            return "discovered";
        case PluginState::Loaded:
            return "loaded";
        case PluginState::Active:
            return "active";
        case PluginState::Inactive:
            return "inactive";
        case PluginState::Failed:
            return "failed";
    }
    return "failed";
}

struct PluginManager::Impl {
    struct LoadedPlugin;

    struct Bridge {
        Impl* impl = nullptr;
        LoadedPlugin* plugin = nullptr;
    };

    struct LoadedPlugin {
        Bridge bridge;
        ogw_host_api api{};
        std::string configuredId;
        std::string id;
        std::string displayName;
        std::string version;
        std::string file;
        std::string canonicalPath;
        std::string configJson;
        std::optional<bool> simulatedOverride;
        bool suspendedForReal = false;
        PluginState state = PluginState::Failed;
        std::string error;
        std::vector<SettingField> settings;
        bool accepting = false;
        void* library = nullptr;
        ogw_plugin* plugin = nullptr;
        ogw_plugin_create_fn create = nullptr;
        ogw_plugin_destroy_fn destroy = nullptr;
    };

    struct TelemetryItem {
        LoadedPlugin* plugin = nullptr;
        std::string localId;
        std::string capabilityId;
        int32_t kind = 0;
        int32_t quality = OGW_QUALITY_GOOD;
        int32_t boolValue = 0;
        double number = 0;
        std::string stringValue;
        std::string detail;
        bool hasDetail = false;
        std::int64_t sourceTimestampMs = 0;
    };

    Impl(DeviceRegistry& registry, TelemetryPipeline& pipeline, Clock& clock, std::string plugin_dir,
         std::size_t telemetry_capacity)
        : registry_(registry),
          pipeline_(pipeline),
          clock_(clock),
          plugin_dir_(std::move(plugin_dir)),
          telemetry_capacity_(telemetry_capacity == 0 ? 1 : telemetry_capacity) {
        std::filesystem::create_directories(plugin_dir_);
        worker_ = std::thread([this] { loop(); });
    }

    PluginInfo info_of(const LoadedPlugin& plugin) const {
        PluginInfo info;
        info.id = plugin.id;
        info.displayName = plugin.displayName.empty() ? plugin.id : plugin.displayName;
        info.version = plugin.version;
        info.file = plugin.file;
        info.state = plugin.state;
        info.error = plugin.error;
        info.settings = plugin.settings;
        return info;
    }

    void emit(RuntimeEvent event) {
        Listener listener;
        {
            std::lock_guard lock(listener_mu_);
            listener = listener_;
        }
        if (!listener) {
            return;
        }
        try {
            listener(event);
        } catch (const std::exception& ex) {
            log_message(LogLevel::Error, "plugins", ex.what());
        }
    }

    void emit_plugin(const LoadedPlugin& plugin) {
        RuntimeEvent event;
        event.kind = RuntimeEventKind::PluginState;
        event.pluginId = plugin.id;
        event.plugin = info_of(plugin);
        emit(std::move(event));
    }

    std::string support_object() const {
        nlohmann::json sources = nlohmann::json::array();
        for (const auto& plugin : plugins_) {
            nlohmann::json fields = nlohmann::json::array();
            for (const auto& field : plugin->settings) {
                nlohmann::json item = {{"name", field.name}, {"valueType", field.valueType}, {"value", field.value}};
                if (field.hasMin) {
                    item["min"] = field.min;
                }
                if (field.hasMax) {
                    item["max"] = field.max;
                }
                fields.push_back(std::move(item));
            }
            sources.push_back({{"id", plugin->id},
                               {"name", plugin->displayName.empty() ? plugin->id : plugin->displayName},
                               {"version", plugin->version},
                               {"state", std::string(to_string(plugin->state))},
                               {"error", plugin->error},
                               {"simulated", resolve_simulated(plugin->id, plugin->simulatedOverride)},
                               {"settings", std::move(fields)}});
        }
        nlohmann::json document = {
            {"exports",
             nlohmann::json::array({{{"id", "rest"}, {"enabled", rest_enabled_}, {"mutable", true}, {"settings", nlohmann::json::array()}},
                                    {{"id", "websocket"},
                                     {"enabled", websocket_enabled_},
                                     {"mutable", true},
                                     {"settings", nlohmann::json::array()}}})},
            {"sources", std::move(sources)}};
        return document.dump();
    }

    void note_catalog() {
        RuntimeEvent event;
        event.kind = RuntimeEventKind::Support;
        event.payload = std::string("{\"type\":\"support\",\"support\":") + support_object() + "}";
        emit(std::move(event));
    }

    void read_settings(LoadedPlugin& plugin) {
        plugin.settings.clear();
        if (plugin.plugin == nullptr || plugin.plugin->vtable == nullptr) {
            return;
        }
        const auto* vtable = plugin.plugin->vtable;
        const std::size_t describe_end = offsetof(ogw_plugin_vtable, describe_settings) + sizeof(vtable->describe_settings);
        if (vtable->size < describe_end || vtable->describe_settings == nullptr) {
            return;
        }
        char buffer[4096] = {};
        if (vtable->describe_settings(plugin.plugin, buffer, sizeof(buffer)) != OGW_OK) {
            return;
        }
        const auto schema = nlohmann::json::parse(buffer, nullptr, false);
        if (!schema.is_array()) {
            return;
        }
        const auto config = nlohmann::json::parse(plugin.configJson, nullptr, false);
        for (const auto& item : schema) {
            if (!item.is_object() || !item.contains("name") || !item.contains("valueType")) {
                continue;
            }
            SettingField field;
            field.name = item.at("name").get<std::string>();
            field.valueType = item.at("valueType").get<std::string>();
            if (item.contains("min")) {
                field.hasMin = true;
                field.min = item.at("min").get<double>();
            }
            if (item.contains("max")) {
                field.hasMax = true;
                field.max = item.at("max").get<double>();
            }
            if (config.is_object() && config.contains(field.name)) {
                const auto& current = config.at(field.name);
                field.value = current.is_string() ? current.get<std::string>() : current.dump();
            }
            plugin.settings.push_back(std::move(field));
        }
    }

    bool setting_accepted(const SettingField& field, const nlohmann::json& value) const {
        if (field.valueType == "number" && value.is_number()) {
            const double number = value.get<double>();
            if (field.hasMin && number < field.min) {
                return false;
            }
            if (field.hasMax && number > field.max) {
                return false;
            }
            return true;
        }
        if (field.valueType == "bool" && value.is_boolean()) {
            return true;
        }
        return field.valueType == "string" && value.is_string();
    }

    void fail(LoadedPlugin& plugin, std::string error) {
        plugin.state = PluginState::Failed;
        plugin.error = std::move(error);
        log_message(LogLevel::Warn, "plugins", plugin.error, {{"pluginId", plugin.id}, {"file", plugin.file}});
        emit_plugin(plugin);
        if (find(plugin.id) != nullptr) {
            note_catalog();
        }
    }

    int call_stop(LoadedPlugin& plugin) {
        if (plugin.plugin == nullptr || plugin.plugin->vtable == nullptr || plugin.plugin->vtable->stop == nullptr) {
            return OGW_OK;
        }
        char err[256] = {};
        try {
            return plugin.plugin->vtable->stop(plugin.plugin, err, sizeof(err));
        } catch (const std::exception& ex) {
            copy_error(err, sizeof(err), ex.what());
            return OGW_ERR_STATE;
        }
    }

    void destroy_object(LoadedPlugin& plugin) {
        if (plugin.plugin != nullptr && plugin.destroy != nullptr) {
            plugin.destroy(plugin.plugin);
        }
        plugin.plugin = nullptr;
    }

    void drain_telemetry() {
        std::deque<TelemetryItem> batch;
        {
            std::lock_guard lock(mu_);
            batch.swap(telemetry_);
        }
        for (auto& item : batch) {
            handle_telemetry(item);
        }
    }

    void handle_telemetry(const TelemetryItem& item) {
        if (item.plugin == nullptr || item.plugin->id.empty()) {
            return;
        }
        Reading reading;
        reading.quality = map_quality(item.quality);
        reading.sourceTimestampMs = item.sourceTimestampMs;
        if (item.hasDetail) {
            reading.detail = item.detail;
        }
        switch (item.kind) {
            case OGW_VALUE_NUMBER:
                reading.value = item.number;
                break;
            case OGW_VALUE_BOOL:
                reading.value = item.boolValue != 0;
                break;
            case OGW_VALUE_STRING:
                reading.value = item.stringValue;
                break;
            default:
                rejected_note("unsupported value kind");
                return;
        }
        const std::string device_id = device_key(item.plugin->id, item.localId);
        auto result = pipeline_.publish(device_id, item.capabilityId, std::move(reading));
        if (!result.accepted) {
            log_message(LogLevel::Debug, "telemetry", result.reason,
                        {{"pluginId", item.plugin->id}, {"deviceId", device_id}, {"capabilityId", item.capabilityId}});
            return;
        }
        RuntimeEvent event;
        event.kind = RuntimeEventKind::Reading;
        event.deviceId = device_id;
        event.capabilityId = item.capabilityId;
        event.pluginId = item.plugin->id;
        emit(std::move(event));
    }

    void rejected_note(const char* reason) { log_message(LogLevel::Debug, "telemetry", reason); }

    void post(std::function<void()> job) {
        {
            std::lock_guard lock(mu_);
            if (exit_) {
                return;
            }
            control_.push_back(std::move(job));
        }
        cv_.notify_one();
    }

    int32_t apply_register(LoadedPlugin* plugin, const std::string& json, bool require_full) {
        if (plugin == nullptr || plugin->id.empty() || !plugin->accepting) {
            return OGW_ERR_STATE;
        }
        auto parsed = parse_device_json(json, require_full);
        if (!parsed.ok) {
            log_message(LogLevel::Warn, "plugins", parsed.error, {{"pluginId", plugin->id}});
            return OGW_ERR_REJECTED;
        }
        auto mutation = registry_.upsert(plugin->id, parsed.draft, require_full);
        if (!mutation.ok) {
            log_message(LogLevel::Warn, "plugins", mutation.error, {{"pluginId", plugin->id}});
            return OGW_ERR_REJECTED;
        }
        RuntimeEvent event;
        event.kind = RuntimeEventKind::DeviceUpsert;
        event.deviceId = mutation.deviceId;
        event.pluginId = plugin->id;
        emit(std::move(event));
        if (!reconciling_) {
            reconcile_sources();
        }
        return OGW_OK;
    }

    int32_t apply_remove(LoadedPlugin* plugin, const std::string& local_id) {
        if (plugin == nullptr || !plugin->accepting) {
            return OGW_ERR_STATE;
        }
        auto mutation = registry_.remove(plugin->id, local_id);
        if (!mutation.ok) {
            return OGW_ERR_REJECTED;
        }
        RuntimeEvent event;
        event.kind = RuntimeEventKind::DeviceRemoved;
        event.deviceId = mutation.deviceId;
        event.pluginId = plugin->id;
        emit(event);
        return OGW_OK;
    }

    void enqueue_telemetry(TelemetryItem item) {
        {
            std::lock_guard lock(mu_);
            if (telemetry_.size() >= telemetry_capacity_) {
                telemetry_.pop_front();
                dropped_.fetch_add(1, std::memory_order_relaxed);
            }
            telemetry_.push_back(std::move(item));
        }
        cv_.notify_one();
    }

    template <typename F>
    auto on_consumer(F&& fn) -> decltype(fn()) {
        using Result = decltype(fn());
        if (!worker_.joinable() || std::this_thread::get_id() == consumer_id_) {
            return fn();
        }
        std::promise<Result> promise;
        auto future = promise.get_future();
        {
            std::lock_guard lock(mu_);
            if (exit_) {
                if constexpr (std::is_same_v<Result, void>) {
                    return;
                } else {
                    return Result{};
                }
            }
            control_.push_back([&] {
                try {
                    if constexpr (std::is_same_v<Result, void>) {
                        fn();
                        promise.set_value();
                    } else {
                        promise.set_value(fn());
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            });
        }
        cv_.notify_one();
        return future.get();
    }

    void loop() {
        consumer_id_ = std::this_thread::get_id();
        while (true) {
            std::function<void()> job;
            bool has_telemetry = false;
            {
                std::unique_lock lock(mu_);
                cv_.wait(lock, [&] { return exit_ || !control_.empty() || !telemetry_.empty(); });
                if (!control_.empty()) {
                    job = std::move(control_.front());
                    control_.pop_front();
                } else if (!telemetry_.empty()) {
                    has_telemetry = true;
                } else if (exit_) {
                    return;
                }
            }
            if (job) {
                job();
            }
            if (has_telemetry || job) {
                drain_telemetry();
            }
        }
    }

    CommandStatus load_path(const std::filesystem::path& path, const std::string& file, const std::string& config_json,
                            const std::string& expected_id, bool activate, std::optional<bool> simulated = std::nullopt) {
        return on_consumer([&] { return load_path_impl(path, file, config_json, expected_id, activate, simulated); });
    }

    CommandStatus load_path_impl(const std::filesystem::path& path, const std::string& file,
                                 const std::string& config_json, const std::string& expected_id, bool activate,
                                 std::optional<bool> simulated = std::nullopt) {
        std::error_code error;
        const auto canonical = std::filesystem::weakly_canonical(path, error);
        if (error) {
            CommandStatus status;
            status.error = "plugin file is not available";
            return status;
        }
        for (const auto& existing : plugins_) {
            if (existing->canonicalPath == canonical.string()) {
                if (activate) {
                    return activate_impl(*existing);
                }
                CommandStatus status;
                status.ok = existing->state != PluginState::Failed;
                status.id = existing->id;
                status.error = existing->error;
                return status;
            }
        }

        auto loaded = std::make_unique<LoadedPlugin>();
        loaded->simulatedOverride = simulated;
        loaded->bridge.impl = this;
        loaded->bridge.plugin = loaded.get();
        loaded->file = file.empty() ? canonical.filename().string() : file;
        loaded->canonicalPath = canonical.string();
        loaded->configJson = config_json.empty() ? "{}" : config_json;
        loaded->configuredId = expected_id;
        loaded->api.abi_version = OGW_PLUGIN_ABI_VERSION;
        loaded->api.size = sizeof(ogw_host_api);
        loaded->api.user_data = &loaded->bridge;
        loaded->api.log = &host_log;
        loaded->api.register_device = &host_register;
        loaded->api.update_device = &host_update;
        loaded->api.remove_device = &host_remove;
        loaded->api.publish_reading = &host_publish;
        loaded->api.wall_time_ms = &host_time;

        loaded->library = dlopen(canonical.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (loaded->library == nullptr) {
            loaded->id = canonical.stem().string();
            const char* message = dlerror();
            fail(*loaded, message != nullptr ? message : "dlopen failed");
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }
        loaded->create = reinterpret_cast<ogw_plugin_create_fn>(dlsym(loaded->library, "ogw_plugin_create"));
        loaded->destroy = reinterpret_cast<ogw_plugin_destroy_fn>(dlsym(loaded->library, "ogw_plugin_destroy"));
        if (loaded->create == nullptr || loaded->destroy == nullptr) {
            loaded->id = canonical.stem().string();
            fail(*loaded, "plugin is missing ogw_plugin_create or ogw_plugin_destroy");
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }

        loaded->plugin = loaded->create(&loaded->api, loaded->configJson.c_str());
        if (loaded->plugin == nullptr || loaded->plugin->vtable == nullptr ||
            loaded->plugin->vtable->size < offsetof(ogw_plugin_vtable, describe_settings) ||
            loaded->plugin->vtable->get_info == nullptr || loaded->plugin->vtable->init == nullptr ||
            loaded->plugin->vtable->start == nullptr || loaded->plugin->vtable->stop == nullptr) {
            loaded->id = canonical.stem().string();
            fail(*loaded, "plugin create returned an incomplete object");
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }

        ogw_plugin_info info{};
        char err[256] = {};
        const int info_rc = loaded->plugin->vtable->get_info(loaded->plugin, &info);
        loaded->id = info.id == nullptr ? canonical.stem().string() : info.id;
        loaded->displayName = info.name == nullptr ? loaded->id : info.name;
        loaded->version = info.version == nullptr ? "" : info.version;
        if (info_rc != OGW_OK) {
            fail(*loaded, "plugin get_info failed");
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }
        if (info.abi_version != OGW_PLUGIN_ABI_VERSION) {
            fail(*loaded, "abi version mismatch");
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }
        if (!expected_id.empty() && loaded->id != expected_id) {
            fail(*loaded, "plugin id does not match configuration");
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }
        for (const auto& existing : plugins_) {
            if (existing->id == loaded->id) {
                fail(*loaded, "plugin id is already loaded");
                plugins_.push_back(std::move(loaded));
                note_catalog();
                CommandStatus status;
                status.id = plugins_.back()->id;
                status.error = plugins_.back()->error;
                return status;
            }
        }

        int init_rc = OGW_OK;
        loaded->accepting = true;
        try {
            init_rc = loaded->plugin->vtable->init(loaded->plugin, err, sizeof(err));
        } catch (const std::exception& ex) {
            init_rc = OGW_ERR_INIT;
            copy_error(err, sizeof(err), ex.what());
        }
        if (init_rc != OGW_OK) {
            loaded->accepting = false;
            fail(*loaded, err[0] == '\0' ? "plugin init failed" : err);
            plugins_.push_back(std::move(loaded));
            note_catalog();
            CommandStatus status;
            status.id = plugins_.back()->id;
            status.error = plugins_.back()->error;
            return status;
        }

        loaded->state = PluginState::Loaded;
        loaded->error.clear();
        read_settings(*loaded);
        log_message(LogLevel::Info, "plugins", "loaded", {{"pluginId", loaded->id}, {"file", loaded->file}});
        plugins_.push_back(std::move(loaded));
        emit_plugin(*plugins_.back());
        note_catalog();
        if (activate) {
            return activate_impl(*plugins_.back());
        }
        CommandStatus status;
        status.ok = true;
        status.id = plugins_.back()->id;
        return status;
    }

    bool real_device_connected() const {
        for (const auto& device : registry_.list()) {
            if (device.status != DeviceStatus::Connected) {
                continue;
            }
            for (const auto& plugin : plugins_) {
                if (plugin->id == device.pluginId && plugin->state == PluginState::Active &&
                    !resolve_simulated(plugin->id, plugin->simulatedOverride)) {
                    return true;
                }
            }
        }
        return false;
    }

    void reconcile_sources() {
        if (reconciling_) {
            return;
        }
        reconciling_ = true;
        struct Guard {
            bool& flag;
            ~Guard() { flag = false; }
        } guard{reconciling_};
        if (real_device_connected()) {
            for (auto& plugin : plugins_) {
                if (!resolve_simulated(plugin->id, plugin->simulatedOverride) || plugin->state != PluginState::Active) {
                    continue;
                }
                plugin->suspendedForReal = true;
                deactivate_impl(*plugin);
            }
            return;
        }
        for (auto& plugin : plugins_) {
            if (!plugin->suspendedForReal) {
                continue;
            }
            plugin->suspendedForReal = false;
            if (plugin->state == PluginState::Inactive || plugin->state == PluginState::Loaded) {
                activate_impl(*plugin);
            }
        }
    }

    CommandStatus activate_impl(LoadedPlugin& plugin) {
        if (plugin.state == PluginState::Active) {
            CommandStatus status;
            status.ok = true;
            status.id = plugin.id;
            return status;
        }
        if (plugin.state != PluginState::Loaded && plugin.state != PluginState::Inactive) {
            CommandStatus status;
            status.id = plugin.id;
            status.error = plugin.error.empty() ? "plugin cannot be activated" : plugin.error;
            return status;
        }
        char err[256] = {};
        int rc = OGW_OK;
        plugin.accepting = true;
        try {
            rc = plugin.plugin->vtable->start(plugin.plugin, err, sizeof(err));
        } catch (const std::exception& ex) {
            rc = OGW_ERR_INIT;
            copy_error(err, sizeof(err), ex.what());
        }
        drain_telemetry();
        if (rc == OGW_ERR_STATE) {
            plugin.accepting = false;
            call_stop(plugin);
            drain_telemetry();
            plugin.state = PluginState::Inactive;
            plugin.error.clear();
            log_message(LogLevel::Info, "plugins", "sensor not present", {{"pluginId", plugin.id}});
            emit_plugin(plugin);
            note_catalog();
            CommandStatus status;
            status.id = plugin.id;
            status.error = "not_present";
            return status;
        }
        if (rc != OGW_OK) {
            plugin.accepting = false;
            call_stop(plugin);
            drain_telemetry();
            fail(plugin, err[0] == '\0' ? "plugin start failed" : err);
            CommandStatus status;
            status.id = plugin.id;
            status.error = plugin.error;
            return status;
        }
        plugin.state = PluginState::Active;
        plugin.error.clear();
        log_message(LogLevel::Info, "plugins", "activated", {{"pluginId", plugin.id}});
        emit_plugin(plugin);
        note_catalog();
        if (!reconciling_) {
            reconcile_sources();
        }
        CommandStatus status;
        status.ok = true;
        status.id = plugin.id;
        return status;
    }

    CommandStatus deactivate_impl(LoadedPlugin& plugin) {
        if (plugin.state == PluginState::Inactive || plugin.state == PluginState::Loaded) {
            CommandStatus status;
            status.ok = true;
            status.id = plugin.id;
            return status;
        }
        if (plugin.state != PluginState::Active) {
            CommandStatus status;
            status.id = plugin.id;
            status.error = "plugin is not active";
            return status;
        }
        plugin.accepting = false;
        if (!reconciling_) {
            plugin.suspendedForReal = false;
        }
        call_stop(plugin);
        drain_telemetry();
        for (const auto& snapshot : registry_.mark_plugin_disconnected(plugin.id)) {
            RuntimeEvent event;
            event.kind = RuntimeEventKind::DeviceUpsert;
            event.deviceId = snapshot.deviceId;
            event.pluginId = plugin.id;
            emit(event);
        }
        plugin.state = PluginState::Inactive;
        log_message(LogLevel::Info, "plugins", "deactivated", {{"pluginId", plugin.id}});
        emit_plugin(plugin);
        note_catalog();
        if (!reconciling_) {
            reconcile_sources();
        }
        CommandStatus status;
        status.ok = true;
        status.id = plugin.id;
        return status;
    }

    LoadedPlugin* find(const std::string& id) {
        for (auto& plugin : plugins_) {
            if (plugin->id == id) {
                return plugin.get();
            }
        }
        return nullptr;
    }

    static void host_log(void* user, int32_t level, const char* component, const char* message) {
        auto* bridge = static_cast<Bridge*>(user);
        LogLevel mapped = LogLevel::Info;
        if (level <= OGW_LOG_ERROR) {
            mapped = LogLevel::Error;
        } else if (level == OGW_LOG_WARN) {
            mapped = LogLevel::Warn;
        } else if (level >= OGW_LOG_DEBUG) {
            mapped = LogLevel::Debug;
        }
        log_message(mapped, component != nullptr ? component : "plugin", message != nullptr ? message : "",
                    {{"pluginId", bridge->plugin->id}});
    }

    static int32_t host_register(void* user, const char* json) {
        auto* bridge = static_cast<Bridge*>(user);
        if (json == nullptr) {
            return OGW_ERR_BAD_ARG;
        }
        const std::string copy(json);
        if (std::this_thread::get_id() == bridge->impl->consumer_id_) {
            return bridge->impl->apply_register(bridge->plugin, copy, true);
        }
        bridge->impl->post([bridge, copy] { bridge->impl->apply_register(bridge->plugin, copy, true); });
        return OGW_OK;
    }

    static int32_t host_update(void* user, const char* json) {
        auto* bridge = static_cast<Bridge*>(user);
        if (json == nullptr) {
            return OGW_ERR_BAD_ARG;
        }
        const std::string copy(json);
        if (std::this_thread::get_id() == bridge->impl->consumer_id_) {
            return bridge->impl->apply_register(bridge->plugin, copy, false);
        }
        bridge->impl->post([bridge, copy] { bridge->impl->apply_register(bridge->plugin, copy, false); });
        return OGW_OK;
    }

    static int32_t host_remove(void* user, const char* local_id) {
        auto* bridge = static_cast<Bridge*>(user);
        if (local_id == nullptr) {
            return OGW_ERR_BAD_ARG;
        }
        const std::string copy(local_id);
        if (std::this_thread::get_id() == bridge->impl->consumer_id_) {
            return bridge->impl->apply_remove(bridge->plugin, copy);
        }
        bridge->impl->post([bridge, copy] { bridge->impl->apply_remove(bridge->plugin, copy); });
        return OGW_OK;
    }

    static int32_t host_publish(void* user, const char* local_device_id, const char* capability_id,
                                const ogw_reading* reading) {
        auto* bridge = static_cast<Bridge*>(user);
        if (local_device_id == nullptr || capability_id == nullptr || reading == nullptr) {
            return OGW_ERR_BAD_ARG;
        }
        TelemetryItem item;
        item.plugin = bridge->plugin;
        item.localId = local_device_id;
        item.capabilityId = capability_id;
        item.kind = reading->kind;
        item.quality = reading->quality;
        item.boolValue = reading->bool_value;
        item.number = reading->number;
        if (reading->string_value != nullptr) {
            item.stringValue = reading->string_value;
        }
        if (reading->detail != nullptr) {
            item.detail = reading->detail;
            item.hasDetail = true;
        }
        item.sourceTimestampMs = reading->source_timestamp_ms;
        bridge->impl->enqueue_telemetry(std::move(item));
        return OGW_OK;
    }

    static int64_t host_time(void* user) {
        auto* bridge = static_cast<Bridge*>(user);
        return bridge->impl->clock_.wall_time_ms();
    }

    DeviceRegistry& registry_;
    TelemetryPipeline& pipeline_;
    Clock& clock_;
    std::string plugin_dir_;
    std::size_t telemetry_capacity_;
    std::list<std::unique_ptr<LoadedPlugin>> plugins_;
    bool reconciling_ = false;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> control_;
    std::deque<TelemetryItem> telemetry_;
    std::thread worker_;
    std::thread::id consumer_id_{};
    bool exit_ = false;
    std::atomic<bool> shutdown_{false};
    std::atomic<std::uint64_t> dropped_{0};
    std::mutex listener_mu_;
    Listener listener_;
    bool rest_enabled_ = true;
    bool websocket_enabled_ = true;

    bool recreate(LoadedPlugin& plugin, std::string& error) {
        if (plugin.state == PluginState::Active) {
            deactivate_impl(plugin);
        }
        plugin.accepting = false;
        destroy_object(plugin);
        if (plugin.create == nullptr) {
            error = "plugin cannot be reconfigured";
            fail(plugin, error);
            return false;
        }
        plugin.plugin = plugin.create(&plugin.api, plugin.configJson.c_str());
        if (plugin.plugin == nullptr || plugin.plugin->vtable == nullptr || plugin.plugin->vtable->init == nullptr) {
            error = "plugin create failed";
            fail(plugin, error);
            return false;
        }
        char err[256] = {};
        plugin.accepting = true;
        const int rc = plugin.plugin->vtable->init(plugin.plugin, err, sizeof(err));
        if (rc != OGW_OK) {
            plugin.accepting = false;
            error = err[0] == '\0' ? "plugin init failed" : err;
            fail(plugin, error);
            return false;
        }
        plugin.state = PluginState::Loaded;
        plugin.error.clear();
        read_settings(plugin);
        return true;
    }
};

PluginManager::PluginManager(DeviceRegistry& registry, TelemetryPipeline& pipeline, Clock& clock,
                             std::string plugin_dir, std::size_t telemetry_capacity)
    : impl_(std::make_unique<Impl>(registry, pipeline, clock, std::move(plugin_dir), telemetry_capacity)) {}

PluginManager::~PluginManager() { shutdown(); }

void PluginManager::set_listener(Listener listener) {
    std::lock_guard lock(impl_->listener_mu_);
    impl_->listener_ = std::move(listener);
}

CommandStatus PluginManager::load_configured(const std::string& file, const std::string& expected_id,
                                              const std::string& config_json, bool activate, std::optional<bool> simulated) {
    const std::string base = basename_only(file);
    if (base.empty()) {
        CommandStatus status;
        status.error = "plugin file must be a basename ending in .so";
        return status;
    }
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(impl_->plugin_dir_, error);
    if (error) {
        CommandStatus status;
        status.error = "plugin directory is not available";
        return status;
    }
    const auto candidate = std::filesystem::weakly_canonical(root / base, error);
    if (error) {
        CommandStatus status;
        status.error = "plugin file was not found";
        return status;
    }
    const std::string root_text = root.string();
    const std::string candidate_text = candidate.string();
    if (candidate_text != root_text && candidate_text.rfind(root_text + "/", 0) != 0) {
        CommandStatus status;
        status.error = "plugin path escapes the plugin directory";
        return status;
    }
    return impl_->load_path(candidate, base, config_json, expected_id, activate, simulated);
}

CommandStatus PluginManager::scan() {
    return impl_->on_consumer([&] {
        CommandStatus status;
        status.ok = true;
        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(impl_->plugin_dir_, error);
        if (error) {
            status.ok = false;
            status.error = "plugin directory is not available";
            return status;
        }
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto name = entry.path().filename().string();
            if (basename_only(name).empty()) {
                continue;
            }
            impl_->load_path_impl(entry.path(), name, "{}", "", false);
        }
        return status;
    });
}

CommandStatus PluginManager::activate(const std::string& id) {
    return impl_->on_consumer([&] {
        auto* plugin = impl_->find(id);
        if (plugin == nullptr) {
            CommandStatus status;
            status.error = "unknown plugin";
            return status;
        }
        return impl_->activate_impl(*plugin);
    });
}

CommandStatus PluginManager::deactivate(const std::string& id) {
    return impl_->on_consumer([&] {
        auto* plugin = impl_->find(id);
        if (plugin == nullptr) {
            CommandStatus status;
            status.error = "unknown plugin";
            return status;
        }
        return impl_->deactivate_impl(*plugin);
    });
}

CommandStatus PluginManager::set_export(const std::string& id, bool enabled) {
    return impl_->on_consumer([&] {
        CommandStatus status;
        status.id = id;
        if (id == "rest") {
            impl_->rest_enabled_ = enabled;
        } else if (id == "websocket") {
            impl_->websocket_enabled_ = enabled;
        } else {
            status.error = "not_advertised";
            return status;
        }
        status.ok = true;
        impl_->note_catalog();
        return status;
    });
}

CommandStatus PluginManager::configure_source(const std::string& id, const std::string& state,
                                              const std::string& settings_json) {
    return impl_->on_consumer([&] {
        CommandStatus status;
        status.id = id;
        auto* plugin = impl_->find(id);
        if (plugin == nullptr) {
            status.error = "not_advertised";
            return status;
        }
        if (state != "active" && state != "inactive") {
            status.error = "invalid_setting";
            return status;
        }
        const auto settings = nlohmann::json::parse(settings_json.empty() ? "{}" : settings_json, nullptr, false);
        if (settings.is_discarded() || !settings.is_object()) {
            status.error = "invalid_setting";
            return status;
        }
        if (!settings.empty()) {
            auto config = nlohmann::json::parse(plugin->configJson, nullptr, false);
            if (config.is_discarded() || !config.is_object()) {
                config = nlohmann::json::object();
            }
            for (const auto& [key, value] : settings.items()) {
                const SettingField* field = nullptr;
                for (const auto& candidate : plugin->settings) {
                    if (candidate.name == key) {
                        field = &candidate;
                        break;
                    }
                }
                if (field == nullptr || !impl_->setting_accepted(*field, value)) {
                    status.error = "invalid_setting";
                    return status;
                }
                config[key] = value;
            }
            plugin->configJson = config.dump();
            std::string error;
            if (!impl_->recreate(*plugin, error)) {
                status.error = error.empty() ? "invalid_setting" : error;
                return status;
            }
            impl_->note_catalog();
        }
        if (state == "active") {
            return impl_->activate_impl(*plugin);
        }
        return impl_->deactivate_impl(*plugin);
    });
}

bool PluginManager::export_enabled(const std::string& id) {
    return impl_->on_consumer([&] {
        if (id == "rest") {
            return impl_->rest_enabled_;
        }
        if (id == "websocket") {
            return impl_->websocket_enabled_;
        }
        return false;
    });
}

std::string PluginManager::support_json() {
    return impl_->on_consumer([&] { return impl_->support_object(); });
}

std::vector<PluginInfo> PluginManager::list() {
    return impl_->on_consumer([&] {
        std::vector<PluginInfo> infos;
        infos.reserve(impl_->plugins_.size());
        for (const auto& plugin : impl_->plugins_) {
            infos.push_back(impl_->info_of(*plugin));
        }
        return infos;
    });
}

bool PluginManager::real_device_connected() {
    return impl_->on_consumer([&] { return impl_->real_device_connected(); });
}

void PluginManager::evaluate_staleness() {
    if (impl_->shutdown_.load()) {
        return;
    }
    std::lock_guard lock(impl_->mu_);
    if (impl_->exit_) {
        return;
    }
    impl_->control_.push_back([impl = impl_.get()] {
        for (const auto& update : impl->pipeline_.evaluate_staleness()) {
            RuntimeEvent event;
            event.kind = RuntimeEventKind::Reading;
            event.deviceId = update.deviceId;
            event.capabilityId = update.capabilityId;
            impl->emit(std::move(event));
        }
    });
    impl_->cv_.notify_one();
}

void PluginManager::shutdown() {
    if (!impl_ || impl_->shutdown_.exchange(true)) {
        return;
    }
    impl_->on_consumer([&] {
        for (auto& plugin : impl_->plugins_) {
            if (plugin->state == PluginState::Active) {
                impl_->call_stop(*plugin);
            }
        }
        impl_->drain_telemetry();
        for (auto& plugin : impl_->plugins_) {
            impl_->destroy_object(*plugin);
        }
    });
    {
        std::lock_guard listener_lock(impl_->listener_mu_);
        impl_->listener_ = nullptr;
    }
    {
        std::lock_guard lock(impl_->mu_);
        impl_->exit_ = true;
    }
    impl_->cv_.notify_one();
    if (impl_->worker_.joinable()) {
        impl_->worker_.join();
    }
}

std::uint64_t PluginManager::telemetry_dropped() const { return impl_->dropped_.load(std::memory_order_relaxed); }

}  // namespace ogw
