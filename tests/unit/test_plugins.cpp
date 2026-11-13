#include "ogw/clock.hpp"
#include "ogw/descriptor.hpp"
#include "ogw/pipeline.hpp"
#include "ogw/plugin_manager.hpp"
#include "ogw/registry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace {

struct TempDir {
    std::filesystem::path path;
    TempDir() {
        static std::atomic<int> sequence{0};
        path = std::filesystem::temp_directory_path() /
               ("ogw-" + std::to_string(::getpid()) + "-" + std::to_string(sequence.fetch_add(1)));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

class Fixture {
public:
    explicit Fixture(std::filesystem::path dir)
        : directory(std::move(dir)), pipeline(registry, clock, 32), plugins(registry, pipeline, clock, directory.string(), 32) {}

    ogw::ManualClock clock{1'000};
    ogw::DeviceRegistry registry;
    std::filesystem::path directory;
    ogw::TelemetryPipeline pipeline;
    ogw::PluginManager plugins;
};

}  // namespace

TEST_CASE("device documents accept the public capability shape") {
    auto parsed = ogw::parse_device_json(R"({
        "localId": "room-1",
        "displayName": "Room Sensor",
        "status": "connected",
        "capabilities": [{
            "id": "temperature",
            "kind": "measurement",
            "valueType": "number",
            "displayName": "Temperature",
            "unit": "C",
            "presentation": "gauge",
            "range": {"min": -10, "max": 50},
            "staleAfterMs": 5000
        }]
    })",
                                         true);
    REQUIRE(parsed.ok);
    REQUIRE(parsed.draft.localId == "room-1");
    REQUIRE(parsed.draft.capabilities->size() == 1);
}

TEST_CASE("a plugin loads, publishes, and deactivates without stopping the host") {
    TempDir dir;
    std::filesystem::copy_file(OGW_TEST_PLUGIN_PATH, dir.path / "libtest_plugin.so");
    Fixture fixture(dir.path);
    auto activated = fixture.plugins.load_configured("libtest_plugin.so", "test-plugin", "{}", true);
    REQUIRE(activated.ok);
    auto device = fixture.registry.find("test-plugin/dev-1");
    REQUIRE(device);
    REQUIRE_FALSE(device->readings.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    auto stopped = fixture.plugins.deactivate("test-plugin");
    REQUIRE(stopped.ok);
    const auto accepted = fixture.pipeline.counters().accepted;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    REQUIRE(fixture.pipeline.counters().accepted == accepted);
    device = fixture.registry.find("test-plugin/dev-1");
    REQUIRE(device);
    REQUIRE(device->status == ogw::DeviceStatus::Disconnected);
}

TEST_CASE("init failure and abi mismatch stay inside the plugin") {
    TempDir dir;
    std::filesystem::copy_file(OGW_TEST_PLUGIN_PATH, dir.path / "libtest_plugin.so");
    std::filesystem::copy_file(OGW_ABI_PLUGIN_PATH, dir.path / "libabi_mismatch.so");
    Fixture fixture(dir.path);

    auto failed = fixture.plugins.load_configured("libtest_plugin.so", "test-plugin", R"({"failInit":true})", true);
    REQUIRE_FALSE(failed.ok);
    auto abi = fixture.plugins.load_configured("libabi_mismatch.so", "abi-mismatch", "{}", false);
    REQUIRE_FALSE(abi.ok);

    bool saw_init = false;
    bool saw_abi = false;
    for (const auto& plugin : fixture.plugins.list()) {
        if (plugin.id == "test-plugin") {
            saw_init = plugin.state == ogw::PluginState::Failed;
        }
        if (plugin.id == "abi-mismatch") {
            saw_abi = plugin.state == ogw::PluginState::Failed && plugin.error.find("abi") != std::string::npos;
        }
    }
    REQUIRE(saw_init);
    REQUIRE(saw_abi);
    REQUIRE(fixture.registry.size() == 0);
}

TEST_CASE("scan activates a library copied in after startup") {
    TempDir dir;
    Fixture fixture(dir.path);
    REQUIRE(fixture.plugins.scan().ok);
    REQUIRE(fixture.plugins.list().empty());
    std::filesystem::copy_file(OGW_TEST_PLUGIN_PATH, dir.path / "libtest_plugin.so");
    REQUIRE(fixture.plugins.scan().ok);
    auto activated = fixture.plugins.activate("test-plugin");
    REQUIRE(activated.ok);
    REQUIRE(fixture.registry.find("test-plugin/dev-1").has_value());
}
