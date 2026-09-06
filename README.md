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
IP address, subnet mask, gateway and DNS server. Local MAC and MQTT settings are
kept outside the Git repository. Copy `include/config_local.h.example` to
`include/config_local.h`, then replace every placeholder with values from your
network. The local file is ignored by Git:

```cpp
#define GDC_MAC_ADDRESS {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
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

### Verify the DHCP address through MQTT

After the controller reports `MQTT: Connected`, subscribe to the retained system
information topic. A subscriber receives the most recently published value
immediately, so it also works when the controller is already online:

```sh
mosquitto_sub -h <broker> -p <port> -u <username> -P <password> \
  -t 'gdc/system/info' -C 1 -v
```

Example output:

```text
gdc/system/info {"application":"GarageDoorController","version":"1.0.0","author":"smhex","ip":"192.0.2.42"}
```

The `ip` property is the DHCP address currently used by the controller. It is
republished after every MQTT reconnect and after a lease change. Home Assistant
or another MQTT client can subscribe to this topic and read the JSON `ip` value
for diagnostics. `gdc/system/status` is retained as `online` while the MQTT
connection is active and changes to `offline` through the MQTT last will.

The MQTT functionality is implemented using the MQTTPubSubClient library. Set
the following values in the ignored `include/config_local.h` file:

```cpp
#define GDC_MQTT_BROKER "mqtt.example.invalid"
#define GDC_MQTT_PORT 1883
#define GDC_MQTT_CLIENT_ID "garage-door-controller"
#define GDC_MQTT_USERNAME "your-mqtt-user"
#define GDC_MQTT_PASSWORD "change-this-password"
```

The example values are deliberately nonfunctional. The MAC address must be unique
on your LAN, and `GDC_MQTT_CLIENT_ID` must be unique on brokers that enforce
unique client IDs. Do not commit `config_local.h`.

## Homebridge
The interface to Homebrigde is basically the MQTT broker. The garage door controller provides a set of specific topics which will be read or written by the Homebridge plugin *homebridge-mqttthing*. For more information please read the plugin's [documentation](https://github.com/arachnetech/homebridge-mqttthing/blob/master/docs/Accessories.md#garage-door-opener) for setting up a garage door opener accessory in Homebridge. Please note that this controller does not support the optional topcis. You can use the following configuration to get started:

```
{
    "name": "Garage",
    "accessory": "mqttthing",
    "type": "garageDoorOpener",
    "url": "mqtt://mqtt.example.invalid:1883",
    "username": "your-mqtt-user",
    "password": "change-this-password",
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
