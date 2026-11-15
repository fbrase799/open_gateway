#include "ogw/bounded_queue.hpp"
#include "ogw/clock.hpp"
#include "ogw/pipeline.hpp"
#include "ogw/registry.hpp"
#include "ogw/source_policy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

namespace {

ogw::Capability number_capability(std::string id, std::int64_t stale_after = 1000) {
    ogw::Capability capability;
    capability.id = std::move(id);
    capability.kind = "measurement";
    capability.valueType = ogw::ValueType::Number;
    capability.displayName = capability.id;
    capability.staleAfterMs = stale_after;
    return capability;
}

ogw::DeviceDraft room(std::string local_id, std::vector<ogw::Capability> capabilities) {
    ogw::DeviceDraft draft;
    draft.localId = std::move(local_id);
    draft.displayName = "Room";
    draft.capabilities = std::move(capabilities);
    draft.status = ogw::DeviceStatus::Connected;
    return draft;
}

}  // namespace

TEST_CASE("registry keeps plugins distinct") {
    ogw::DeviceRegistry registry;
    auto first = registry.upsert("alpha", room("room-1", {number_capability("temperature")}), true);
    auto second = registry.upsert("beta", room("room-1", {number_capability("temperature")}), true);
    REQUIRE(first.ok);
    REQUIRE(second.ok);
    REQUIRE(first.deviceId == "alpha/room-1");
    REQUIRE(second.deviceId == "beta/room-1");
    REQUIRE(registry.size() == 2);
}

TEST_CASE("registry rejects an invalid capability id") {
    ogw::DeviceRegistry registry;
    auto capability = number_capability("bad id");
    auto result = registry.upsert("alpha", room("room-1", {capability}), true);
    REQUIRE_FALSE(result.ok);
    REQUIRE(registry.size() == 0);
}

TEST_CASE("invalid readings do not replace the current value") {
    ogw::ManualClock clock(1'000);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 10);
    REQUIRE(registry.upsert("alpha", room("room-1", {number_capability("temperature")}), true).ok);

    ogw::Reading good;
    good.value = 21.5;
    good.quality = ogw::Quality::Good;
    good.sourceTimestampMs = 1'000;
    REQUIRE(pipeline.publish("alpha/room-1", "temperature", good).accepted);

    ogw::Reading bad = good;
    bad.value = std::numeric_limits<double>::quiet_NaN();
    auto rejected = pipeline.publish("alpha/room-1", "temperature", bad);
    REQUIRE_FALSE(rejected.accepted);
    REQUIRE(rejected.reason == "non_finite");

    ogw::Reading wrong_type = good;
    wrong_type.value = true;
    REQUIRE(pipeline.publish("alpha/room-1", "temperature", wrong_type).reason == "type_mismatch");

    auto current = registry.find("alpha/room-1");
    REQUIRE(current);
    REQUIRE(std::get<double>(current->readings.at(0).second.value) == 21.5);
    REQUIRE(pipeline.counters().rejected == 2);
    REQUIRE(pipeline.counters().accepted == 1);
}

TEST_CASE("enum tokens must be declared") {
    ogw::ManualClock clock(0);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 4);
    ogw::Capability state;
    state.id = "state";
    state.kind = "measurement";
    state.valueType = ogw::ValueType::Enum;
    state.displayName = "State";
    state.staleAfterMs = 1000;
    state.enumStates.push_back({"running", "Running", "ok"});
    REQUIRE(registry.upsert("alpha", room("motor-01", {state}), true).ok);

    ogw::Reading reading;
    reading.value = std::string("flying");
    reading.sourceTimestampMs = 1;
    REQUIRE(pipeline.publish("alpha/motor-01", "state", reading).reason == "bad_enum");
}

TEST_CASE("disconnect keeps the last value and marks it stale") {
    ogw::ManualClock clock(5'000);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 4);
    REQUIRE(registry.upsert("alpha", room("room-1", {number_capability("temperature")}), true).ok);
    ogw::Reading reading;
    reading.value = 12.5;
    reading.quality = ogw::Quality::Good;
    reading.sourceTimestampMs = 4'000;
    REQUIRE(pipeline.publish("alpha/room-1", "temperature", reading).accepted);

    auto updates = pipeline.mark_plugin_disconnected("alpha");
    REQUIRE_FALSE(updates.empty());
    auto device = registry.find("alpha/room-1");
    REQUIRE(device);
    REQUIRE(device->status == ogw::DeviceStatus::Disconnected);
    REQUIRE(std::get<double>(device->readings.at(0).second.value) == 12.5);
    REQUIRE(device->readings.at(0).second.quality == ogw::Quality::Stale);
    REQUIRE(device->readings.at(0).second.detail == "disconnected");
    REQUIRE(device->readings.at(0).second.sourceTimestampMs == 4'000);
}

TEST_CASE("staleness uses the injected clock") {
    ogw::ManualClock clock(10'000);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 4);
    REQUIRE(registry.upsert("alpha", room("room-1", {number_capability("temperature", 1000)}), true).ok);
    ogw::Reading reading;
    reading.value = 19.0;
    reading.quality = ogw::Quality::Good;
    reading.sourceTimestampMs = 10'000;
    REQUIRE(pipeline.publish("alpha/room-1", "temperature", reading).accepted);

    clock.advance(1001);
    auto updates = pipeline.evaluate_staleness();
    REQUIRE(updates.size() == 1);
    REQUIRE(updates[0].reading.quality == ogw::Quality::Stale);
    REQUIRE(std::get<double>(updates[0].reading.value) == 19.0);
}

TEST_CASE("history keeps the newest samples in order") {
    ogw::ManualClock clock(0);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 2);
    REQUIRE(registry.upsert("alpha", room("room-1", {number_capability("temperature")}), true).ok);
    for (double value : {1.0, 2.0, 3.0}) {
        ogw::Reading reading;
        reading.value = value;
        reading.sourceTimestampMs = static_cast<std::int64_t>(value);
        REQUIRE(pipeline.publish("alpha/room-1", "temperature", reading).accepted);
    }
    auto samples = pipeline.history("alpha/room-1", "temperature", 10);
    REQUIRE(samples.size() == 2);
    REQUIRE(std::get<double>(samples[0].value) == 2.0);
    REQUIRE(std::get<double>(samples[1].value) == 3.0);
}

TEST_CASE("bounded queue drops the oldest item") {
    ogw::BoundedQueue<int> queue(2);
    queue.push_drop_oldest(1);
    queue.push_drop_oldest(2);
    queue.push_drop_oldest(3);
    REQUIRE(queue.dropped() == 1);
    REQUIRE(queue.try_pop() == 2);
    REQUIRE(queue.try_pop() == 3);
    REQUIRE_FALSE(queue.try_pop().has_value());
}

TEST_CASE("a real sensor is probed and a simulator stays off") {
    const ogw::SourceRequest real{false, true};
    const ogw::SourceRequest fallback{true, true};
    const ogw::SourceRequest optional_sim{true, false};
    REQUIRE(ogw::probe_source(real));
    REQUIRE_FALSE(ogw::probe_source(fallback));
    REQUIRE_FALSE(ogw::use_simulated_source(fallback, true));
    REQUIRE_FALSE(ogw::use_simulated_source(fallback, false));
    REQUIRE_FALSE(ogw::use_simulated_source(optional_sim, false));
    REQUIRE(ogw::resolve_simulated("temperature-sim", std::nullopt));
    REQUIRE_FALSE(ogw::resolve_simulated("can-motor", std::nullopt));
    REQUIRE_FALSE(ogw::resolve_simulated("temperature-sim", false));
}

TEST_CASE("a command capability is not a measurement") {
    ogw::ManualClock clock(0);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 4);
    auto command = number_capability("set_duty");
    command.kind = "command";
    REQUIRE(registry.upsert("motor", room("showcase", {command}), true).ok);
    ogw::Reading reading;
    reading.value = 40.0;
    reading.sourceTimestampMs = 1;
    auto published = pipeline.publish("motor/showcase", "set_duty", reading);
    REQUIRE_FALSE(published.accepted);
    REQUIRE(published.reason == "command_not_measurement");
    auto actuator = number_capability("move");
    actuator.kind = "actuator";
    REQUIRE_FALSE(registry.upsert("motor", room("other", {actuator}), true).ok);
}

TEST_CASE("two sources share one pipeline") {
    ogw::ManualClock clock(0);
    ogw::DeviceRegistry registry;
    ogw::TelemetryPipeline pipeline(registry, clock, 8);
    REQUIRE(registry.upsert("sim", room("a", {number_capability("temperature")}), true).ok);
    REQUIRE(registry.upsert("other", room("b", {number_capability("speed")}), true).ok);
    ogw::Reading first;
    first.value = 20.0;
    first.sourceTimestampMs = 1;
    ogw::Reading second;
    second.value = 100.0;
    second.sourceTimestampMs = 2;
    REQUIRE(pipeline.publish("sim/a", "temperature", first).accepted);
    REQUIRE(pipeline.publish("other/b", "speed", second).accepted);
    REQUIRE(pipeline.counters().accepted == 2);
}
