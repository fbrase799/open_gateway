#include "catalog.hpp"

#include <QtGlobal>

int main() {
    const auto devices = devices_from_json(R"({"devices":[{"deviceId":"temperature-sim/room-1","displayName":"Room Sensor","status":"connected"}]})");
    const auto sources = sources_from_json(R"({"sources":[{"id":"temperature-sim","name":"Temperature","state":"inactive","error":""}]})");
    if (devices.size() != 1 || devices[0].status != "connected") {
        return 1;
    }
    if (sources.size() != 1 || sources[0].id != "temperature-sim") {
        return 2;
    }
    const auto absent = sources_from_json(R"({"sources":[]})");
    if (!absent.isEmpty()) {
        return 3;
    }
    return 0;
}
