#include "config.h"

// this information is shown on the OLED display and also sent to the MQTT broker
String application = "GarageDoorController";
String version = "0.1.10";
String author = "smhex";

// DHCP identifies this controller by MAC; keep it unique on the local network.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// MQTT configuration
const char mqttBrokerAddress[] = "mosquitto.debes-online.com";
const unsigned int mqttBrokerPort = 1883;
String mqttClientID = "arduino-gdc";
String mqttUsername = "mosquitto";
String mqttPassword = "mosquitto";
String mqttLastWillMsg = "offline";
String mqttFirstWillMsg = "online";

// duration for OLED display in HMI module being active after button press
int displayTimeout_ms = 30000;

// On/Off time in ms for the leds when door is moving
int ledBlinkDuration_ms = 100;

// duration in ms for the command pulse
int commandDuration_ms = 500;
