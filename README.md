# Garage Door Controller
This is a smart home garage door controller based on an Arduino MKR Zero for Marantec Drives using Systembus. While the software part can be used with many other drives providing simple IOs to start/stop the drive the hardware interface is special to Marantec. There is an excellent schematic available which not only provides the necessary commands to open and close the door but also to get the corresponding states (door open/door closed) directly from the drive (see  [Marantec Kopplung - Ralf-Rathgeber](https://www.ralf-rathgeber.de/hausautomation/marantec.html)). There is no need to install additional sensors to detect the door state. 

# Hardware
As I own a Marantec model Comfort 220 with Systembus I cannot guarantee that it works with other models. However, this project can easily be adopted to other brands. One of my requirements was to connect the Arduino via Ethernet and not via Wifi. Moreover the housing of the controller should be industrial grade and DIN rail mountable. 

## Required parts
- Arduino [MKR Zero](https://docs.arduino.cc/hardware/mkr-zero)
- Arduino Ethernet Shield [MKR ETH Shield](https://docs.arduino.cc/hardware/mkr-eth-shield)
- Housing [ArduiBox MKR](https://www.hwhardsoft.de/deutsch/projekte/arduibox-mkr/) from Zihatec

## Optional parts
- Arduino Environmental sensor [MKR ENV Shield](https://docs.arduino.cc/hardware/mkr-env-shield)
- Display module [OLED Display Shield](https://www.hwhardsoft.de/deutsch/projekte/display-shield/) from Zihatec with additional buttons and leds

The ENV shield is optional at runtime. Failed initialization or invalid sensor
readings leave the door controller running, with initialization retries every
30 seconds. Measurements run every ten seconds. MQTT topic
`gdc/system/sensors/status` reports retained `available` or `unavailable`; numeric
snapshots are omitted while unavailable. Consumers should use this status to
avoid displaying cached measurements as current. See the
[sensor test guide](tests/host/SENSORS.md) for checks and shared-I2C limitations.


# Software
The purpose of this controller is to provide connectivity to my Smarthome system. I am using [Homebridge](https://homebridge.io), a fantastic piece of software to connect and extend Apple's HomeKit system. As I didn't want to develop a native interface an alternative was needed. Luckily, MQTT is a lightweight protocol which is supported by many other systems. In this setup you will need:

- Smarthome system [Homebridge](https://homebridge.io)
- MQTT Plugin [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing)
- MQTT Broker [Mosquitto](https://mosquitto.org)

# Configuration
## Hardware Interface
To control the door by sending commands and reading the status a simple io interface is needed. The required interface is well described [here](https://www.ralf-rathgeber.de/hausautomation/marantec.html). The interface to the Arduino MKR Zero (or any other compatible controller) requires 2 digital output and 2 digital inputs. On my controller the configuration is the following

| PIN | Signal             | Type   |
| --- | ------------------ | ------ |
| D0  | Command Door Open  | Output |
| D1  | Status Door Open   | Input  |
| D2  | Command Door Close | Output |
| D3  | Status Door Closed | Input  |

If you want to use a different configuration the pins can be assigned in *config.h*

```
#define CMD_OPENDOOR_OUTPUT       0
#define STATUS_DOORISOPEN_INPUT   1
#define CMD_CLOSEDOOR_OUTPUT      2
#define STATUS_DOORISCLOSED_INPUT 3
```

## Ethernet/MQTT interface
This solution uses wired Ethernet with DHCP. The router/DHCP server supplies the
IP address, subnet mask, gateway and DNS server. Only the MAC address is configured
in `src/config.cpp`; it must be unique on your local network:

```cpp
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
```

`src/network.cpp` acquires and renews the lease. DHCP attempts use a 2000 ms
transaction timeout and a 500 ms response timeout (these are library timeouts,
not a hard real-time deadline). Failed acquisition/renewal is retried after
10 seconds, without falling back to a static address. MQTT connects only after
DHCP succeeds and retries failed connections every 10 seconds. DNS, TCP and MQTT
operations have explicit timeouts. Lease changes that alter the IP close the old
TCP connection so MQTT can reconnect and subscribe again.

Network maintenance and MQTT processing are skipped during active door command
pulses. Connection attempts can still briefly delay button processing between
pulses; this is not a fully asynchronous network stack. The existing 16-second
watchdog remains enabled and is serviced between DHCP and MQTT work.
The current IP is printed as `NET: DHCP address: ...` and shown on the OLED System
page. A retained `gdc/system/info` MQTT JSON document also includes the controller
application, firmware version, author and current DHCP address as `ip`. It is
republished whenever MQTT reconnects, including after an IP change. A DHCP
reservation for the MAC can provide a stable address if desired.

The MQTT functionality is implemented using the MQTTPubSubClient library. There are some configuration settings in *config.cpp*.

```
const char mqttBrokerAddress[] = "mosquitto.debes-online.com";
const unsigned int mqttBrokerPort = 1883;
String mqttClientID = "arduino-gdc";
String mqttUsername = "mosquitto";
String mqttPassword = "mosquitto";
String mqttLastWillMsg = "offline";
String mqttFirstWillMsg = "online";
````

Broker address, port and the credentials must be changed according to your environment. Please be ware of the mqttClientID. On some brokers it must be unique, otherwise a connection request will be rejected.

## Homebridge
The interface to Homebrigde is basically the MQTT broker. The garage door controller provides a set of specific topics which will be read or written by the Homebridge plugin *homebridge-mqttthing*. For more information please read the plugin's [documentation](https://github.com/arachnetech/homebridge-mqttthing/blob/master/docs/Accessories.md#garage-door-opener) for setting up a garage door opener accessory in Homebridge. Please note that this controller does not support the optional topcis. You can use the following configuration to get started:

```
{
    "name": "Garage",
    "accessory": "mqttthing",
    "type": "garageDoorOpener",
    "url": "mqtt://mosquitto.debes-online.com:1883",
    "username": "mosquitto",
    "password": "mosquitto",
    "logMqtt": true,
    "mqttOptions": {
        "keepalive": 60
    },
    "topics": {
        "getCurrentDoorState": "gdc/control/getcurrentdoorstate",
        "setTargetDoorState": "gdc/control/setnewdoorstate",
        "getTargetDoorState": "gdc/control/getnewdoorstate"
    }
    "doorTargetValues": [
        "open",
        "close"
    ],
    "doorCurrentValues": [
        "open",
        "closed",
        "opening",
        "closing",
        "stopped"
    ],
}
```

Please make sure that you use the same credentials and broker settings as configured earlier in the source code.

## Build and upload over USB (macOS / VS Code)

1. Open this project in VS Code with the PlatformIO extension. Under PlatformIO
   Project Tasks, select `mkrzero-release` → General → Build.
2. Connect the MKR Zero's micro-USB port to the Mac using a USB data cable.
   Close any serial monitor before uploading. No external programmer is required.
3. Select `mkrzero-release` → General → Upload. PlatformIO uses the board's
   default `sam-ba` upload protocol and automatically detects the USB port.
4. Open Monitor at 9600 baud. Confirm `NET: DHCP address: ...` and
   `MQTT: Connected`. The router must provide DHCP and a working DNS server.

Equivalent commands from the project directory in a PlatformIO terminal:

```sh
pio run -e mkrzero-release
pio device list
pio run -e mkrzero-release -t upload
pio device monitor -b 9600
```

If `pio` is not on PATH on this Mac, use `~/.platformio/penv/bin/pio` instead.
If multiple serial devices are connected, select the controller explicitly:

```sh
pio run -e mkrzero-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 9600
```

Replace the example port with the one reported by `pio device list`. If uploading
fails because the running sketch does not respond, press RESET twice quickly to
enter the MKR bootloader, list the ports again, and retry Upload using the new port.
The port may change between bootloader and application mode.
Ethernet/MQTT firmware upload (OTA) is not implemented in this project.

References: [PlatformIO MKR Zero upload support](https://docs.platformio.org/en/latest/boards/atmelsam/mkrzero.html),
[Arduino bootloader reset procedure](https://support.arduino.cc/hc/en-us/articles/5779192727068-Reset-your-board).

### Hardware checks after flashing

- Boot with Ethernet connected: verify DHCP address, MQTT connection, commands and
  published door state.
- Boot without Ethernet/DHCP: verify local buttons still work between connection
  attempts and no recurring watchdog reboot occurs. Reconnect Ethernet and verify
  DHCP/MQTT recovery.
- Interrupt the broker/DNS service and restore it: verify retries and subscriptions
  recover without restarting the controller.
- Test lease renewal (including an IP change), ideally with a short DHCP lease on
  a test network. Verify MQTT reconnects after an IP change.
- Verify command pulses remain at the configured duration during network outages.
  These timing and recovery checks require the actual controller; compilation alone
  does not validate them.


## Continue on a Windows laptop

Install the PlatformIO IDE extension in VS Code and Git for Windows if Git is not
already installed. Clone the DHCP branch and open the folder in VS Code:

```sh
git clone --branch dev https://github.com/smhex/garagedoorcontroller.git
cd garagedoorcontroller
code .
```

In an existing clone, use `git fetch origin` followed by `git switch dev`.
The release preparation branch is `release/0.1.9`; see [CHANGELOG.md](CHANGELOG.md)
for tested functionality and known limitations.
PlatformIO downloads the board toolchain and libraries on first build. The MQTT
library is pinned to its upstream 0.1.2 commit because the old registry requirement
is no longer available. Use Project Tasks → mkrzero-release → Build, then connect
the MKR Zero with a USB data cable and select Upload. Monitor uses 9600 baud.
If automatic port detection fails, run `pio device list` in a PlatformIO terminal
and specify the actual COM port:

```sh
pio run -e mkrzero-release -t upload --upload-port COM5
pio device monitor -p COM5 -b 9600
```

Close Monitor before uploading. Double-press RESET if necessary to enter the
bootloader; its COM port can differ from the running application's port.
