#include "ogw/sdk/plugin.hpp"

#include <atomic>
#include <chrono>
#include <string_view>
#include <thread>

namespace {

constexpr const char* kDevice = R"({
  "localId": "dev-1",
  "displayName": "Test Device",
  "status": "connected",
  "capabilities": [
    {
      "id": "value",
      "kind": "measurement",
      "valueType": "number",
      "displayName": "Value",
      "presentation": "value",
      "staleAfterMs": 60000
    }
  ]
})";

class TestPlugin final : public ogw::sdk::Plugin {
public:
    ogw::sdk::Info info() const override { return {"test-plugin", "Test Plugin", "0.1.0"}; }

    void init(std::string_view config) override {
        if (config.find("failInit") != std::string_view::npos) {
            throw std::runtime_error("init failed");
        }
    }

    void start() override {
        host().register_device(kDevice);
        const auto now = host().wall_time_ms();
        auto reading = ogw::sdk::number_reading(1.0, OGW_QUALITY_GOOD, now, nullptr);
        host().publish("dev-1", "value", reading);
        running_.store(true);
        thread_ = std::thread([this] {
            while (running_.load()) {
                const auto timestamp = host().wall_time_ms();
                auto sample = ogw::sdk::number_reading(2.0, OGW_QUALITY_GOOD, timestamp, nullptr);
                host().publish("dev-1", "value", sample);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }

    void stop() override {
        running_.store(false);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace

OGW_EXPORT_PLUGIN(TestPlugin)
