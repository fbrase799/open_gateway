#pragma once

#include "ogw/config.hpp"
#include "ogw/http_server.hpp"
#include "ogw/mqtt_client.hpp"
#include "ogw/pipeline.hpp"
#include "ogw/plugin_manager.hpp"
#include "ogw/registry.hpp"

namespace ogw {

class Application {
public:
    explicit Application(Config config);
    int run(bool headless);

private:
    Config config_;
    SystemClock clock_;
    DeviceRegistry registry_;
    TelemetryPipeline pipeline_;
    PluginManager plugins_;
    WsHub hub_;
    MqttClient mqtt_;
};

}  // namespace ogw
