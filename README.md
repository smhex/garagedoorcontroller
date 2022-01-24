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
This solution uses a wired Ethernet interface. The network configuration is static. If you want to use your own configuration please change the following settings in *config.cpp* before downloading the sketch. Adapting the code for using with a Wifi Shield should be easy.

```
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 30, 241);
IPAddress dns(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 30, 1);
```

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