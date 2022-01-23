/* 
* File:     main.cpp
* Date:     19.01.2021
* Version:  v0.1.5
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

#define PAGE_OVERVIEW 0
#define PAGE_SENSORS 1
#define PAGE_DRIVEIO 2
#define PAGE_HMI 3
#define PAGE_MQTT 4
#define PAGE_SYSTEM 5

// Network configuration - sets MAC and IP address
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 30, 241);
IPAddress dns(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 30, 1);
EthernetClient ethClient;

// global settings
String application = "GarageDoorController";
String version = "0.1.5";
String author = "smhex";

// global buffer for dealing with json packets

// Heartbeat counter
unsigned long uptime_in_sec = 0;
bool mainFirstRun = true;

// Watchdog
WDTZero watchdog;

static unsigned long millisWhenStarted_ms;
int ledState = LOW;

// maintain door status
int oldDoorStatus = 0;
int newDoorStatus = 0;
int lastCommand = 0;

// initial page to display on the display after system start
int currentSystemInfoPage = PAGE_OVERVIEW;

// 30s timeout for OLED display in HMI module
int displayTimeout_ms = 30000;
unsigned long prev_displayTimeout_ms = 0;
bool displayIsOn = false;

// Forward declarations
void watchdog_init();
void watchdog_reset();
void watchdog_onShutdown();
void publish_sensor_values();
void command_open(String fromSource);
void command_close(String fromSource);
void status_isopen();
void status_isclosed();
void status_ismovingorstopped();
void show_systeminfo();
void show_page_sensors();
void show_page_overview();
void show_page_driveio();
void show_page_hmi();
void show_page_mqtt();
void show_page_system();

// setup the board an all variables
void setup()
{
  // Init serial line with 9600 baud and wait 5s to get a terminal connected
  Serial.begin(9600);
  delay(2000);

  // setup watchdog
  watchdog_init();

  // initialize display
  hmi_init();

  // store offset for uptime counter
  millisWhenStarted_ms = millis();

  // show initial screen
  displayIsOn = true;
  prev_displayTimeout_ms = millis();
  hmi_display_off(displayIsOn);
  show_page_overview();

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

  // Initialize MQTT client
  mqtt_init();
}

// main loop - reads/writes commands and sensor values
void loop()
{
  // calculate uptime in seconds
  uptime_in_sec = (millis() - millisWhenStarted_ms) / 1000;

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
    if ((newDoorStatus == DOORSTATUSOPEN) && (driveio_doorcommandactive()==false))
    {
      status_isopen();
    }
    if ((newDoorStatus == DOORSTATUSCLOSED) && (driveio_doorcommandactive()==false))
    {
      status_isclosed();
    }
    if (newDoorStatus == DOORSTATUSMOVINGORSTOPPED)
    {
      status_ismovingorstopped();
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
    if (buttonPressed == HMI_BUTTON_SYSTEMINFO)
    {
      // change page if display is on - otherwise button press will
      // only activate the display again
      if (displayIsOn)
      {
        if (currentSystemInfoPage == PAGE_SYSTEM)
        {
          // start over with first page agin
          currentSystemInfoPage = PAGE_OVERVIEW;
        }
        else
        {
          //switch to next page
          currentSystemInfoPage++;
        }
      }
      char buffer[80];
      sprintf(buffer, "RUN: SYSINFO: %d", currentSystemInfoPage);
      Serial.println(buffer);
      displayIsOn = true;
      prev_displayTimeout_ms = millis();
      hmi_display_off(displayIsOn);
    }
  }

  // check for remote command (over MQTT)
  String remoteCommand = mqtt_getcommand();
  if (remoteCommand.length() != 0)
  {
    if (remoteCommand == MQTT_COMMANDDOOROPEN)
    {
      command_open(MQTT_COMMANDSOURCEREMOTE);
    }
    if (remoteCommand == MQTT_COMMANDDOORCLOSE)
    {
      command_close(MQTT_COMMANDSOURCEREMOTE);
    }
  }

  if (displayIsOn)
  {
    show_systeminfo();
    if (millis() > prev_displayTimeout_ms + displayTimeout_ms)
    {
      displayIsOn = false;
      hmi_display_off(displayIsOn);
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
  sprintf(buffer, "RUN: Command: DOOROPEN (source=%s)", fromSource.c_str());
  Serial.println(buffer);

  mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOOROPEN);
  mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOOROPENING);
  mqtt_publish(MQTT_TOPICCONTROLCOMMANDSOURCE, fromSource);

  driveio_setdoorcommand(DOORCOMMANDOPEN);

  hmi_setled_blinking(HMI_LED_DOORCLOSED, false);
  hmi_setled_blinking(HMI_LED_DOOROPEN, true);
  hmi_setled(HMI_LED_DOORCLOSED, LOW);
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

  hmi_setled(HMI_LED_DOOROPEN, LOW);
  hmi_setled_blinking(HMI_LED_DOOROPEN, false);
  hmi_setled_blinking(HMI_LED_DOORCLOSED, true);
}

/*
 *  door is open
 */
void status_isopen()
{
  Serial.println("RUN: STATUS: DOOROPEN");

  mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOOROPEN);
  mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOOROPEN);

  hmi_setled_blinking(HMI_LED_DOOROPEN, false);
  hmi_setled_blinking(HMI_LED_DOORCLOSED, false);
  hmi_setled(HMI_LED_DOOROPEN, HIGH);
  hmi_setled(HMI_LED_DOORCLOSED, LOW);
}

/*
 *  door is closed
 */
void status_isclosed()
{
  Serial.println("RUN: STATUS: DOORCLOSED");

  mqtt_publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, MQTT_STATUSDOORCLOSED);
  mqtt_publish(MQTT_TOPICCONTROLGETNEWDOORSTATE, MQTT_COMMANDDOORCLOSE);

  hmi_setled_blinking(HMI_LED_DOOROPEN, false);
  hmi_setled_blinking(HMI_LED_DOORCLOSED, false);
  hmi_setled(HMI_LED_DOOROPEN, LOW);
  hmi_setled(HMI_LED_DOORCLOSED, HIGH);
}

/*
 *  door is closed
 */
void status_ismovingorstopped()
{
  Serial.println("RUN: STATUS: DOORMOVINGORSTOPPED");
}

/*
 * shows the next system info page on the OLED
 */
void show_systeminfo()
{
  switch (currentSystemInfoPage)
  {
  case PAGE_OVERVIEW:
    show_page_overview();
    break;
  case PAGE_SENSORS:
    show_page_sensors();
    break;
  case PAGE_DRIVEIO:
    show_page_driveio();
    break;
  case PAGE_HMI:
    show_page_hmi();
    break;
  case PAGE_MQTT:
    show_page_mqtt();
    break;
  case PAGE_SYSTEM:
    show_page_system();
    break;
  }
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
    sensorIlluminance["value"] = toString(sensors_get_illuminance(),4);
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

/*
* Displays the application overview page on the HMI OLED display
*/
void show_page_overview()
{
  String ethStatus = (ethClient.connected()==true) ? "connected" : "disconnected";
  String mqttStatus = (mqtt_isconnected()==true) ? "connected" : "disconnected";
  String text[4] = {
    "Version " + version, 
    "Copyright " + author, 
    "Ethernet " + ethStatus,
    "MQTT " + mqttStatus
  };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame(application, text, len);
}

/*
* Display the sensor values
*/
void show_page_sensors()
{
  String text[4] = {
      "Temperature: " + toString(sensors_get_temperature(), 1) + "\xb0" + "C",
      "Humidity: " + toString(sensors_get_humidity()) + "%",
      "Pressure: " + toString(sensors_get_pressure()) + "kPa",
      "Illuminance: " + toString(sensors_get_illuminance()) + "lx"};
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame("Sensors", text, len);
}

void show_page_driveio()
{
  String text[4] = {
      "D0 (Output): " + String(driveio_getiostatus(CMD_OPENDOOR_OUTPUT)),
      "D1 (Input): " + String(driveio_getiostatus(STATUS_DOORISOPEN_INPUT)),
      "D2 (Output): " + String(driveio_getiostatus(CMD_CLOSEDOOR_OUTPUT)),
      "D3 (Input): " + String(driveio_getiostatus(STATUS_DOORISCLOSED_INPUT))};
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame("DRIVEIO", text, len);
}

/*
* Displays led states
*/
void show_page_hmi()
{
  String text[3] = {
      "Led 1: " + String(hmi_getled(HMI_LED_DOOROPEN)),
      "Led 2: " + String(hmi_getled(HMI_LED_SYSTEMINFO)),
      "Led 3: " + String(hmi_getled(HMI_LED_DOORCLOSED)),
  };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame("HMI", text, len);
}

/*
* Display the number of sent and received packets
*/
void show_page_mqtt()
{
  String text[3] = {
      "Msg.Sent: " + String(mqtt_getpacketssent()),
      "Msg.Received: " + String(mqtt_getpacketsreceived()),
      "Connected: " + String(mqtt_isconnected()),
  };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame("MQTT", text, len);
}

/*
* Display the IP, Link status and Uptime
*/
void show_page_system()
{
  unsigned int days=0;
  unsigned int hours=0;
  unsigned int mins=0;
  unsigned int secs=0;
  secs = uptime_in_sec;
  mins=secs/60; 
  hours=mins/60; 
  days=hours/24; 
  secs=secs-(mins*60);  
  mins=mins-(hours*60); 
  hours=hours-(days*24); 

  char buffer[80];
  sprintf(buffer, "%u.%02u:%02u:%02u", days, hours, mins, secs);
  String text[3] = {
      "IP: " + IPAddressToString(Ethernet.localIP()),
      "Link: " + String(Ethernet.linkStatus()),
      "Uptime: " + String(buffer),
  };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame("System", text, len);
}