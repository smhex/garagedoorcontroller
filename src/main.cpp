/* 
* File:     main.cpp
* Date:     09.01.2021
* Version:  v0.0.4
* Author:   smhex
*/

// Include libraries
#include <Arduino.h>

#include <SPI.h>
#include <Ethernet.h>
#include <WDTZero.h>

// Include local libraries/headers
#include "hmi.h"
#include "util.h"
#include "mqtt.h"
#include "sensors.h"

// Network configuration - sets MAC and IP address
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 30, 241);
IPAddress dns(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 30, 1);
EthernetClient ethClient;

// global settings
String application = "GarageDoorController";
String version = "0.0.4";
String author = "smhex";

// Heartbeat counter
unsigned long uptime_in_sec = 0;

// Watchdog
WDTZero watchdog;

static unsigned long last_milliseconds;
int ledState = LOW;

// Forward declarations
void watchdog_init();
void watchdog_reset();
void watchdog_onShutdown();

// setup the board an all variables
void setup()
{

  // Init serial line with 9600 baud
  Serial.begin(9600);
  while (!Serial)
    ;

  // setup watchdog
  watchdog_init();

  // Initialize display
  hmi_init();
  hmi_display_splashscreen("Loading...");

  last_milliseconds = millis();

  // Initial delay to get the serial monitor attached after port is availabe for host
  delay(1000);

  // This should be the first line in the serial log
  Serial.println("INIT: Starting...");
  Serial.println("INIT: Sketch built on " __DATE__ " at " __TIME__);

  // check if all the hardware is installed/present
  // start with MKR ENV shield
  sensors_init();

  // check MKR ETH shield / connection
  // interface uses a fully configured static ip
  Ethernet.begin(mac, ip, dns, gateway, subnet);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("ERROR: Ethernet shield was not found");
    while (true)
    {
      delay(1);
    }
  }
  else
  {
    Serial.print("INIT: Ethernet chipset type is ");
    Serial.println(Ethernet.hardwareStatus());
  }
  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("ERROR: Ethernet cable is not connected");
  }

  Serial.print("INIT: Controller network interface is at ");
  Serial.println(Ethernet.localIP());

  // show IP addess on display
  hmi_display_splashscreen(IPAddressToString(Ethernet.localIP()));

  // Initialize MQTT client
  mqtt_init();
}

// main loop - reads/writes commands and sensor values
void loop()
{

  // calculate uptime in seconds
  uptime_in_sec = (millis() - last_milliseconds) / 1000;

  /*    float temp = sensors_get_temperature();
  char buffer[12];
  sprintf(buffer, "%f", temp);
  mqtt_publish("gdc/system/sensors/temperature", buffer);*/

  // loop over all modules
  hmi_loop();
  mqtt_loop();
  sensors_loop();

  // trigger the watchdog
  watchdog_reset();
}

/*
 * This function needs to be called to initialize the watchdog.
 */
void watchdog_init()
{
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  watchdog.attachShutdown(watchdog_onShutdown);
  watchdog.setup(WDT_SOFTCYCLE16S);
}

/*
 * This function needs to be called to reset the watchdog.
 */
void watchdog_reset()
{
  // clear the watchdog
  watchdog.clear();

  // led the inbuilt led blink as a heartbeat
  static uint32_t prev_ms = millis();
  if (millis() > prev_ms + 1000)
  {
    // if the LED is off turn it on and vice-versa
    ledState = (ledState == LOW) ? HIGH : LOW;
    prev_ms = millis();
    digitalWrite(LED_BUILTIN, ledState);
  }
}

/*
 * This function is called of the watchdog is not cleared. This usally happens if
 * the processor is stalled.
 */
void watchdog_onShutdown()
{
  Serial.print("\nERROR: watchdog not cleared. Controller reboot initiated");
}