# Open Gateway

Open Gateway is a headless C++23 gateway that loads trusted plugins at runtime. `sample_client` is the React dashboard. The same API is shown by a Qt screen: in Docker through noVNC, and on a Raspberry Pi by a separate Yocto product that includes this source and uses that image’s Qt. The core does not know whether a reading came from a simulator or CAN. A real sensor is shown when one is connected. The temperature simulator runs when none is.

## Run the demo

Docker is the only setup requirement.

```bash
docker compose -f docker/compose.yaml up --build
```

Open http://127.0.0.1:8080. With no real sensor connected, the room sensor is publishing. The Qt screen is at http://127.0.0.1:6080/vnc.html?autoconnect=1&resize=scale. The support panel lists REST, the live channel, and the installed sources. In another terminal, install the motor plugin without restarting the gateway:

```bash
./scripts/demo-activate-motor.sh
```

The script copies the library and scans. Enable Motor in the support panel. The open dashboard adds the motor card. Disable it from the same panel.

Admin scan is bound for local use on `127.0.0.1:8081`. The cloud profile and the separate Pi product leave that listener off. How the Pi image includes this source is in [docs/pi.md](docs/pi.md).

Simulators walk a timeline of normal readings, a fault, and a disconnect. Stale or bad quality stays visible as a status; the last number is not presented as live.

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
| `sdk/` | C ABI and the C++ helpers plugin authors include. |
| `plugins/temperature_sim` | Simulator shipped with the gateway. |
| `plugins/motor_sim` | Standalone CMake project. It links only the SDK. |
| `sample_client` | React sample dashboard. |
| `edge/display` | Qt/QML sources. Docker shows them through noVNC. The separate Yocto product builds them for the panel. |
| `plugins/can_motor` | SocketCAN plugin for the bare-metal controller. Probed before the simulator. |
| `docker/` | Toolchain, gateway, web, and Compose files. |

See [docs/architecture.md](docs/architecture.md) and [docs/add-a-plugin.md](docs/add-a-plugin.md). Cloud deployment is described in [docs/azure.md](docs/azure.md). The product requirements are in [PRD.md](PRD.md) and the build sequence is in [PLAN.md](PLAN.md).
