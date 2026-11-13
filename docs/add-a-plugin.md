# Add a plugin

A plugin is a shared library that depends on the SDK headers and not on the gateway core.

## Contract

Export `ogw_plugin_create` and `ogw_plugin_destroy`. The SDK macro does that when you subclass `ogw::sdk::Plugin`:

```cpp
#include "ogw/sdk/plugin.hpp"

class ClockPlugin final : public ogw::sdk::Plugin {
 public:
  ogw::sdk::Info info() const override { return {"clock", "Clock", "0.1.0"}; }
  void init(std::string_view) override {}
  void start() override { /* register_device, then publish */ }
  void stop() override { /* join your threads */ }
};

OGW_EXPORT_PLUGIN(ClockPlugin)
```

`info().id` must match the `id` in the gateway config. The ABI version is 1. A mismatch marks that plugin failed and leaves the process running.

Override `settings_schema()` when the client may change a value. Return a JSON array. Each object has `name`, `valueType` (`number`, `bool`, or `string`), and optional `min` and `max`. The host copies that schema into the support document and rejects a PUT whose value has the wrong type or falls outside the range. Applying settings recreates the plugin with the merged config.

```cpp
std::string settings_schema() const override {
    return R"([{"name":"intervalMs","valueType":"number","min":200,"max":60000}])";
}
```

Register a device with JSON:

```json
{
  "localId": "desk",
  "displayName": "Desk",
  "status": "connected",
  "capabilities": [{
    "id": "temperature",
    "kind": "measurement",
    "valueType": "number",
    "displayName": "Temperature",
    "unit": "°C",
    "presentation": "gauge",
    "range": {"min": 0, "max": 40},
    "staleAfterMs": 5000
  }]
}
```

`valueType` is `number`, `bool`, `enum`, or `string`. Enum capabilities include `enumStates`. Publish readings with `host().publish`. Call `update_device` with `{"localId":"desk","status":"disconnected"}` when the source goes away. The host then marks current values stale.

Build the plugin against an installed SDK, the same way `plugins/motor_sim` does:

```bash
cmake -S plugins/motor_sim -B build/motor -DOGW_SDK_DIR=/path/to/sdk
cmake --build build/motor
```

Copy the resulting `.so` into the gateway plugin directory and ask the operator listener to scan:

```bash
curl -fsS -X POST http://127.0.0.1:8081/api/v1/admin/plugins/scan
```

Scan advertises the source. The sample client or the Qt screen then activates it with `PUT /api/v1/support/sources/clock` and `{"state":"active"}`. The public API does not accept a library path or a file upload.

Do not edit `src/`, `sample_client`, or `edge/display` to add the plugin. The admin API is for operators on a trusted network. The cloud and Pi profiles leave it off.

## Checklist

- The `.so` exports the C entry points from the SDK and links only the SDK.
- `init` failure is reported and the gateway stays up.
- Device ids use `[A-Za-z0-9._-]` and are at most 64 characters.
- Capabilities declare `kind`, `valueType`, and `staleAfterMs`. Readings include a timestamp and a quality.
- Client-visible settings are declared in `settings_schema`, with a type and a range.
- `stop` returns only after your threads have finished.
- Both clients show the new device from the API, with no client change.
