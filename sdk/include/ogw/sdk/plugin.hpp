#pragma once

#include "ogw/plugin_abi.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ogw::sdk {

struct Info {
    std::string id;
    std::string name;
    std::string version;
};

class SensorAbsent : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class HostServices {
public:
    HostServices() = default;
    explicit HostServices(const ogw_host_api* api) : api_(api) {}

    void log(int level, std::string_view message) const {
        if (api_ == nullptr || api_->log == nullptr) {
            return;
        }
        const std::string text(message);
        api_->log(api_->user_data, level, "plugin", text.c_str());
    }

    void register_device(std::string_view json) const { call_json(api_->register_device, json, "register_device"); }

    void update_device(std::string_view json) const { call_json(api_->update_device, json, "update_device"); }

    void remove_device(std::string_view local_id) const {
        if (api_ == nullptr || api_->remove_device == nullptr) {
            throw std::runtime_error("host api is unavailable");
        }
        const std::string id(local_id);
        const int32_t rc = api_->remove_device(api_->user_data, id.c_str());
        if (rc != OGW_OK) {
            throw std::runtime_error("remove_device failed");
        }
    }

    void publish(std::string_view local_device_id, std::string_view capability_id, const ogw_reading& reading) const {
        if (api_ == nullptr || api_->publish_reading == nullptr) {
            throw std::runtime_error("host api is unavailable");
        }
        const std::string device(local_device_id);
        const std::string capability(capability_id);
        const int32_t rc = api_->publish_reading(api_->user_data, device.c_str(), capability.c_str(), &reading);
        if (rc != OGW_OK) {
            throw std::runtime_error("publish_reading failed");
        }
    }

    std::int64_t wall_time_ms() const {
        if (api_ == nullptr || api_->wall_time_ms == nullptr) {
            return 0;
        }
        return api_->wall_time_ms(api_->user_data);
    }

private:
    void call_json(int32_t (*fn)(void*, const char*), std::string_view json, const char* what) const {
        if (api_ == nullptr || fn == nullptr) {
            throw std::runtime_error(std::string(what) + " is unavailable");
        }
        const std::string text(json);
        const int32_t rc = fn(api_->user_data, text.c_str());
        if (rc != OGW_OK) {
            throw std::runtime_error(std::string(what) + " was rejected");
        }
    }

    const ogw_host_api* api_ = nullptr;
};

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual Info info() const = 0;
    virtual void init(std::string_view config_json) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::string settings_schema() const { return "[]"; }

    void bind(const ogw_host_api* api) { host_ = HostServices(api); }

protected:
    HostServices& host() { return host_; }

private:
    HostServices host_;
};

inline ogw_reading number_reading(double value, int quality, std::int64_t timestamp_ms, const char* detail) {
    ogw_reading reading{};
    reading.kind = OGW_VALUE_NUMBER;
    reading.quality = quality;
    reading.number = value;
    reading.source_timestamp_ms = timestamp_ms;
    reading.detail = detail;
    return reading;
}

inline ogw_reading string_reading(const char* value, int quality, std::int64_t timestamp_ms, const char* detail) {
    ogw_reading reading{};
    reading.kind = OGW_VALUE_STRING;
    reading.quality = quality;
    reading.string_value = value;
    reading.source_timestamp_ms = timestamp_ms;
    reading.detail = detail;
    return reading;
}

namespace detail {

inline void copy_error(char* err, std::size_t len, std::string_view message) {
    if (err == nullptr || len == 0) {
        return;
    }
    const std::size_t count = std::min(len - 1, message.size());
    std::memcpy(err, message.data(), count);
    err[count] = '\0';
}

template <typename T>
struct Wrapper {
    ogw_plugin base{};
    std::unique_ptr<T> impl;
    std::string config;
    std::string id;
    std::string name;
    std::string version;
};

template <typename T>
int32_t get_info(ogw_plugin* plugin, ogw_plugin_info* info) {
    auto* wrapper = reinterpret_cast<Wrapper<T>*>(plugin);
    try {
        Info value = wrapper->impl->info();
        wrapper->id = std::move(value.id);
        wrapper->name = std::move(value.name);
        wrapper->version = std::move(value.version);
        info->id = wrapper->id.c_str();
        info->name = wrapper->name.c_str();
        info->version = wrapper->version.c_str();
        info->abi_version = OGW_PLUGIN_ABI_VERSION;
        return OGW_OK;
    } catch (const std::exception& ex) {
        info->id = "";
        info->name = ex.what();
        info->version = "";
        info->abi_version = OGW_PLUGIN_ABI_VERSION;
        return OGW_ERR_INIT;
    }
}

enum class Lifecycle { Init, Start, Stop };

template <typename T>
int32_t call_lifecycle(ogw_plugin* plugin, char* err, std::size_t err_len, Lifecycle phase) {
    auto* wrapper = reinterpret_cast<Wrapper<T>*>(plugin);
    try {
        if (phase == Lifecycle::Init) {
            wrapper->impl->init(wrapper->config);
        } else if (phase == Lifecycle::Start) {
            wrapper->impl->start();
        } else {
            wrapper->impl->stop();
        }
        return OGW_OK;
    } catch (const SensorAbsent& ex) {
        copy_error(err, err_len, ex.what());
        return OGW_ERR_STATE;
    } catch (const std::exception& ex) {
        copy_error(err, err_len, ex.what());
        return OGW_ERR_INIT;
    } catch (...) {
        copy_error(err, err_len, "unknown plugin error");
        return OGW_ERR_INIT;
    }
}

template <typename T>
int32_t init_plugin(ogw_plugin* plugin, char* err, std::size_t err_len) {
    return call_lifecycle<T>(plugin, err, err_len, Lifecycle::Init);
}

template <typename T>
int32_t start_plugin(ogw_plugin* plugin, char* err, std::size_t err_len) {
    return call_lifecycle<T>(plugin, err, err_len, Lifecycle::Start);
}

template <typename T>
int32_t stop_plugin(ogw_plugin* plugin, char* err, std::size_t err_len) {
    return call_lifecycle<T>(plugin, err, err_len, Lifecycle::Stop);
}

template <typename T>
int32_t describe_settings(ogw_plugin* plugin, char* buf, std::size_t len) {
    auto* wrapper = reinterpret_cast<Wrapper<T>*>(plugin);
    try {
        const std::string text = wrapper->impl->settings_schema();
        copy_error(buf, len, text);
        return OGW_OK;
    } catch (const std::exception& ex) {
        copy_error(buf, len, ex.what());
        return OGW_ERR_INIT;
    } catch (...) {
        copy_error(buf, len, "unknown plugin error");
        return OGW_ERR_INIT;
    }
}

template <typename T>
const ogw_plugin_vtable* vtable() {
    static const ogw_plugin_vtable table{sizeof(ogw_plugin_vtable), &get_info<T>, &init_plugin<T>, &start_plugin<T>,
                                         &stop_plugin<T>, &describe_settings<T>};
    return &table;
}

template <typename T>
ogw_plugin* create_plugin(const ogw_host_api* host, const char* config_json) {
    if (host == nullptr || host->abi_version != OGW_PLUGIN_ABI_VERSION || host->size < sizeof(ogw_host_api)) {
        return nullptr;
    }
    try {
        auto wrapper = std::make_unique<Wrapper<T>>();
        wrapper->base.vtable = vtable<T>();
        wrapper->config = config_json == nullptr ? "{}" : config_json;
        wrapper->impl = std::make_unique<T>();
        wrapper->impl->bind(host);
        return &wrapper.release()->base;
    } catch (...) {
        return nullptr;
    }
}

template <typename T>
void destroy_plugin(ogw_plugin* plugin) {
    delete reinterpret_cast<Wrapper<T>*>(plugin);
}

}  // namespace detail

}  // namespace ogw::sdk

#define OGW_EXPORT_PLUGIN(Type)                                                                                  \
    extern "C" {                                                                                                 \
    __attribute__((visibility("default"))) ogw_plugin* ogw_plugin_create(const ogw_host_api* host,               \
                                                                         const char* config_json) {              \
        return ogw::sdk::detail::create_plugin<Type>(host, config_json);                                         \
    }                                                                                                            \
    __attribute__((visibility("default"))) void ogw_plugin_destroy(ogw_plugin* plugin) {                         \
        ogw::sdk::detail::destroy_plugin<Type>(plugin);                                                          \
    }                                                                                                            \
    }
