# Raspberry Pi 4

The gateway stays headless. The 7-inch panel runs `edge/display`, a Qt/QML application that reads the Gateway API and enables or disables advertised sources. Closing that screen does not stop the gateway.

The Yocto image and U-Boot for that board are a separate product. That product includes this source and builds `edge/display` with the Qt from its image. This repository does not contain that layer. `OGW_API` should point at the gateway on the board, and the window is 800×480.

In this product, the same Qt UI runs in Docker and is shown in a browser through noVNC:

```bash
docker compose -f docker/compose.yaml up --build
```

Open http://127.0.0.1:6080/vnc.html?autoconnect=1&resize=scale. The page is the 800×480 screen. `OGW_API` inside that container is `http://gateway:8080`.

`can0` is the SocketCAN interface for the bare-metal controller. The gateway probes that real source first. The status frame is `0x101`: encoder counts in the first four bytes, duty in byte 4, and direction in byte 5 (`0` is CW). When the controller is connected, it is the device on screen and the temperature simulator is inactive. When `can0` is missing, the CAN source stays inactive with an empty error and the temperature simulator runs. The controller firmware is not in this repository.

The Azure image does not include this display or the CAN plugin.
