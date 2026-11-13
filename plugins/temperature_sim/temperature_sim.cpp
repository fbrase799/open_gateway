#include "ogw/sdk/plugin.hpp"
#include "ogw/sdk/scenario.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace {

constexpr const char* kConnected = R"({
  "localId": "room-1",
  "displayName": "Room Sensor",
  "status": "connected",
  "capabilities": [
    {
      "id": "temperature",
      "kind": "measurement",
      "valueType": "number",
      "displayName": "Temperature",
      "unit": "°C",
      "presentation": "gauge",
      "range": {"min": -10, "max": 50},
      "staleAfterMs": 5000
    },
    {
      "id": "humidity",
      "kind": "measurement",
      "valueType": "number",
      "displayName": "Humidity",
      "unit": "%",
      "presentation": "sparkline",
      "range": {"min": 0, "max": 100},
      "staleAfterMs": 5000
    }
  ]
})";

class TemperaturePlugin final : public ogw::sdk::Plugin {
public:
    ogw::sdk::Info info() const override { return {"temperature-sim", "Temperature Simulator", "0.1.0"}; }

    std::string settings_schema() const override {
        return R"([{"name":"intervalMs","valueType":"number","min":200,"max":60000}])";
    }

    void init(std::string_view config) override {
        scenario_ = ogw::sdk::parse_scenario(config);
        if (scenario_.timeline.empty()) {
            scenario_.timeline = {{20000, "normal"}, {10000, "out_of_range"}, {10000, "disconnected"}};
        }
    }

    void start() override {
        host().register_device(kConnected);
        running_.store(true);
        started_ = std::chrono::steady_clock::now();
        thread_ = std::thread([this] { loop(); });
    }

    void stop() override {
        running_.store(false);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void publish_status(const char* status) {
        const std::string json = std::string(R"({"localId":"room-1","status":")") + status + "\"}";
        host().update_device(json);
    }

    void loop() {
        std::string mode;
        std::int64_t tick = 0;
        while (running_.load()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                      started_)
                                     .count();
            const auto next = scenario_.mode_at(elapsed);
            if (next != mode) {
                mode = next;
                if (mode == "disconnected") {
                    publish_status("disconnected");
                } else if (mode == "out_of_range") {
                    publish_status("faulted");
                } else {
                    publish_status("connected");
                }
            }
            if (mode != "disconnected") {
                const auto now = host().wall_time_ms();
                const bool fault = mode == "out_of_range";
                const double temperature = fault ? 88.0 : ogw::sdk::wave(scenario_.seed, tick, 21.5, 1.5);
                auto temperature_reading =
                    ogw::sdk::number_reading(temperature, fault ? OGW_QUALITY_BAD : OGW_QUALITY_GOOD, now,
                                             fault ? "above_range" : nullptr);
                host().publish("room-1", "temperature", temperature_reading);
                auto humidity = ogw::sdk::number_reading(ogw::sdk::wave(scenario_.seed, tick, 48.0, 4.0), OGW_QUALITY_GOOD,
                                                         now, nullptr);
                host().publish("room-1", "humidity", humidity);
            }
            ++tick;
            std::this_thread::sleep_for(std::chrono::milliseconds(scenario_.intervalMs));
        }
    }

    ogw::sdk::Scenario scenario_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point started_{};
    std::thread thread_;
};

}  // namespace

OGW_EXPORT_PLUGIN(TemperaturePlugin)
