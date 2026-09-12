#ifndef __CONFIG_H_INCLUDED__
#define __CONFIG_H_INCLUDED__

#include <Arduino.h>
#include <Ethernet.h>

// Arduino pins connected by the opto-isolated interface circuit to the
// Marantec drive. D0 activates "Tor auf" and D2 activates "Tor zu"; D1/D3
// only read the corresponding status signals.
#define CMD_OPENDOOR_OUTPUT       0
#define STATUS_DOORISOPEN_INPUT   1
#define CMD_CLOSEDOOR_OUTPUT      2
#define STATUS_DOORISCLOSED_INPUT 3

// Define the pages on the OLED display in their navigation order.
#define PAGE_OVERVIEW   0
#define PAGE_NETWORK    1
#define PAGE_SENSORS    2
#define PAGE_DRIVEIO    3
#define PAGE_SYSTEM     4
#define PAGE_UPDATE     5

/* To change the content of the following variables go to config.cpp */

// variables for global settings shared between the cpp modules
extern String application;
extern String version;
extern const char firmwareVersionMarker[];
extern String author;

// variables for network settings
extern byte mac[];

// variables for MQTT settings
extern const char mqttBrokerAddress[];
extern const unsigned int mqttBrokerPort;
extern String mqttClientID;
extern String mqttUsername;
extern String mqttPassword;
extern String mqttLastWillMsg;
extern String mqttFirstWillMsg;

// shared varaibles being used in more than one module - look in config.cpp 
// for their initial values
extern EthernetClient ethClient;
extern int displayTimeout_ms;
extern int displayCommandTimeout_ms;
extern unsigned long uptime_in_secs;
extern int ledBlinkDuration_ms;
extern int commandDuration_ms;

#endif // __CONFIG_H_INCLUDED__
