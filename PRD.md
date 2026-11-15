# Open Gateway — Product Requirements Document

**Project:** Open Gateway  
**Repository:** `open-gateway`  
**Status:** Aligned with `PROJECT_BRIEF.md`  
**Primary language:** Modern C++ (C++23) for the gateway. Qt/QML for the UI.

## 1. Product vision

Open Gateway is a standalone, extensible data gateway that connects data sources to external consumers. It demonstrates modern C++ software architecture through **runtime-loadable plugins**, **dependency injection**, **clear separation of concerns**, and **metadata-driven clients**.

Consumers fetch data through **MQTT** and **REST**. An **OpenAPI** specification describes both.

The UI is one **Qt/QML** application, written for the **Raspberry Pi Touch Display 2** in landscape at **1280×720**. The gateway process does not draw that screen. The Qt application is the screen, and it talks to the gateway.

This repository develops that screen at 1280×720. On a developer machine, `docker compose` opens a display session of that same size, and the main window fills it. A setting in the Compose file, for example an environment variable, chooses that desktop session or noVNC. Azure opens the same UI through noVNC.

The Yocto image and U-Boot are not part of this repository. A later embedded-Linux project provides that build and includes this application. That image sets the display to this size. When the process starts there, it covers the whole Raspberry Pi screen.

The real sensor is **Motor Control**. A **DSD TECH SH-C31A** USB-to-CAN adapter (CAN FD, based on CANable 2.0) connects the gateway to an **STM32F303** that controls the motor and reports status on CAN. The controller firmware is not part of this repository. A simulated Motor Control sensor is available when no hardware is connected. A simulated temperature sensor is also available. Simulated sensors are disabled by default.

A trusted `.so` can be installed and activated while the gateway is running.

## 2. Goals

1. Demonstrate extensible architecture using dependency inversion and dependency injection.
2. Load, activate, and inspect a trusted plugin while the gateway keeps running.
3. Expose a device and telemetry model that the Qt UI can render from metadata.
4. Publish an OpenAPI specification for how consumers fetch data, covering MQTT and REST.
5. Develop the Qt UI at the Raspberry Pi Touch Display 2 size, 1280×720 landscape, and fill that display.
6. Offer a sensor list of every available device, where the user enables or disables each one. A real sensor is enabled when it is plugged in and greyed out when it is not. Simulated sensors start disabled. The dashboard shows only enabled devices and their current values.
7. Open that UI from local Docker Compose as a desktop display of the same size, and on Azure through noVNC, with a Compose setting to choose which one.

## 3. Target users and showcase scenarios

- **Software architects and C++ engineers:** Examine extension points, ownership, lifecycle, and testing boundaries.
- **Reviewers and prospective employers:** Try the dashboard at the panel size and see a plugin appear without a gateway restart.
- **Developers integrating devices:** Add a source through the published plugin contract.
- **External consumers:** Fetch data over MQTT or REST as described by the OpenAPI specification.

### Local demonstration

1. `docker compose` starts the gateway and opens the Qt main window in a display session on the developer machine. The session is 1280×720 landscape, the same size as the Raspberry Pi Touch Display 2, and the window fills it.
2. A sensor list shows every available device. With no hardware connected, Motor Control is greyed out. The temperature simulator and the Motor Control simulator are listed and disabled. The dashboard is empty of those devices.
3. Enable a simulated sensor from the list. It appears on the dashboard with its current values. Disable it and it leaves the dashboard.
4. Install and activate a trusted `.so` while the gateway keeps running. The new device appears in the sensor list.
5. Switch the Compose setting to noVNC and open the same 1280×720 window in a browser.

### Azure demonstration

1. The deployment runs the gateway and the simulated sensors. It does not require the CAN adapter or the Pi panel.
2. The Qt UI opens in a browser through noVNC at 1280×720.
3. Consumers can fetch data over MQTT and REST as described by the OpenAPI specification.

### Later, on the Raspberry Pi

The embedded-Linux project includes this application in its Yocto image and sets the display to 1280×720 landscape. Starting the process covers the whole Raspberry Pi screen. With the SH-C31A and the STM32F303 connected, Motor Control is enabled and its CAN status is on the dashboard. The gateway can control the motor. Unplugged, that sensor is greyed out in the list and is not on the dashboard.

## 4. Product scope

### This repository

- Headless C++23 gateway executable for Linux.
- Plugin contract, dependency injection, and runtime loading of a trusted `.so` without restarting the gateway.
- Normalized device and telemetry model.
- Qt/QML UI for the Raspberry Pi Touch Display 2, landscape, 1280×720. The window fills that display.
- A sensor list of every available device, used to enable or disable each one. Real sensors are enabled when plugged in and greyed out when not. Simulated sensors are disabled by default.
- A dashboard that shows only enabled devices and their current values.
- Real Motor Control source: DSD TECH SH-C31A, CAN FD, CANable 2.0, STM32F303. The gateway receives status on CAN and can control the motor.
- Simulated temperature sensor.
- Simulated Motor Control sensor for when no hardware is connected.
- OpenAPI specification that includes MQTT and REST as the ways consumers fetch data.
- Docker Compose local demo. The desktop display session matches 1280×720. A Compose setting selects the desktop session or noVNC.
- Azure deployment of the same UI through noVNC.

### Later, outside this repository

- The embedded-Linux project’s Yocto build and U-Boot. That project includes this application, sets the display size to 1280×720 landscape, and starts the UI so it covers the whole Raspberry Pi screen.
- The STM32F303 firmware.

### Follow-on

- Richer fault and staleness presentation beyond the greyed-out state.
- Opening the SH-C31A from a browser when the gateway container cannot see the USB adapter.
- Further consumer techniques beyond MQTT and REST.
- Persistence, plugin unload, and a broader CAN surface than this motor controller.

### Out of scope

- A React client, or any second dashboard.
- Yocto, U-Boot, and the embedded Linux image in this repository.
- Developing the STM32 firmware in this repository.
- Full CANopen or CiA 402.
- A public upload path for native plugins.
- High-availability clustering, multi-tenancy, or enterprise identity.
- Frontend code supplied by plugins.

## 5. Functional requirements

### FR-01 — Runtime plugin activation

The gateway shall load a compatible trusted plugin from a configured local directory or an explicit path **without restarting the gateway process**. Each plugin shall expose identity, version, compatibility information, and declared capabilities. A load or activation failure shall be reported without terminating the gateway.

**Acceptance:** During a running demo, a newly deployed `.so` is activated and becomes visible. Existing sources keep operating.

### FR-02 — Dependency injection and isolation

The gateway core shall depend on interfaces, not on a concrete CAN, MQTT, REST, or simulator implementation. The host shall provide plugin services through controlled injection points. Adding a plugin shall not require a change to the gateway core.

**Acceptance:** An example plugin is added through the documented extension mechanism without modifying or recompiling the gateway core.

### FR-03 — Qt/QML screen at the panel size

The UI shall be Qt/QML, laid out for the Raspberry Pi Touch Display 2 in landscape at 1280×720. The main window shall fill that display.

On the Raspberry Pi, once the later embedded-Linux image has set the display to this size, starting the process shall cover the whole screen. This repository does not contain that image.

The Qt application shall not link the gateway core, load plugins, or talk to CAN itself. Closing it shall leave the gateway running.

**Acceptance:** The window is 1280×720 and fills the display session it is given. On the Pi image, it covers the panel.

### FR-04 — Sensor list and dashboard

The UI shall show a list of all available sensors. From that list the user shall enable or disable a device.

A real sensor shall be enabled by default when it is plugged in, and greyed out when it is not. A simulated sensor shall be disabled by default.

The dashboard shall display enabled devices and their current values. A disabled or greyed-out device shall not appear on the dashboard.

**Acceptance:** With the controller connected, Motor Control is enabled in the list and its values are on the dashboard. With nothing plugged in, Motor Control is greyed out in the list, and the temperature simulator and the Motor Control simulator are listed and disabled. None of those three appear on the dashboard until one is enabled. Enabling a simulator puts it on the dashboard with its values. Disabling it removes it from the dashboard.

### FR-05 — Motor Control

The gateway shall support Motor Control as a real sensor.

The sensor is reached through a DSD TECH SH-C31A USB-to-CAN adapter with CAN FD, based on CANable 2.0. An STM32F303 controls the motor and reports status on CAN. The gateway shall accept that control path and shall publish the reported status through the same device model as every other sensor.

The firmware is developed outside this repository. Azure shall not require the adapter.

**Acceptance:** With the adapter and controller connected, Motor Control is enabled in the sensor list and the dashboard displays the CAN status. A control sent through the gateway is accepted on that path. With the hardware absent, the same sensor is greyed out in the list and is absent from the dashboard. That absence is distinct from a plugin failure.

### FR-06 — Simulated sensors

The gateway shall provide a simulated temperature sensor.

The gateway shall provide a simulated Motor Control sensor for when no hardware is connected.

Both are simulated sensors, so FR-04 leaves them disabled until the user enables them from the sensor list. Enabling the simulated motor shall present its values on the dashboard, without a separate page.

**Acceptance:** With no SH-C31A and no CAN interface, both simulators are in the list and disabled, and Motor Control is greyed out. The dashboard does not show them. After the simulated motor is enabled, its values appear on the dashboard.

### FR-07 — Local display and noVNC

`docker compose` on a local machine shall open the main window as a desktop application. The display session that pops up on that machine, whether an X session or another display mechanism, shall be 1280×720 landscape, matching the later Raspberry Pi display. The window shall fill that session.

A deployment on Azure shall open the UI through noVNC at that same size.

The Compose file shall offer a choice, for example an environment variable, between the desktop application and noVNC.

**Acceptance:** The default local Compose command shows the 1280×720 window on the developer machine. Changing the Compose setting and starting again serves that window through noVNC. The Azure deployment opens the UI through noVNC.

### FR-08 — OpenAPI, MQTT, and REST

The repository shall offer an OpenAPI specification that describes how consumers fetch data.

MQTT and REST shall both be ways to provide data to consumers, and both shall appear in that OpenAPI specification.

**Acceptance:** A consumer can follow the specification and fetch data over REST and over MQTT. The Qt UI is not required for that fetch.

### FR-09 — Metadata-driven UI

The Qt UI shall render sensors from the gateway’s device metadata: names, status, and values. A new plugin’s devices shall be able to appear from that metadata without a device-specific page and without a UI rebuild.

**Acceptance:** Activating a plugin that registers a device adds that device to the sensor list from the gateway’s description of it. Enabling it from the list puts it on the dashboard.

## 6. Key domain concepts

- **Plugin:** Deployable binary extension implementing a defined host contract.
- **Device:** A sensor the list can offer. Motor Control, the temperature simulator, and the simulated motor are devices. Only an enabled device is on the dashboard.
- **Real sensor:** A device backed by hardware. Motor Control is enabled when the SH-C31A and STM32F303 are present, and greyed out when they are not.
- **Simulated sensor:** A device that produces data without hardware. Simulated sensors are disabled by default. The simulated motor is the Motor Control stand-in when no hardware is connected.
- **Capability:** A named value or control on a device. A numeric status is not itself a control; control of the motor is an explicit capability of Motor Control.
- **Telemetry:** A timestamped value for a device capability.
- **Consumer technique:** MQTT or REST, as listed in the OpenAPI specification.
- **Sensor list:** The control that shows every available device and enables or disables it.
- **Dashboard:** The view of enabled devices and their current values.
- **UI:** The Qt/QML application. It contains the sensor list and the dashboard. It fills a 1280×720 landscape display.

Transport handling and device meaning stay separate. The dashboard shall not hard-code a page per sensor.

### Example device (illustrative)

```json
{
  "deviceId": "motor-control",
  "displayName": "Motor Control",
  "status": "enabled",
  "simulated": false,
  "capabilities": [
    {
      "id": "status",
      "kind": "measurement",
      "valueType": "number",
      "presentation": "gauge"
    },
    {
      "id": "command",
      "kind": "command",
      "valueType": "number"
    }
  ]
}
```

## 7. Non-functional requirements

- **Maintainability:** Protocol-specific code stays in plugins. The core depends on interfaces.
- **Compatibility:** The plugin contract is documented, including version checks. A loaded library must outlive code executing from it.
- **Resource safety:** RAII, explicit ownership, and a clean shutdown.
- **Concurrency:** Thread-safe handoff from plugin callbacks to consumers, with bounded queues.
- **Reliability:** A failed plugin or a missing CAN adapter must not take down the gateway. A missing real sensor is greyed out and is distinct from a plugin failure.
- **Display:** Layout and the local display session are 1280×720 landscape, matching the Raspberry Pi Touch Display 2. The window fills that area.
- **Portability:** Linux-first. Local development is Docker-first, including on a MacBook, where the display session is the panel size. Azure does not require the CAN adapter or the panel. The Pi image is built later by the embedded-Linux project.
- **Security:** Native plugins are trusted and are installed by deployment, not by a public upload. Cloud access to MQTT and REST uses appropriate access control and transport security.
- **Deployment:** Docker assets live under `docker/`. The Compose file selects the desktop display or noVNC.

## 8. Deployment expectations

**Local:** `docker compose` starts the gateway and opens the Qt main window in a 1280×720 landscape display session on the developer machine. The window fills that session. A Compose setting selects this desktop session or noVNC.

**Azure:** The deployment serves the Qt UI through noVNC at 1280×720. MQTT and REST are available as specified. The CAN adapter and the Pi panel are not required. Simulated sensors are present and disabled until enabled. Motor Control is greyed out.

**Raspberry Pi:** Provided later by the embedded-Linux project, which includes this application. That Yocto image sets the display to 1280×720 landscape. The process covers the whole screen. The SH-C31A is attached there when the real motor is demonstrated.

## 9. Demo success criteria

1. Local `docker compose` opens a 1280×720 landscape display on the developer machine, and the Qt window fills it.
2. A sensor list shows every available device. A plugged-in Motor Control sensor is enabled. An unplugged one is greyed out. Simulated sensors start disabled. The dashboard shows only enabled devices and their current values.
3. With no hardware, the simulated Motor Control sensor can be enabled and then shows values. The temperature simulator can be enabled on its own.
4. A trusted `.so` can be activated while the gateway is running, without a restart.
5. The OpenAPI specification describes fetching data over MQTT and over REST, and a consumer can do both.
6. The Compose setting switches the same UI to noVNC, and the Azure deployment opens it that way.
7. The window is the size of the Raspberry Pi Touch Display 2, so the later Yocto image can start this same application fullscreen on that panel.

## 10. Recommended delivery milestones

| Milestone | Outcome |
| --- | --- |
| **M1 — Core** | Device model, telemetry path, plugin contract, dependency injection |
| **M2 — Runtime plugins** | Load and activate a trusted `.so` while the gateway is running |
| **M3 — Consumers** | REST, MQTT, and the OpenAPI specification that documents both |
| **M4 — Sensor list and dashboard** | Qt/QML window at 1280×720. The list holds every sensor, with real sensors enabled or greyed out and simulators disabled by default. The dashboard shows enabled devices and their values |
| **M5 — Display sessions** | Compose opens the desktop session at 1280×720, the Compose setting selects noVNC, and Azure serves noVNC |
| **M6 — Motor Control** | SH-C31A CAN FD source for the STM32F303, with status and control, plus the simulated motor for when no hardware is connected |

The Yocto image that places this application on the Raspberry Pi screen is a milestone of the embedded-Linux project.

## 11. Open design decisions

1. Plugin ABI and the host/plugin ownership contract.
2. How a newly copied `.so` is discovered: an API trigger, a directory watch, or both.
3. HTTP library, MQTT library, and how both are represented in the OpenAPI document.
4. Threading and the internal event path.
5. Azure topology and the access controls on MQTT and REST.
6. The CAN frame map for STM32F303 status and control, agreed with the firmware that lives outside this repository.

Already decided: the only UI is Qt/QML at 1280×720 landscape; local Compose opens a matching desktop display; Azure uses noVNC; a Compose setting chooses between them; there is no React client; Yocto and U-Boot belong to the later embedded-Linux project.

## 12. Guiding principle

> **New sensors should extend Open Gateway without a change to its core, and should appear in the sensor list from metadata. The dashboard shows the ones the user has enabled. Consumers should be able to fetch data over MQTT and REST as the OpenAPI specification describes.**
