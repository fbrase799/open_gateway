#include "ogw/config.hpp"

#include "ogw/log.hpp"
#include "ogw/source_policy.hpp"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace ogw {
namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot read config file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool parse_bool(const std::string& text) {
    return text == "1" || text == "true" || text == "TRUE" || text == "yes";
}

std::uint16_t parse_port(const std::string& text, const char* name) {
    try {
        const int value = std::stoi(text);
        if (value < 1 || value > 65535) {
            throw std::runtime_error("out of range");
        }
        return static_cast<std::uint16_t>(value);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string(name) + " is not a valid port");
    }
}

LogLevel parse_level(const std::string& text) {
    if (text == "error") return LogLevel::Error;
    if (text == "warn") return LogLevel::Warn;
    if (text == "info") return LogLevel::Info;
    if (text == "debug") return LogLevel::Debug;
    throw std::runtime_error("log level must be error, warn, info, or debug");
}

void apply_json(Config& config, const nlohmann::json& document) {
    if (document.contains("httpPort")) {
        config.httpPort = static_cast<std::uint16_t>(document.at("httpPort").get<int>());
    }
    if (document.contains("adminPort")) {
        config.adminPort = static_cast<std::uint16_t>(document.at("adminPort").get<int>());
    }
    if (document.contains("adminApi")) {
        config.adminApi = document.at("adminApi").get<bool>();
    }
    if (document.contains("pluginDir")) {
        config.pluginDir = document.at("pluginDir").get<std::string>();
    }
    if (document.contains("historySamples")) {
        config.historySamples = document.at("historySamples").get<std::size_t>();
    }
    if (document.contains("logLevel")) {
        config.logLevel = parse_level(document.at("logLevel").get<std::string>());
    }
    if (document.contains("corsOrigin")) {
        config.corsOrigin = document.at("corsOrigin").get<std::string>();
    }
    if (document.contains("mqttUrl") && document.at("mqttUrl").is_string()) {
        config.mqttUrl = document.at("mqttUrl").get<std::string>();
    }
    if (document.contains("plugins")) {
        config.plugins.clear();
        for (const auto& item : document.at("plugins")) {
            PluginConfig plugin;
            plugin.id = item.at("id").get<std::string>();
            plugin.file = item.at("file").get<std::string>();
            plugin.activate = item.value("activate", false);
            std::optional<bool> configured_simulated;
            if (item.contains("simulated")) {
                configured_simulated = item.at("simulated").get<bool>();
            }
            plugin.simulated = resolve_simulated(plugin.id, configured_simulated);
            if (item.contains("config")) {
                plugin.configJson = item.at("config").dump();
            }
            config.plugins.push_back(std::move(plugin));
        }
    }
}

const char* env_or_null(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return nullptr;
    }
    return value;
}

}  // namespace

Config load_config() {
    Config config;
    if (const char* path = env_or_null("OGW_CONFIG")) {
        const auto document = nlohmann::json::parse(read_file(path));
        apply_json(config, document);
    } else {
        std::ifstream fallback("/etc/ogw/gateway.json");
        if (fallback) {
            std::ostringstream buffer;
            buffer << fallback.rdbuf();
            apply_json(config, nlohmann::json::parse(buffer.str()));
        }
    }

    if (const char* value = env_or_null("OGW_HTTP_PORT")) {
        config.httpPort = parse_port(value, "OGW_HTTP_PORT");
    }
    if (const char* value = env_or_null("OGW_ADMIN_PORT")) {
        config.adminPort = parse_port(value, "OGW_ADMIN_PORT");
    }
    if (const char* value = env_or_null("OGW_ADMIN_API")) {
        config.adminApi = parse_bool(value);
    }
    if (const char* value = env_or_null("OGW_PLUGIN_DIR")) {
        config.pluginDir = value;
    }
    if (const char* value = env_or_null("OGW_HISTORY_SAMPLES")) {
        config.historySamples = static_cast<std::size_t>(std::stoul(value));
    }
    if (const char* value = env_or_null("OGW_LOG_LEVEL")) {
        config.logLevel = parse_level(value);
    }
    if (const char* value = env_or_null("OGW_CORS_ORIGIN")) {
        config.corsOrigin = value;
    }
    if (const char* value = env_or_null("OGW_MQTT_URL")) {
        config.mqttUrl = value;
    }
    if (config.httpPort == config.adminPort && config.adminApi) {
        throw std::runtime_error("httpPort and adminPort must be different");
    }
    return config;
}

}  // namespace ogw
