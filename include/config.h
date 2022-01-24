#ifndef __CONFIG_H_INCLUDED__
#define __CONFIG_H_INCLUDED__

#include <Arduino.h>
#include <Ethernet.h>

// define the input and output pins to control the drive
// those pin numbers must match the circuit/schematic
#define CMD_OPENDOOR_OUTPUT       0
#define STATUS_DOORISOPEN_INPUT   1
#define CMD_CLOSEDOOR_OUTPUT      2
#define STATUS_DOORISCLOSED_INPUT 3

// define the different pages on the OLED display - their sequence
// can be changed by simply re-arranging their position 
#define PAGE_OVERVIEW   0
#define PAGE_SENSORS    1
#define PAGE_DRIVEIO    2
#define PAGE_HMI        3
#define PAGE_MQTT       4
#define PAGE_SYSTEM     5

/* To change the content of the following variables go to config.cpp */

// variables for global settings shared between the cpp modules
extern String application;
extern String version;
extern String author;

// variables for network settings
extern byte mac[];
extern IPAddress ip;
extern IPAddress dns;
extern IPAddress subnet;
extern IPAddress gateway;

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
extern unsigned long uptime_in_secs;
extern int ledBlinkDuration_ms;
extern int commandDuration_ms;

#endif // __CONFIG_H_INCLUDED__