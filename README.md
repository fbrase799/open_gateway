# Open Gateway

Open Gateway is a headless C++23 gateway that loads trusted plugins at runtime. The only UI is the Qt screen in `edge/display`, laid out for the Raspberry Pi Touch Display 2 at 1280×720. Local Compose opens that window on the developer machine. `OGW_UI=novnc` shows the same window in a browser. Azure uses noVNC. Consumers read the same data over REST and MQTT, both described in `api/openapi.yaml`.

Simulated sensors stay off until a person enables them. A real sensor that is not plugged in is listed and greyed out. The dashboard shows only enabled devices.

## Run the demo

Docker is the only setup requirement.

```bash
docker compose -f docker/compose.yaml up --build
```

The API is at http://127.0.0.1:8090. With no hardware connected, the sensor list shows Motor Control greyed out and the simulators disabled. Enable Temperature in the list and the room sensor appears on the dashboard.

Desktop mode (`OGW_UI=desktop`, the Compose default) opens a 1280×720 window on the developer machine. On WSL the container uses the WSLg socket at `/tmp/.X11-unix`, so Compose can start directly. On a Mac the container reaches XQuartz over TCP and presents an MIT-MAGIC-COOKIE. Start XQuartz, then refresh the cookie before Compose:

```bash
./docker/xquartz-xauth.sh
docker compose -f docker/compose.yaml up --build
```

The script turns on XQuartz TCP (`nolisten_tcp` false) and leaves `no_auth` false, so a client without the cookie is rejected. Restart XQuartz once after that preference change. The container does not install an X server on the Mac. If that display is not available, start the browser session instead:

```bash
OGW_UI=novnc docker compose -f docker/compose.yaml up --build
```

Open http://127.0.0.1:6080/vnc.html?autoconnect=1&resize=scale.

Admin scan stays on `127.0.0.1:8081`. The cloud profile and the separate Pi product leave that listener off. The later embedded-Linux project includes this source and starts the same Qt window fullscreen on the panel. See [docs/pi.md](docs/pi.md).

## Tests

```bash
docker compose -f docker/compose.yaml run --rm unit-tests
docker compose -f docker/compose.yaml -f docker/compose.test.yaml up --build --abort-on-container-exit --exit-code-from live-tests
```

## Layout

| Path | Role |
| --- | --- |
| `include/ogw`, `src/core` | Device model, registry, telemetry. No sockets and no `dlopen`. |
| `src/host` | Plugin loader, lifecycle, configuration. |
| `src/api` | REST, WebSocket, and the MQTT publisher. |
| `sdk/` | C ABI and the C++ helpers plugin authors include. |
| `plugins/temperature_sim` | Temperature simulator. Disabled until enabled. |
| `plugins/motor_sim` | Motor Control simulator. Disabled until enabled. |
| `plugins/can_motor` | SocketCAN Motor Control for the bare-metal controller. |
| `edge/display` | Qt/QML sensor list and dashboard at 1280×720. |
| `api/openapi.yaml` | REST paths and MQTT topics. |
| `docker/` | Toolchain, gateway, display, broker, and Compose files. |

See [docs/architecture.md](docs/architecture.md) and [docs/add-a-plugin.md](docs/add-a-plugin.md). Cloud deployment is described in [docs/azure.md](docs/azure.md). The product requirements are in [PRD.md](PRD.md) and the build sequence is in [PLAN.md](PLAN.md).
