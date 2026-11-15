# Raspberry Pi 4

The gateway stays headless. The 7-inch Raspberry Pi Touch Display 2 is 1280×720 landscape. `edge/display` fills that screen. Closing the screen does not stop the gateway.

The Yocto image and U-Boot for that board are a separate product. That product includes this source, sets the panel to 1280×720, and starts this window fullscreen. This repository does not contain that layer. `OGW_API` should point at the gateway on the board.

On a developer machine, Compose opens the same 1280×720 session:

```bash
docker compose -f docker/compose.yaml up --build
```

`OGW_UI=desktop` is the default and needs the host X server. `OGW_UI=novnc` serves http://127.0.0.1:6080/vnc.html?autoconnect=1&resize=scale. `OGW_API` inside the display container is `http://gateway:8080`.

`can0` is the SocketCAN interface for the SH-C31A adapter and the STM32F303 controller. The status frame is `0x101`: encoder counts in the first four bytes, duty in byte 4, and direction in byte 5 (`0` is CW). A command on the `set_duty` capability sends frame `0x100` with the duty in the first byte. When the adapter is missing, Motor Control stays inactive with an empty error and the list row is greyed out. The simulated Motor Control and the temperature simulator stay disabled until someone enables them. The controller firmware is not in this repository.

The Azure image does not include the CAN plugin. It serves this Qt window through noVNC.
