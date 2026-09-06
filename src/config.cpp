#include "config.h"

#if defined(__has_include)
#if __has_include("config_local.h")
#include "config_local.h"
#endif
#endif

// Safe build defaults. A local config_local.h can override these values without
// placing local network details or credentials in the Git repository.
#ifndef GDC_MAC_ADDRESS
#define GDC_MAC_ADDRESS {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
#endif
#ifndef GDC_MQTT_BROKER
#define GDC_MQTT_BROKER "mqtt.example.invalid"
#endif
#ifndef GDC_MQTT_PORT
#define GDC_MQTT_PORT 1883
#endif
#ifndef GDC_MQTT_CLIENT_ID
#define GDC_MQTT_CLIENT_ID "garage-door-controller"
#endif
#ifndef GDC_MQTT_USERNAME
#define GDC_MQTT_USERNAME "your-mqtt-user"
#endif
#ifndef GDC_MQTT_PASSWORD
#define GDC_MQTT_PASSWORD "change-this-password"
#endif

// this information is shown on the OLED display and also sent to the MQTT broker
String application = "GarageDoorController";
String version = "1.0.0";
String author = "smhex";

// DHCP identifies this controller by MAC; keep it unique on the local network.
byte mac[] = GDC_MAC_ADDRESS;

// MQTT configuration
const char mqttBrokerAddress[] = GDC_MQTT_BROKER;
const unsigned int mqttBrokerPort = GDC_MQTT_PORT;
String mqttClientID = GDC_MQTT_CLIENT_ID;
String mqttUsername = GDC_MQTT_USERNAME;
String mqttPassword = GDC_MQTT_PASSWORD;
String mqttLastWillMsg = "offline";
String mqttFirstWillMsg = "online";

// duration for OLED display in HMI module being active after button press
int displayTimeout_ms = 30000;

// On/Off time in ms for the leds when door is moving
int ledBlinkDuration_ms = 100;

// duration in ms for the command pulse
int commandDuration_ms = 500;
