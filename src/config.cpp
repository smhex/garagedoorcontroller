#include "config.h"

// this information is shown on the OLED display and also sent to the MQTT broker
String application = "GarageDoorController";
String version = "0.1.7";
String author = "smhex";

// Network configuration - sets MAC and IP address
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 30, 241);
IPAddress dns(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 30, 1);

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