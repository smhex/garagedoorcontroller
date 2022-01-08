/* 
* File:     main.cpp
* Date:     04.01.2021
* Version:  v0.0.2
* Author:   smhex
*/

// Include libraries
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_MKRENV.h>
#include <MQTTPubSubClient.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WDTZero.h>

#include "display.h"
#include "util.h"

// global settings
String application = "GarageDoorController";
String version = "0.0.2";
String author = "smhex";

// global buffer for dealing with json packets
StaticJsonDocument<128> jsonDoc;
char jsonBuffer[128];

// Network configuration - sets MAC and IP address
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 30, 241);
IPAddress dns(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 30, 1);
EthernetClient ethClient;

// MQTT broker/topic configuration
MQTTPubSubClient mqttClient;
const char mqttBrokerAddress[] = "mosquitto.debes-online.com";
const unsigned int mqttBrokerPort = 1883;
String mqttClientID = "arduino-gdc";
String mqttUsername = "mosquitto";
String mqttPassword = "mosquitto";
String mqttTopicUptime = "gdc/system/uptime";
String mqttTopicInfo = "gdc/system/info";
String mqttTopicStatus = "gdc/system/status";
String mqttLastWillMsg = "offline";

// Watchdog
WDTZero watchdog;   

// Heartbeat counter
static unsigned long uptime_in_sec = 0;
static unsigned long last_milliseconds;  
int ledState = LOW;

// Forward declarations
void mqtt_init();
void mqtt_loop();
void watchdog_handler();

// setup the board an all variables
void setup() {
  
  // Init serial line with 9600 baud
  Serial.begin(9600);
  while (!Serial);

  // Initialize display
  display_init();
  display_splashscreen(application, version, author, "Loading...");

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  last_milliseconds = millis();

  // Initial delay to get the serial monitor attached after port is availabe for host
  delay(1000);

  // setup watchdog 16s interval
  watchdog.attachShutdown(watchdog_handler);
  //watchdog.setup(WDT_SOFTCYCLE16S);

  // This should be the first line in the serial log
  Serial.println("INIT: Starting...");
  Serial.println ("INIT: Sketch built on " __DATE__ " at " __TIME__);

  // check if all the hardware is installed/present
  // start with MKR ENV shield
  if (!ENV.begin()) {
    Serial.println("ERROR: Failed to initialize MKR ENV shield");
    while (1);
  }

  // check MKR ETH shield / connection
  // interface uses a fully configured static ip
  Ethernet.begin(mac, ip, dns, gateway, subnet);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("ERROR: Ethernet shield was not found");
    while (true) {
      delay(1); 
    }
  }
  else{
    Serial.print("INIT: Ethernet chipset type is ");
    Serial.println(Ethernet.hardwareStatus());

  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("ERROR: Ethernet cable is not connected");
  }

  Serial.print("INIT: Controller network interface is at ");
  Serial.println(Ethernet.localIP());

  // show IP addess on display
  display_splashscreen(application, version, author, IPAddressToString(Ethernet.localIP()));


  // Initialize MQTT client
  //mqtt_init();
}

// main loop - reads/writes commands and sensor values
void loop() {
  // read all the sensor values
  /*
  float temperature = ENV.readTemperature();
  float humidity    = ENV.readHumidity();
  float pressure    = ENV.readPressure();
  float illuminance = ENV.readIlluminance();  
  */

  // calculate uptime in seconds
  uptime_in_sec = (millis()-last_milliseconds)/1000;
  
  // led the inbuilt led blink as a heartbeat
  static uint32_t prev_ms = millis();
  if (millis() > prev_ms + 1000) {

    // if the LED is off turn it on and vice-versa
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    prev_ms = millis();
    digitalWrite(LED_BUILTIN, ledState);
  }

  // Update mqtt
  //mqtt_loop();

  // Trigger watchdog
  watchdog.clear();
}

/*
* Creates the connection to the MQTT broker and performs the authentificaton. The 
* topics will be initially published and the subscriptions will created
*/
void mqtt_init()
{
  // set mqtt client options
  mqttClient.setKeepAliveTimeout(60);
  mqttClient.setCleanSession(true);
  mqttClient.setWill(mqttTopicStatus, mqttLastWillMsg, true, 0);
  
  // Connect to the broker
  Serial.print("INIT: Connecting mqtt broker...");
  while (!ethClient.connect(mqttBrokerAddress, mqttBrokerPort)) {
      Serial.print(".");
      delay(1000);
  }
  Serial.println(" success");

  // Authenticate the client
  mqttClient.begin(ethClient);
  Serial.print("INIT: Logging into mqtt broker...");
  while (!mqttClient.connect(mqttClientID, mqttUsername, mqttPassword)) {
      Serial.print(".");
      delay(1000);
  }
  Serial.println(" success");

  // prepare json payload for info topic
  jsonDoc["application"] = application;
  jsonDoc["version"] = version;
  jsonDoc["author"] = author;

  // serialize json document into global buffer and publish
  // attention: size of buffer is limited to 256 bytes
  serializeJson(jsonDoc, jsonBuffer);
  mqttClient.publish(mqttTopicInfo, jsonBuffer, true, 0);
}

/*
 * This function is manages the mqtt connection and publishes the uptime every sec.
 */
void mqtt_loop()
{
  // if connection to the broker is lost, try to reconnect
  if (!mqttClient.isConnected()){
    Serial.println("RUN: Lost connection to mqtt broker. Trying to reconnect");
    mqtt_init();
  }
  else{
    mqttClient.update();
  }

  // publish uptime message every 1s
  static uint32_t prev_ms = millis();
  char buffer[12];
  sprintf(buffer,"%lu", uptime_in_sec);
  if (millis() > prev_ms + 1000) {
      prev_ms = millis();
      mqttClient.publish(mqttTopicUptime, buffer);
      mqttClient.publish(mqttTopicStatus, "online", true, 0);
    }
}

/*
 * This function is called of the watchdog is not cleared. This usally happens if
 * the processor is stalled.
 */
void watchdog_handler()
{
  Serial.print("\nERROR: watchdog not cleared. Controller reboot initiated");
}