# Open Gateway — Product Requirements Document

**Project:** Open Gateway  
**Repository:** `open-gateway`  
**Status:** Initial draft  
**Primary language:** Modern C++ (C++23) for the gateway. Qt/QML for the edge display. React for the sample web client.

## 1. Product vision

Open Gateway is a standalone, extensible data gateway that connects heterogeneous devices and data sources to external consumers. Its main purpose is to demonstrate modern C++ software architecture through **runtime-loadable plugins**, **dependency injection**, **clear separation of concerns**, and **metadata-driven clients**.

The gateway runs in two places: on **Azure**, as a public simulated showcase, and on a **Raspberry Pi 4** with a **7-inch display**, as the edge box a person can stand in front of. The gateway process is headless in both places.

On the Pi, a **Qt/QML** application is the local frontend. The Pi image, including Yocto and U-Boot, is a separate product. That product includes this source and builds it with the Qt from its image. The screen runs on the 7-inch display, in front of the Gateway API: a person uses that screen, and the screen talks only to the API. It shows which devices are connected. As a start it can enable and disable the simulated devices the gateway advertises. It does not link the gateway core and it does not load plugins itself. This repository does not contain that Yocto layer. When this product deploys the same Qt UI as a Docker container, it is exposed through noVNC so a browser can show it.

A separate **React** dashboard lives in `sample_client`. It is a sample consumer of the same API, including the dashboard now developed under `web/dashboard`. Azure serves this sample client. The Pi does not need it for the local screen.

The central demonstration is that a new plugin can be installed and activated **while the gateway is running**, after which its capabilities and live data become available through the gateway's standard API and appear in both clients **without changing or redeploying the gateway core or those clients**.

Clients ask the gateway which retrieval methods and installed sources it currently supports, and render only those options. When a new source is connected, the gateway informs connected clients of the new configuration options. The clients gain those options without a client change.

The first physical device is the separately developed STM32F404 bare-metal motor controller, reached over **CAN**. When that controller is connected, it is the sensor the dashboards show. A simulated sensor is used only when no real sensor is found. The cloud showcase has no CAN hardware, so it uses the simulated sensor. The controller firmware is not part of this product.

## 2. Goals

1. Demonstrate extensible architecture using dependency inversion and dependency injection.
2. Add support for new transports, data sources, and device capabilities without modifying gateway core logic.
3. Discover, load, configure, start, stop, and inspect plugins at runtime.
4. Expose a consistent, technology-independent device and telemetry model.
5. Generate useful UI views from capability metadata, not device-specific frontend code.
6. Provide an accessible end-to-end demonstration using simulated devices, locally, on Azure, and on a Raspberry Pi 4 with a 7-inch display.
7. Demonstrate production-minded engineering: automated tests, observable failures, controlled concurrency, safe shutdown, and CI/CD.
8. Let a client discover the options a running gateway supports, apply those options, and learn new options when a source is connected, without a client change.
9. Give the Pi a Qt/QML frontend that shows connected devices and can enable or disable simulated devices through the Gateway API.
10. Show the first real CAN device in that same dashboard when the bare-metal controller is attached to the Pi.

## 3. Target users and showcase scenarios

- **Software architects and C++ engineers:** Examine extension points, ownership, lifecycle, and testing boundaries.
- **Reviewers and prospective employers:** Try a live demonstration and inspect how features are added without changes to the core.
- **Developers integrating devices:** Build a new adapter or device provider using the published plugin contract.

### Primary demonstration

1. Start Open Gateway. With no real sensor attached, the simulated temperature device is the active source.
2. Open the React dashboard in `sample_client` and view device metadata, live values, and connection status.
3. Install/activate a new trusted shared-library plugin while the gateway keeps running.
4. The gateway loads the plugin, injects its dependencies, and registers any new capabilities or devices.
5. The sample client discovers the changes and begins displaying live data without a frontend build or deployment.
6. Disable the plugin or simulate a connection failure and observe explicit status changes.
7. The client reads the gateway's support description and shows the retrieval methods and installed sources this gateway offers.
8. When a new source becomes available, the already-open client shows the new configuration options and can apply them. No client build is required.

### Edge demonstration

1. Run the same gateway on a Raspberry Pi 4. The image is built by the separate Yocto product, which includes this source, and the panel is the 7-inch display.
2. The Qt/QML frontend, using the Qt from that image, starts on that display and lists the devices the gateway reports as connected. In this product’s Docker deployment, the same UI is open in a browser through noVNC.
3. From that screen, enable and disable simulated devices. The dashboard updates while it stays open.
4. Attach the bare-metal controller over CAN. The controller becomes the connected device in the Qt dashboard through the same API, without a CAN-specific page, and the simulated sensor is inactive. Detach the controller and the simulated sensor is used again.

The physical controller and the 7-inch panel are not required for the Azure demonstration. Azure runs the gateway and serves `sample_client`.

## 4. Product scope

### Initial release (MVP)

- Headless C++23 gateway executable for Linux.
- Defined plugin contract and runtime loading of trusted `.so` libraries.
- Plugin lifecycle management and dependency injection.
- A normalized device/capability model and live telemetry pipeline.
- At least one realistic simulated device, plus an example independently packaged plugin that can be added while the gateway runs.
- REST API for discovery, metadata, current values, and plugin status.
- A versioned support description of the export methods and installed sources this running gateway offers, including which are enabled and which settings each one accepts.
- Configuration of advertised options: enable or disable an export method, and activate, deactivate, or configure an installed source. The client shall not upload or install a native plugin.
- A live update mechanism for telemetry, registry changes, and support changes (e.g., WebSocket or SSE).
- A React sample client in `sample_client`. It is a consumer of the Gateway API: cards, tables, and time-series views come from capability metadata, and configuration controls come from the support description. This is the client now developed under `web/dashboard`.
- A Qt/QML frontend for the Raspberry Pi 4. This repository ships the application sources. The separate Yocto product includes those sources and builds them with the Qt from its image, for the 7-inch display, in front of the Gateway API. A Docker deployment in this product exposes the same UI through noVNC for a browser. It lists connected devices and, as its first controls, enables and disables advertised simulated devices.
- The same headless gateway, deployable on Azure and on the Pi. Docker-based local demo, automated tests, and CI.
- A CAN source on the Pi for the bare-metal motor controller, shown as a connected device by the Qt dashboard and by `sample_client`. The Azure image does not include that CAN interface.

### Follow-on capabilities

- Further export methods the gateway can advertise and a client can enable, in particular MQTT and gRPC/Protobuf, beside REST and the live channel.
- MQTT source adapter and one genuinely different transport adapter (e.g., gRPC/Protobuf).
- A broader CANopen or motion-control surface beyond the first bare-metal controller.
- More advanced simulation, fault injection, data replay, persistence, and alert rules.
- Plugin hot replacement/unloading, where lifecycle and ownership can be managed safely.
- Optional specialized frontend extensions for visualizations that cannot be expressed using generic UI metadata.

### Out of scope for the MVP

- Developing or controlling the STM32 motor firmware.
- Full CANopen or CiA 402 implementation.
- An unrestricted marketplace or arbitrary public upload of native executable plugins.
- High-availability distributed clustering, multi-tenancy, or enterprise identity infrastructure.
- Arbitrary frontend code supplied by backend plugins.
- Safety-critical remote control of physical actuators.

## 5. Functional requirements

### FR-01 — Runtime plugin discovery and activation

The gateway shall load compatible plugins from a configured local directory or explicit path **without restarting the gateway process**. Each plugin shall expose identity, version, compatibility information, and declared capabilities. Loading or activation failures shall be reported without terminating the gateway.

**Acceptance:** During a running demo, a newly deployed example `.so` is activated and becomes visible in the plugin status API; existing sources continue operating.

### FR-02 — Dependency injection and isolation

The gateway core shall depend on defined interfaces/contracts, not concrete MQTT, CANopen, gRPC, or simulator implementations. The host shall create or obtain plugin instances and provide required host services through controlled injection points. Plugin implementation code must not require changes to the gateway core.

**Acceptance:** A new example plugin is added through the documented extension mechanism without modifying or recompiling the gateway core.

### FR-03 — Plugin lifecycle

The gateway shall support loading, initialization, activation, deactivation, and controlled shutdown with observable lifecycle states. It shall handle failed initialization and invalid plugin versions gracefully. Initial releases may retain loaded libraries until process exit; true unload/hot replacement is a later feature.

**Acceptance:** A plugin can be activated and deactivated and its state is visible; stopping a plugin stops its telemetry without stopping the gateway.

### FR-04 — Normalized device and capability registry

Plugins shall register discovered devices and/or capabilities using a common model independent of transport. Descriptors shall support stable identifiers, display names, data types, units, timestamps, status, and optional presentation hints. The registry shall support device arrival, disappearance, and metadata changes.

**Acceptance:** Devices from different plugins are represented consistently through the same discovery API.

### FR-05 — Telemetry processing

The gateway shall accept device readings, validate basic structure, associate readings with device/capability IDs, record current values, and distribute updates to consumers. Invalid or stale values shall not be silently treated as healthy current readings.

**Acceptance:** Live measurements from two independent sources are visible through one API, including timestamp and source health information.

### FR-06 — Standard external API

The gateway shall provide versioned API endpoints for device discovery, capability metadata, current values, plugin state, and health. It shall offer a real-time update channel for values and registry changes. The contract shall not expose protocol-specific details as a requirement for ordinary UI rendering.

**Acceptance:** A generic client can discover and display new plugin capabilities using only published API contracts.

### FR-07 — Sample React dashboard

`sample_client` shall contain a React dashboard that consumes the Gateway API. It shall dynamically render supported measurement types (e.g., numeric values, booleans, enumerated states) using reusable components. It shall show units, labels, connection/health status, and suitable standard visualizations. It shall react to devices being added, changed, or removed while open. It shall not be built into the gateway process.

**Acceptance:** Activating a plugin results in a newly visible device or measurement in an already-open `sample_client` session, with no frontend code change or redeployment. The same client can run against the gateway on a developer machine, on Azure, or on the Pi.

### FR-08 — Simulation

The solution shall include deterministic or configurable simulated data sources, usable without physical devices. At least one scenario shall show changing values, disconnection/reconnection, and an out-of-range or fault condition. Simulation shall run in containers.

**Acceptance:** An end-to-end demo works in Docker and in the chosen Azure deployment without attaching hardware.

### FR-09 — Diagnostics and observability

The gateway shall expose structured logs, health information, plugin lifecycle state, active devices, and basic telemetry counters. A device or plugin failure shall be distinguishable from an overall gateway failure.

**Acceptance:** Reviewers can identify a disconnected sensor, invalid plugin, and active data stream from logs/API/UI.

### FR-10 — Extensibility documentation

The repository shall document the plugin contract, capability schema, ownership and lifecycle rules, and how to implement a new plugin. It shall include an independently buildable example plugin and architecture diagrams.

**Acceptance:** A developer can create and deploy an additional demonstration plugin following repository guidance without changing gateway core files.

### FR-11 — Read-only by default

The MVP shall focus on monitoring. If commands are introduced later, command capabilities shall be explicitly declared, validated and authorized by the gateway, with safety constraints enforced independently of UI controls.

**Acceptance:** A generic UI does not infer that a writable command exists merely because a capability has a numeric value.

### FR-12 — Support discovery

The gateway shall publish a versioned support description of what this running instance offers. The description shall list export methods and installed sources. Each entry shall include its identity, whether it is enabled, and the settings it accepts. The description shall be readable with the other discovery endpoints, and changes shall be pushed on the same real-time channel as registry updates.

A gateway shall advertise only methods and sources it actually contains. Connecting a new source, or loading an installed plugin, shall update the description and inform connected clients of the new options. Removing a source shall remove its options. Each installed source in the description shall say whether it is simulated.

**Acceptance:** A generic client, started against a gateway that advertises only REST, the live channel, and the temperature source, shows those options. After a motor source is connected, the same open client shows the motor source and can act on it, with no client change. A build without MQTT does not offer an MQTT switch.

### FR-13 — Configuration of advertised options

The client shall be able to change options that appear in the support description. It may enable or disable an advertised export method and submit that method's declared settings. It may activate, deactivate, and configure an advertised installed source.

The client shall not add an option the gateway did not advertise. Installing a new native plugin remains a deployment action: a new image, or a trusted library placed where the gateway loads plugins. After the gateway has that plugin, it advertises the plugin and the client may activate it through FR-13.

**Acceptance:** From the client, a reviewer can turn an advertised export on or off and can activate an installed source that was inactive. An attempt to name a source or export absent from the support description is rejected. The public client still cannot upload a native plugin.

### FR-14 — Qt/QML edge frontend

The Raspberry Pi 4 shall run a Qt/QML application on its 7-inch display. The Yocto image and U-Boot for that board belong to a separate product. That product includes this source and shall build the application with the Qt provided by its image. This repository shall not contain that Yocto layer. The application is the local frontend and sits in front of the Gateway API: every device it shows, and every simulated device it enables or disables, comes from the API. It shall not link the gateway core, load a plugin, or talk to CAN itself.

A Docker deployment of the same Qt UI shall expose it through noVNC, so the screen can be viewed in a browser. The gateway process stays separate from that UI.

The screen shall include a dashboard of connected devices: identity, display name, and connection status, updating while the application stays open. As a start, the same screen shall enable and disable simulated devices the gateway advertises. Controls exist only for sources in the support description.

The layout shall be usable on a 7-inch touch display, including the official Raspberry Pi 7-inch panel at 800×480. The gateway process itself stays headless and keeps running if the Qt application is closed.

**Acceptance:** With no real sensor attached, the Qt screen lists the simulated device and its connection status, on the Pi panel and in a browser through this product’s Docker noVNC view. Enabling a simulated device that was off makes it connected on that screen. Disabling it shows it disconnected. When a real sensor is connected, that sensor is the one listed, as in FR-16. No plugin file can be chosen from the screen. The Pi image is produced by the separate Yocto product and uses the Qt from that image.

### FR-15 — First physical device over CAN

The first real device is the separately developed STM32F404 bare-metal motor controller, connected to the Pi over CAN. A source adapter on the Pi shall publish that controller through the same device and telemetry model as the simulators. The Qt dashboard and `sample_client` shall show it as a connected device when the bus is up, and as disconnected when it is not, without a page written only for that controller.

The Azure deployment shall not require a CAN interface. The controller firmware is developed outside this repository.

**Acceptance:** On the Pi, with the controller attached, the Qt connected-device dashboard shows that controller and the simulated sensor is inactive. Detaching the controller makes the simulated sensor the active source. The Azure showcase still runs with the simulated sensor.

### FR-16 — Real sensors before simulated sensors

The gateway shall try to detect real sensors before it uses a simulated one. At startup it shall probe each configured real source. A real source whose hardware is absent shall remain inactive, and that absence shall be distinct from a plugin failure.

When at least one real sensor is connected, the clients shall show that sensor by default. Simulated sources shall stay inactive while a real sensor is connected. When the real sensor disconnects, the gateway shall use the configured simulated sensor again.

When no real sensor is found, the gateway shall activate the configured simulated sensor, and the clients shall show it. A simulated source that is not that fallback shall stay inactive until an operator activates it through FR-13. While no real sensor is connected, an operator can still enable or disable an advertised simulated source from the support panel.

On a developer machine where the gateway container cannot see a USB CAN adapter, `sample_client` shall detect that adapter through the browser and treat the open adapter as the real sensor. While it is open, the dashboard shows its measurements and the simulated sensor is inactive.

**Acceptance:** With no CAN adapter and no CAN interface, the dashboard shows the temperature simulator, and the CAN source is inactive with an empty error. With the bare-metal controller reachable, the dashboard shows that controller and the temperature simulator is inactive. Disconnecting the controller returns the temperature simulator. The Azure showcase, which has no real sensor, shows the simulator.

## 6. Key domain concepts

- **Plugin:** Deployable binary extension implementing a defined host contract.
- **Source adapter:** Receives data through a transport/protocol (MQTT, CANopen, gRPC, replay, etc.).
- **Device provider:** Exposes devices and their capabilities; a single source adapter may expose multiple devices.
- **Device:** A logical telemetry producer or monitored system.
- **Capability:** A named observable property or, in a later release, an explicitly permitted command.
- **Capability registry:** Gateway-owned record of available devices and capabilities.
- **Telemetry event:** A timestamped value associated with a device and capability.
- **Export method:** A way for a client to retrieve data from the gateway, such as REST, the live channel, and later MQTT or gRPC. The gateway implements the method. The client enables only methods the gateway advertises.
- **Support description:** The gateway-owned list of export methods and installed sources, their enabled state, and the settings each one accepts. The client treats this list as the menu of configuration options.
- **Edge frontend:** The Qt/QML application on the Raspberry Pi 4. This repository contains its sources. The separate Yocto product builds them with that image’s Qt for the panel. In this product’s Docker deployment the same UI is reached in a browser through noVNC.
- **Sample client:** The React dashboard in `sample_client`. It is an example consumer of the Gateway API, not the Pi display.
- **Real source:** A plugin that reads a physical sensor. The gateway probes it first.
- **Simulated source:** A plugin that produces telemetry without hardware. The configured simulated sensor is the fallback used when no real sensor is connected.

Transport handling and device semantics shall remain separable; one transport must not imply one fixed device type or UI. A client shall not hard-code the methods or sources of a particular gateway build.

### Example capability descriptor (illustrative, not a finalized API)

```json
{
  "deviceId": "motor-sim-01",
  "displayName": "Simulated Motor",
  "capabilities": [
    {
      "id": "speed",
      "kind": "measurement",
      "valueType": "number",
      "unit": "rpm",
      "presentation": "gauge"
    },
    {
      "id": "state",
      "kind": "measurement",
      "valueType": "enum",
      "presentation": "status"
    }
  ]
}
```

Presentation hints are optional. The UI shall provide sensible fallbacks for unknown or absent hints.

### Example support description (illustrative, not a finalized API)

```json
{
  "exports": [
    { "id": "rest", "enabled": true, "settings": [] },
    { "id": "websocket", "enabled": true, "settings": [] },
    {
      "id": "mqtt",
      "enabled": false,
      "settings": [{ "name": "port", "valueType": "number" }]
    }
  ],
  "sources": [
    { "id": "can-motor", "state": "inactive", "simulated": false, "settings": [] },
    { "id": "temperature-sim", "state": "active", "simulated": true, "settings": [] },
    { "id": "motor-sim", "state": "inactive", "simulated": true, "settings": [] }
  ]
}
```

A client reads this description, for example from `GET /api/v1/support`, and applies a later live event when the gateway changes it. The `mqtt` entry is present only on a gateway that contains MQTT. After a new source connects, the gateway adds that source to `sources` and notifies clients. The client then shows the new settings and the new activation control. In the example above no real sensor was found, so `can-motor` is inactive and `temperature-sim` is the active source. `motor-sim` stays inactive until an operator enables it.

## 7. Non-functional requirements

- **Maintainability:** Clear domain boundaries; protocol-specific dependencies confined to adapters/plugins.
- **Compatibility:** A documented plugin contract with version negotiation/compatibility checks. Native C++ ABI risks and module ownership must be addressed in the design; a C-compatible binary boundary is an option, not a predetermined implementation.
- **Resource safety:** RAII, explicit ownership, safe plugin lifetime, and clean shutdown. A loaded library must outlive code and objects executing from it.
- **Concurrency:** Thread-safe handoff between plugin callbacks, processing, and external consumers; bounded queues or equivalent backpressure handling; no unbounded memory growth under sustained overload.
- **Reliability:** Plugin initialization and transport failures must not silently corrupt gateway state; failures shall be observable and recoverable where feasible.
- **Testability:** Core behavior testable without network or hardware; unit tests and end-to-end container-based integration tests.
- **Portability:** Linux-first deployment, including Azure and Raspberry Pi 4 (aarch64). Hardware-dependent components stay in plugins. The Azure image and the local Docker demo must not depend on SocketCAN, a CAN hat, or the 7-inch panel. The Pi image is built by the separate Yocto product, which uses its own Qt. In this product’s Docker deployment, the Qt UI is shown through noVNC and does not need the panel.
- **Security:** Native plugins execute trusted code and shall be installed only through controlled deployment mechanisms. A public client may read the support description and may change only advertised export settings and advertised installed sources. Public access must not permit arbitrary native plugin installation or upload. Cloud-exposed interfaces require appropriate access controls and transport security.
- **Deployment:** Local development of the gateway should be Docker-first. Keep Docker-specific assets under `docker/` where practical. Use GitHub Actions for repeatable build/test workflows. The Yocto layer and U-Boot are not part of this repository; the separate board product includes this source and builds it there. A Docker deployment of the Qt UI in this product exposes it through noVNC. In both cases the UI talks to the gateway over the local API and is not part of the gateway process.
- **Performance:** Suitable for an interactive multi-device demo; specific throughput and latency targets to be established during performance testing, not guessed up front.

## 8. Deployment expectations

**Local:** Docker Compose should start the gateway, the `sample_client` dashboard, and any required local infrastructure with minimal setup. The gateway probes for a real sensor and uses the temperature simulator when none is found. Where the container cannot see a USB CAN adapter, the sample client opens that adapter in the browser and shows it in place of the simulator. The Docker deployment of the Qt UI is exposed through noVNC so it can be opened in a browser.

**Cloud:** Deploy a fully functional simulated system on Azure. Azure Container Apps is a candidate, not a mandatory architecture choice. The gateway and simulators shall be deployable without a CAN interface and without a display. The public site serves `sample_client`. That client may retrieve data and may change advertised export methods and advertised installed sources. It must not be able to upload a native plugin. The showcase shall not require a separate telemetry database; history kept in gateway memory is sufficient.

**Edge:** The Raspberry Pi image is built by the separate Yocto and U-Boot product, which includes this source. The Qt/QML frontend uses the Qt from that image and is shown on the 7-inch display. The screen lists the connected device: the bare-metal controller when CAN is up, and the simulated sensor when it is not. Simulated devices can still be enabled and disabled from the screen while no real sensor is connected. CAN for the bare-metal controller is attached only on this Pi. Closing the Qt application does not stop the gateway.

## 9. Demo success criteria

The project is ready to showcase when all of these can be demonstrated:

1. With no real sensor attached, the gateway operates with the simulated temperature sensor. With a real sensor attached, that sensor is shown and the simulated sensor is inactive.
2. A second trusted `.so` can be activated **at runtime**, without a gateway process restart.
3. Plugin activation introduces new capabilities/devices without changing gateway core code.
4. An already-open dashboard renders new devices or measurements without a frontend build/redeploy.
5. The core, the Qt frontend, and `sample_client` operate without knowing whether telemetry comes from simulation, CAN, or a future transport.
6. Simulated faults and disconnections appear as explicit statuses rather than deceptively normal values.
7. Automated tests and a repeatable container-based demo are available.
8. A reviewer can follow an architecture diagram and a documented “add a plugin” tutorial.
9. An already-open client lists only the export methods and sources the gateway advertises, and shows a new configuration option after a new source is connected, without a client change.
10. On a Raspberry Pi 4, the separate Yocto product’s build of this Qt frontend lists connected devices on the 7-inch display and can enable and disable simulated devices through the Gateway API. This product’s Docker deployment of that UI shows the same screen in a browser through noVNC.
11. With the bare-metal controller attached over CAN, that controller is the connected device on the Qt dashboard and in `sample_client`, and the simulated sensor is inactive. Detaching the controller makes the simulated sensor active again.

## 10. Recommended delivery milestones

| Milestone | Outcome |
| --- | --- |
| **M1 — Core contracts** | Device model, capability registry, telemetry path, plugin contract, unit tests |
| **M2 — Runtime extensions** | Plugin loader, lifecycle, DI, independently built example `.so`, runtime activation |
| **M3 — Generic API and sample client** | Discovery API, support description, live updates, `sample_client` React dashboard that adapts its display and configuration options automatically |
| **M4 — Realistic demo** | Sensor/motor simulation, fault injection, logs, integration tests |
| **M5 — Azure and Pi** | Docker Compose, GitHub Actions, Azure demo with `sample_client`, and a Docker deployment of the Qt UI through noVNC. The Pi image is built by the separate Yocto product from this source |
| **M6 — CAN device** | CAN source on the Pi for the bare-metal controller, visible in the Qt dashboard and in `sample_client` |
| **M7 — Optional integrations** | MQTT/gRPC exports and source adapters, persistence, broader CANopen behavior, advanced visualizations |

## 11. Open design decisions

These decisions should be made during architecture/design, not frozen by the PRD:

1. Plugin ABI and host/plugin ownership contract (C ABI, controlled C++ ABI, or another approach).
2. Runtime discovery mechanism (explicit reload/API trigger, directory watching, or both).
3. Whether input and output adapters share one plugin contract or have separate extension interfaces.
4. Choice of HTTP framework and real-time update mechanism.
5. Internal event-processing model and threading strategy.
6. Whether history stays in memory beyond the showcase, and which later transports to add after CAN.
7. Azure deployment topology and minimal security model for the public showcase.
8. Which CAN interface the Pi uses. The choice of Qt/QML for the edge screen, React for `sample_client`, and noVNC for this product’s Docker deployment of that UI, is already made. Yocto and U-Boot belong to the separate board product, which includes this source and uses that image’s Qt.

## 12. Guiding principle

> **New integrations should extend Open Gateway without requiring changes to its core. New standard device capabilities, and new configuration options the gateway advertises, should appear in the Qt edge frontend and in `sample_client` without requiring client code changes.**

The priority is to demonstrate architectural quality and end-to-end extensibility—not to maximize the number of protocols or dashboard features.
