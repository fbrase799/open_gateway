= Collected Project Notes

*Name*

open_gateway

*Purpose*

- a standalone, extensible data gateway that connects data sources to external consumers
- demonstrate modern C++ software architecture through runtime-loadable plugins, dependency injection, clear separation of concerns, and metadata-driven clients
- offer an OpenAPI specification how consumers can fetch data

*Requirements*

1. The UI shall be written in Qt/Qml for a 7' Raspi Touch Display 2 in Landscape mode with 1280x720
2. The gateway shall support as a real sensor `Motor Control`
```
- connected to device via `DSD TECH SH-C31A` USB to CAN adapter with CAN FD, based on CANable 2.0
- a motor is controlled with an STM32F303 and the status is reported on CAN
```
3. The gateway shall support a simulated sensor for temperature
4. The gateway shall support a simulated sensor for `Motor Control` when no hardware is connected
5. The UI shall show a List with all available sensors - real sensors shall be enabled by default when plugged in or greyed out when not plugged in, simulated sensors shall be disabled by default
6. The UI shall display enabled devices in a dashboard and their current values
7. `docker compose` on a local maschine shall open the Main Window as a Desktop application
8. A deployment in Azure shall open the UI via noVNC
9. There shall be an option e.g. environment variable in compose.yml to decide whether to use a Desktop application or noVNC
10. A trusted .so can be installed and activated while the gateway is running
11. Techniques to provide data to consumers shall include MQTT and REST, so they should appear in the OpenAPI spec
12. A configuration toml file shall be used - if the file does not exist it shall be created with useful default values

*Known Limitations*

1. Docker Desktop in MacOS it will provides a linux kernel with no support for CAN - hence CAN will never work as a real target