# Debug console

The controller provides the same application log on USB serial and over a raw
TCP connection on port **2323**. The LAN console is intended for diagnostics and
does not provide source-level debugging or breakpoints.

## Connect with VS Code

Configure the controller address in the ignored `platformio_local.ini` as
described in [LAN firmware update](lan-update.md). Then select
**mkrzero-lan → Monitor** in PlatformIO. It connects to
`socket://ADDRESS:2323`; there is no Telnet negotiation and no separate console
application required.

Only one LAN monitor can be connected at a time. A second connection is refused
while the first is active.

## Logging window

Logging starts automatically when the application boots and remains enabled for
the first 60 seconds. At expiry, the controller writes:

```text
DEBUG: automatic logging expired; send "d" to re-enable it
```

Send a lowercase `d` through USB serial or the TCP console to enable output
until the next reboot. The console replies:

```text
DEBUG: output enabled until reboot
```

The listener starts only after the network is ready. Consequently, a console
connection does not reset the controller and does not replay messages that were
already discarded. In particular, the one-time sensor-start message can precede
the TCP connection. If the ENV shield is unavailable, its retry after at most
30 seconds produces a new `SENSORS: unavailable` message while logging is
enabled.

Log delivery is deliberately best effort: bounded queues prevent a slow or
absent reader from delaying door control. Messages can therefore be lost, and
there is no persistent log on the SD card.

## Typical messages

- `INIT:`: application startup, firmware version and build timestamp.
- `NET:`: DHCP acquisition or lease renewal.
- `MQTT:`: broker connection, subscriptions and publish failures.
- `SENSORS:`: ENV-shield detection and invalid measurements.
- `IO:`: door input transitions and completed command pulses.
- `OTA:`: LAN transfer, image validation and installation requests.

The controller keeps accepting `d` after automatic logging expired; no other
TCP console command is currently implemented.
