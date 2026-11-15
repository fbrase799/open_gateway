#include "catalog.hpp"
#include "display_config.hpp"

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
    if (view.sensors[1].name != "Motor Control\nSimulator") {
        return 11;
    }
    if (view.sensors[2].name != "Temperature\nSimulator") {
        return 12;
    }
    const auto titled_sim = R"({"sources":[{"id":"temperature-sim","name":"Temperature Simulator","state":"loaded","simulated":true}]})";
    const auto titled_view = catalog_from_json("{\"devices\":[]}", titled_sim);
    if (titled_view.sensors.size() != 1 || titled_view.sensors[0].name != "Temperature\nSimulator") {
        return 13;
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
    DisplaySettings hideTemperature;
    hideTemperature.simulators.insert(QStringLiteral("temperature-sim"), DeviceEntry{false, {}});
    const auto hidden = catalog_from_json(devices, support, hideTemperature);
    if (hidden.sensors.size() != 2 || hidden.sensors[0].id != "can-motor" || hidden.sensors[1].id != "motor-sim") {
        return 14;
    }
    if (!hidden.dashboard.isEmpty()) {
        return 15;
    }
    const auto settings = display_settings_from_toml(
        "title = \"Bench\"\n\n"
        "[simulators.temperature-sim]\nenabled = false\nname = \"Room\"\n\n"
        "[simulators.motor-sim]\nenabled = true\nname = \"Motor Control\"\n\n"
        "[devices.can-motor]\nenabled = true\nname = \"Drive\"\n\n"
        "[simulators]\nother-sim = false\n");
    if (settings.title != "Bench" || settings.simulators.value(QStringLiteral("temperature-sim")).enabled ||
        settings.simulators.value(QStringLiteral("temperature-sim")).name != "Room" ||
        settings.devices.value(QStringLiteral("can-motor")).name != "Drive" ||
        settings.simulators.value(QStringLiteral("other-sim")).enabled) {
        return 16;
    }
    DisplaySettings renamed;
    renamed.simulators.insert(QStringLiteral("temperature-sim"), DeviceEntry{true, QStringLiteral("Room")});
    renamed.devices.insert(QStringLiteral("can-motor"), DeviceEntry{true, QStringLiteral("Drive")});
    const auto named = catalog_from_json(devices, support, renamed);
    if (named.sensors[0].name != "Drive" || named.sensors[2].name != "Room\nSimulator" || named.dashboard[0].name != "Room") {
        return 17;
    }
    DisplaySettings hideReal;
    hideReal.devices.insert(QStringLiteral("can-motor"), DeviceEntry{false, {}});
    const auto withoutReal = catalog_from_json(devices, support, hideReal);
    if (withoutReal.sensors.size() != 2 || withoutReal.sensors[0].id != "motor-sim") {
        return 18;
    }
    const auto titled = display_title_from_toml("# comment\n\ntitle = \"Motor Bench\"\n");
    if (titled != "Motor Bench") {
        return 8;
    }
    const auto quoted = display_title_from_toml("title = 'Open Gateway'\n");
    if (quoted != "Open Gateway") {
        return 9;
    }
    const auto fallback = display_title_from_toml("other = \"nope\"\n");
    if (fallback != "Open Gateway") {
        return 10;
    }
    return 0;
}
