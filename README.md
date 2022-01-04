# Garage Door Controller
This is a smart home garage door controller based on an Arduino MKR Zero for Marantec Drives using Systembus. While the software part can be used with many other drives providing simple IOs to start/stop the drive the hardware interface is special to Marantec. There is an excellent schematic available which not only provides the necessary commands to open and close the door but also to get the corresponding states (door opened/door closed) directly from the drive (see  [Marantec Kopplung - Ralf-Rathgeber](https://www.ralf-rathgeber.de/hausautomation/marantec.html)). There is no need to install additional sensors to detect the door state. 

# Hardware
As I own a Marantec model Comfort 220 with Systembus I cannot guarantee that it works with other models. However, this project can easily be adopted to other brands. One of my requirements was to connect the Arduino via Ethernet and not via Wifi. Moreover the housing of the controller should be industrial grade and DIN rail mountable. 

## Required parts
- Arduino [MKR Zero](https://docs.arduino.cc/hardware/mkr-zero)
- Ethernet Shield [MKR ETH Shield](https://docs.arduino.cc/hardware/mkr-eth-shield)
- Housing [ArduiBox MKR](https://www.hwhardsoft.de/deutsch/projekte/arduibox-mkr/)

## Optional parts
- Environmental sensor [MKR ENV Shield](https://docs.arduino.cc/hardware/mkr-env-shield)
- Display module [OLED Display Shield](https://www.hwhardsoft.de/deutsch/projekte/display-shield/)


# Software
The purpose of this controller is to provide connectivity to my Smarthome system. I am using [Homebridge](https://homebridge.io), a fantastic piece of software to connect and extend Apple's HomeKit system. As I don't want to develop a native interface an alternative is needed. Luckily, MQTT is a lightweight protocol which is supported by many other systems. In this setup you will need:

- Smarthome system [Homebridge](https://homebridge.io)
- MQTT Plugin [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing)
- MQTT Broker [Mosquitto](https://mosquitto.org)

# Configuration