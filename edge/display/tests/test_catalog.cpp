#include "catalog.hpp"

int main() {
    const auto support = R"({
      "sources": [
        {"id":"can-motor","name":"Motor Control","state":"inactive","error":"","simulated":false},
        {"id":"motor-sim","name":"Motor Control","state":"inactive","error":"","simulated":true},
        {"id":"temperature-sim","name":"Temperature","state":"active","error":"","simulated":true},
        {"id":"","name":"Nope","state":"active","error":"","simulated":true}
      ]
    })";
    const auto devices = R"({
      "devices": [
        {"deviceId":"can-motor/showcase","pluginId":"can-motor","displayName":"Motor Control","status":"disconnected","capabilities":[]},
        {"deviceId":"temperature-sim/room-1","pluginId":"temperature-sim","displayName":"Room Sensor","status":"connected","capabilities":[
          {"id":"temperature","kind":"measurement","displayName":"Temperature","unit":"°C","current":{"value":21.5,"quality":"good"}},
          {"id":"set_duty","kind":"command","displayName":"Duty"}
        ]},
        {"deviceId":"orphan/x","pluginId":"orphan","displayName":"Orphan","status":"connected","capabilities":[]}
      ]
    })";
    const auto view = catalog_from_json(devices, support);
    if (view.sensors.size() != 3) {
        return 1;
    }
    if (view.sensors[0].availability != "greyed" || view.sensors[0].canToggle) {
        return 2;
    }
    if (view.sensors[1].availability != "disabled" || !view.sensors[1].canToggle) {
        return 3;
    }
    if (view.sensors[2].availability != "enabled") {
        return 4;
    }
    if (view.dashboard.size() != 1 || view.dashboard[0].id != "temperature-sim/room-1") {
        return 5;
    }
    if (!view.dashboard[0].values.contains("21.5") || view.dashboard[0].commandId != "set_duty") {
        return 6;
    }
    bool orphan = false;
    for (const auto& card : view.dashboard) {
        if (card.id == "orphan/x") {
            orphan = true;
        }
    }
    if (orphan) {
        return 7;
    }
    return 0;
}
