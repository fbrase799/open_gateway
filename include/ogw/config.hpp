#pragma once

#include "ogw/log.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ogw {

struct PluginConfig {
    std::string id;
    std::string file;
    bool activate = false;
    bool simulated = false;
    std::string configJson = "{}";
};

struct Config {
    std::uint16_t httpPort = 8080;
    std::uint16_t adminPort = 8081;
    bool adminApi = false;
    std::string pluginDir = "/var/lib/ogw/plugins";
    std::size_t historySamples = 600;
    std::size_t telemetryQueue = 4096;
    LogLevel logLevel = LogLevel::Info;
    std::string corsOrigin;
    std::string mqttUrl;
    std::vector<PluginConfig> plugins;
};

Config load_config();

}  // namespace ogw
