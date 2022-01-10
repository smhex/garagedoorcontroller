/* 
* File:     main.cpp
* Date:     09.01.2021
* Version:  v0.0.5
* Author:   smhex
*/

// Include libraries
#include <Arduino.h>

#include <SPI.h>
#include <Ethernet.h>
#include <WDTZero.h>
#include <ArduinoJson.h>

// Include local libraries/headers
#include "driveio.h"
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
String version = "0.0.5";
String author = "smhex";

// global buffer for dealing with json packets

// Heartbeat counter
unsigned long uptime_in_sec = 0;
bool mainFirstRun = true;

// Watchdog
WDTZero watchdog;

static unsigned long last_milliseconds;
int ledState = LOW;

// maintain door status
int oldDoorStatus = 0;
int newDoorStatus = 0;

// maintain door command
int lastCommand = 0;

// Forward declarations
void watchdog_init();
void watchdog_reset();
void watchdog_onShutdown();
void publish_sensor_values();
void command_open(String fromSource);
void command_close(String fromSource);

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

  // store offset for uptime counter
  last_milliseconds = millis();

  // Initial delay to get the serial monitor attached after port is availabe for host
  delay(1000);

  // This should be the first line in the serial log
  Serial.println("INIT: Starting...");
  Serial.println("INIT: Sketch built on " __DATE__ " at " __TIME__);

  // check if all the hardware is installed/present
  // start with MKR ENV shield
  sensors_init();

  // init baseboard
  driveio_init();

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

  // loop over all modules
  driveio_loop();
  hmi_loop();
  sensors_loop();
  mqtt_loop();

  // gets the current sensor values and sends them via mqtt
  publish_sensor_values();

  // trigger the watchdog
  watchdog_reset();

  // check if door status was changed
  if (driveio_doorstatuschanged(&oldDoorStatus, &newDoorStatus))
  {
    if (newDoorStatus == DOORSTATUSOPEN)
    {
      mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOOROPEN);
      mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOOROPEN);
     }
    if (newDoorStatus == DOORSTATUSCLOSED)
    {
      mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOORCLOSED);
      mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOORCLOSE);
    }
    if (newDoorStatus == DOORSTATUSEXTERNAL)
    {
      mqtt_publish(MQTT_TOPICCONTROLCOMMANDSOURCE, MQTT_COMMANDSOURCEEXTERNAL);     
    }   
  }

  // check for user command (button press on HMI)
  int buttonPressed = hmi_getbuttonpressed();
  if (buttonPressed != HMI_BUTTON_NONE)
  {
    lastCommand = buttonPressed;
    if (buttonPressed == HMI_BUTTON_OPENDOOR)
    {
      command_open(MQTT_COMMANDSOURCELOCAL);
    }
    if (buttonPressed == HMI_BUTTON_CLOSEDOOR)
    {
      command_close(MQTT_COMMANDSOURCELOCAL);
    }
  }

  // check for remote command (over MQTT)
  String remoteCommand = mqtt_getcommand();
  if (remoteCommand.length()!=0){
    if (remoteCommand==MQTT_COMMANDDOOROPEN)
    {
      command_open(MQTT_COMMANDSOURCEREMOTE);
    }
    if (remoteCommand==MQTT_COMMANDDOORCLOSE)
    {
      command_close(MQTT_COMMANDSOURCEREMOTE);
    }
  }

  mainFirstRun = false;
}

/*
 * set door to open
 */
void command_open(String fromSource)
{
    char buffer[80];
    sprintf(buffer, "RUN: Command: DOOROPEN (source=%s)" , fromSource.c_str());
    Serial.println(buffer);
    mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOOROPEN);
    mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOOROPENING);
    mqtt_publish(MQTT_TOPICCONTROLCOMMANDSOURCE, fromSource);
    driveio_setdoorcommand(DOORCOMMANDOPEN);
}

/*
 * set door to close
 */
void command_close(String fromSource)
{
    char buffer[80];
    sprintf(buffer, "RUN: Command: DOORCLOSE (source=%s)", fromSource.c_str());
    Serial.println(buffer);   
    mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOORCLOSE);
    mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOORCLOSING);
    mqtt_publish(MQTT_TOPICCONTROLCOMMANDSOURCE, fromSource);
    driveio_setdoorcommand(DOORCOMMANDCLOSE);
}

/*
 * Gets all the sensor values and publishes them as json string
 */
void publish_sensor_values()
{
  if (timespan_ten_seconds() | mainFirstRun)
  {
    // json document
    DynamicJsonDocument jsonSensorValuesDoc(256);
    char jsonSensorValuesBuffer[256];

    JsonObject sensorTemperature = jsonSensorValuesDoc.createNestedObject("temperature");
    sensorTemperature["value"] = toString(sensors_get_temperature(), 1);
    sensorTemperature["unit"] = "°C";

    JsonObject sensorHumidity = jsonSensorValuesDoc.createNestedObject("humidity");
    sensorHumidity["value"] = toString(sensors_get_humidity());
    sensorHumidity["unit"] = "%";

    JsonObject sensorPressure = jsonSensorValuesDoc.createNestedObject("pressure");
    sensorPressure["value"] = toString(sensors_get_pressure());
    sensorPressure["unit"] = "kPa";

    JsonObject sensorIlluminance = jsonSensorValuesDoc.createNestedObject("illuminance");
    sensorIlluminance["value"] = toString(sensors_get_illuminance());
    sensorIlluminance["unit"] = "lx";

    // prepare json payload for sensors topic
    // serialize json document into global buffer and publish
    // attention: size of buffer is limited to 256 bytes
    serializeJson(jsonSensorValuesDoc, jsonSensorValuesBuffer);
    mqtt_publish("gdc/system/sensors", jsonSensorValuesBuffer);
  }
}

/*
 * This function needs to be called to initialize the watchdog.
 */
void watchdog_init()
{
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // attach own handler which is called if watchdog is not triggered anymore
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

  // led the inbuilt led blink as a heartbeat with 1Hz frequency
  if (timespan_one_second())
  {
    ledState = (ledState == LOW) ? HIGH : LOW;
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