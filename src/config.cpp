#include "config.h"

// this information is shown on the OLED display and also sent to the MQTT broker
String application = "GarageDoorController";
String version = "1.0.0";
String author = "smhex";

// DHCP identifies this controller by MAC; keep it unique on the local network.
byte mac[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

// MQTT configuration
const char mqttBrokerAddress[] = "mqtt.example.invalid";
const unsigned int mqttBrokerPort = 1883;
String mqttClientID = "garage-door-controller";
String mqttUsername = "your-mqtt-user";
String mqttPassword = "change-this-password";
String mqttLastWillMsg = "offline";
String mqttFirstWillMsg = "online";

// duration for OLED display in HMI module being active after button press
int displayTimeout_ms = 30000;

// On/Off time in ms for the leds when door is moving
int ledBlinkDuration_ms = 100;

// duration in ms for the command pulse
int commandDuration_ms = 500;
