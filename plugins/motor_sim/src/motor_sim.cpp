#include "ogw/sdk/plugin.hpp"
#include "ogw/sdk/scenario.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace {

constexpr const char* kDevice = R"({
  "localId": "motor-01",
  "displayName": "Simulated Motor",
  "status": "connected",
  "capabilities": [
    {
      "id": "speed",
      "kind": "measurement",
      "valueType": "number",
      "displayName": "Speed",
      "unit": "rpm",
      "presentation": "gauge",
      "range": {"min": 0, "max": 3000},
      "staleAfterMs": 5000
    },
    {
      "id": "state",
      "kind": "measurement",
      "valueType": "enum",
      "displayName": "State",
      "presentation": "status",
      "staleAfterMs": 5000,
      "enumStates": [
        {"id": "stopped", "label": "Stopped", "tone": "neutral"},
        {"id": "running", "label": "Running", "tone": "ok"},
        {"id": "fault", "label": "Fault", "tone": "fault"}
      ]
    },
    {
      "id": "winding_temperature",
      "kind": "measurement",
      "valueType": "number",
      "displayName": "Winding Temperature",
      "unit": "°C",
      "presentation": "value",
      "range": {"min": 0, "max": 150},
      "staleAfterMs": 5000
    }
  ]
})";

class MotorPlugin final : public ogw::sdk::Plugin {
public:
    ogw::sdk::Info info() const override { return {"motor-sim", "Motor Simulator", "0.1.0"}; }

    std::string settings_schema() const override {
        return R"([{"name":"intervalMs","valueType":"number","min":200,"max":60000}])";
    }

    void init(std::string_view config) override {
        scenario_ = ogw::sdk::parse_scenario(config);
        if (scenario_.timeline.empty()) {
            scenario_.timeline = {{20000, "running"}, {10000, "fault"}, {10000, "disconnected"}};
        }
    }

    void start() override {
        host().register_device(kDevice);
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
        const std::string json = std::string(R"({"localId":"motor-01","status":")") + status + "\"}";
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
                } else if (mode == "fault") {
                    publish_status("faulted");
                } else {
                    publish_status("connected");
                }
            }
            if (mode != "disconnected") {
                const auto now = host().wall_time_ms();
                const bool fault = mode == "fault";
                const double speed = fault ? 0.0 : ogw::sdk::wave(scenario_.seed, tick, 1500.0, 40.0);
                auto speed_reading = ogw::sdk::number_reading(speed, fault ? OGW_QUALITY_BAD : OGW_QUALITY_GOOD, now,
                                                              fault ? "fault" : nullptr);
                host().publish("motor-01", "speed", speed_reading);
                const char* state = fault ? "fault" : "running";
                auto state_reading = ogw::sdk::string_reading(state, fault ? OGW_QUALITY_BAD : OGW_QUALITY_GOOD, now,
                                                              fault ? "fault" : nullptr);
                host().publish("motor-01", "state", state_reading);
                const double winding = fault ? 120.0 : ogw::sdk::wave(scenario_.seed, tick, 62.0, 3.0);
                auto winding_reading = ogw::sdk::number_reading(winding, fault ? OGW_QUALITY_BAD : OGW_QUALITY_GOOD, now,
                                                                fault ? "above_range" : nullptr);
                host().publish("motor-01", "winding_temperature", winding_reading);
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

OGW_EXPORT_PLUGIN(MotorPlugin)
