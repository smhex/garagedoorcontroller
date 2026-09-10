# Garage Door Controller

A smart-home garage-door controller for the Arduino MKR Zero. It drives
Marantec operators through the Systembus interface, reports both end positions
and integrates through wired Ethernet and MQTT. The I/O control code can be
adapted to other door operators with suitable open, close and end-position
signals.

## Hardware

Required:

- Arduino [MKR Zero](https://docs.arduino.cc/hardware/mkr-zero)
- Arduino [MKR ETH Shield](https://docs.arduino.cc/hardware/mkr-eth-shield)
- [ArduiBox MKR](https://www.hwhardsoft.de/deutsch/projekte/arduibox-mkr/) DIN-rail enclosure

Optional:

- Arduino [MKR ENV Shield](https://docs.arduino.cc/hardware/mkr-env-shield)
- [OLED Display Shield](https://www.hwhardsoft.de/deutsch/projekte/display-shield/) from Zihatec

The ENV shield is optional at runtime. Failed initialization or invalid readings
do not stop door control. Initialization is retried every 30 seconds and values
are measured every ten seconds. Home Assistant exposes shield availability and
measurements as diagnostics. See the [sensor test guide](tests/host/SENSORS.md).

### Drive interface

| Pin | Signal | Type |
| --- | --- | --- |
| D0 | Open command | Output |
| D1 | Door open status | Input |
| D2 | Close command | Output |
| D3 | Door closed status | Input |

The pin assignments are configurable in `include/config.h`.

## Configuration

The controller uses DHCP. Copy `include/config_local.h.example` to the ignored
`include/config_local.h` and replace its network-specific placeholders. Do not
commit this file.

```cpp
#define GDC_MAC_ADDRESS {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
#define GDC_MQTT_BROKER "mqtt.example.invalid"
#define GDC_MQTT_PORT 1883
#define GDC_MQTT_CLIENT_ID "garage-door-controller"
#define GDC_MQTT_USERNAME "your-mqtt-user"
#define GDC_MQTT_PASSWORD "change-this-password"
```

The MAC address and MQTT client ID must each be unique. The current DHCP address
is printed as `NET: DHCP address: ...` and shown on the OLED System page.

## MQTT topic tree — breaking change in 1.1.0

Each controller has a separate root derived from its configured MAC address. For
MAC `DE:AD:BE:EF:12:34`, the root is:

```text
gdc/gdc-deadbeef1234/
```

Retained topics published by the controller:

```text
availability              online | offline
state                     JSON: door, sensors, firmware, IP and diagnostics
system/boot               JSON: boot uptime, sent once per MQTT connection
update/state              JSON: installed/latest version and update progress
```

Non-retained command topics:

```text
command/door              open | close | stop
command/update            install
command/restart           restart
```

`availability` uses MQTT birth and last-will messages. The broker sets it to
`offline` if the controller disconnects unexpectedly; the controller sets it to
`online` after connecting. The former global `gdc/system/...`,
`gdc/control/...` and `gdc/update/...` topics are no longer published.

## Home Assistant

Enable MQTT Discovery and add Home Assistant's MQTT integration. The controller
then creates one device with a garage-door cover, firmware-update entity,
restart button, availability, and diagnostic entities for network, firmware,
ENV shield, end stops, command source and travel times. The **Door status** entity
shows `open`, `closed`, `moving`, `stopped` or `unknown`. No manual YAML is
needed.

See [Home Assistant](docs/home-assistant.md) for installation, migration and the
complete entity list.

## Homebridge

Homebridge can use the MQTT tree above. The retained `state` document contains
the door state in `door.state`; commands go to `command/door`. Existing
Homebridge mappings using `gdc/control/...` must be migrated for 1.1.0.

## Firmware update and debug console

PlatformIO uploads stage firmware on the MKR Zero SD card. The staged update can
be installed from the HMI or through Home Assistant when the door is safely open
or closed. The Info button HMI displays availability and progress.

See [LAN firmware update](docs/lan-update.md) and
[Debug console](docs/debug-console.md).
