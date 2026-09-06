// Include libraries
#include <Arduino.h>

#include <SPI.h>
#include <Ethernet.h>
#include <WDTZero.h>
#include <ArduinoJson.h>

// Include local libraries/headers
#include "config.h"
#include "driveio.h"
#include "door_state.h"
#include "hmi.h"
#include "util.h"
#include "mqtt.h"
#include "sensors.h"
#include "network.h"
#include "input_capture.h"
#include "time_math.h"

EthernetClient ethClient;

// Heartbeat counter
unsigned long uptime_in_secs = 0;
bool mainFirstRun = true;

// Watchdog
WDTZero watchdog;

unsigned long millisWhenStarted_ms;
int ledState = LOW;

// maintain door status
int lastCommand = 0;

// initial page to display on the display after system start
int currentSystemInfoPage = PAGE_OVERVIEW;

unsigned long prev_displayTimeout_ms = 0;
bool displayIsOn = false;

// Forward declarations
void watchdog_init();
void watchdog_reset();
void watchdog_onShutdown();
void publish_sensor_values();
void command_open(String fromSource);
void command_close(String fromSource);
void show_door_state();
void show_systeminfo();
void show_page_sensors();
void show_page_overview();
void show_page_driveio();
void show_page_hmi();
void show_page_mqtt();
void show_page_system();

// Type d in the USB serial monitor to capture input levels for 30 seconds.
bool input_diagnostic_loop()
{
  static InputCapture<256> capture;
  static bool active = false;
  static unsigned long startedMs = 0;
  static unsigned long watchdogMs = 0;
  if (!active) {
    if (driveio_doorcommandactive() || mqtt_isrestartrequested()) return false;
    if (!Serial.available() || Serial.read() != 'd') return false;
    // Prevent commands arriving on the old MQTT session during the pause.
    ethClient.stop();
    Serial.println("DIAG: START 30 seconds; use original remote only. Inputs D1/D3; Arduino commands paused.");
    Serial.flush();
    startedMs = watchdogMs = millis();
    watchdog.clear();
    capture.reset(micros());
    active = true;
  }
  const uint8_t levels = (digitalRead(STATUS_DOORISOPEN_INPUT) ? 1 : 0) |
                         (digitalRead(STATUS_DOORISCLOSED_INPUT) ? 2 : 0);
  capture.record(micros(), levels);
  if (millis() - watchdogMs >= 100) {
    watchdog.clear();
    watchdogMs = millis();
  }
  if (millis() - startedMs < 30000) return true;

  Serial.println("DIAG: END; buffered transitions follow (time from capture start)");
  for (size_t i = 0; i < capture.count; ++i) {
    char line[80];
    snprintf(line, sizeof(line), "DIAG: t=%lu us D1=%u D3=%u",
             static_cast<unsigned long>(capture.samples[i].us),
             static_cast<unsigned>(capture.samples[i].levels & 1),
             static_cast<unsigned>((capture.samples[i].levels >> 1) & 1));
    Serial.println(line);
    watchdog.clear();
  }
  Serial.print("DIAG: reads="); Serial.print(capture.reads);
  Serial.print(" max_gap_us="); Serial.print(capture.maxGapUs);
  Serial.print(" dropped_transitions="); Serial.println(capture.dropped);
  // External activity invalidates motion inferred before the capture.
  doorState = DoorStateTracker();
  active = false;
  Serial.println("DIAG: normal operation resumes; MQTT reconnects");
  return true;
}

// Buffer input transitions during pulses; Serial output only runs after release.
void trace_drive_inputs(bool pulseActiveAtSample)
{
  struct Sample { unsigned long time; int state; bool active; };
  static Sample samples[16];
  static unsigned int count = 0;
  static unsigned int dropped = 0;
  static int previous = -1;
  const int input = driveio_getcurrentdoorstatus();
  if (input != previous) {
    previous = input;
    if (count < 16) samples[count++] = {millis(), input, pulseActiveAtSample};
    else ++dropped;
  }
  if (driveio_doorcommandactive()) return;
  for (unsigned int i = 0; i < count; ++i) {
    char line[100];
    snprintf(line, sizeof(line), "IO: t=%lu ms raw=%d pulse=%d (0=external,1=open,2=closed,3=between)",
             samples[i].time, samples[i].state, samples[i].active ? 1 : 0);
    Serial.println(line);
  }
  if (dropped) {
    Serial.print("IO: dropped transitions: ");
    Serial.println(dropped);
  }
  count = dropped = 0;
}

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

  watchdog_reset();
  network_init();
  watchdog_reset();
  mqtt_init();

}

// main loop - reads/writes commands and sensor values
void loop()
{
  // calculate uptime in seconds
  uptime_in_secs = (millis() - millisWhenStarted_ms) / 1000;
  if (input_diagnostic_loop()) return;

  // loop over all modules
  // driveio_loop samples inputs before releasing an expired command output.
  const bool pulseActiveAtSample = driveio_doorcommandactive();
  driveio_loop();
  trace_drive_inputs(pulseActiveAtSample);
  if (driveio_doorcommandactive()) {
    if (!mqtt_isrestartrequested()) watchdog.clear();
    return;
  }
  int completedPin;
  unsigned long pulseDuration;
  if (driveio_takepulsereport(&completedPin, &pulseDuration)) {
    Serial.print("IO: pulse complete Arduino D");
    Serial.print(completedPin);
    Serial.print(" HIGH duration=");
    Serial.print(pulseDuration);
    Serial.println(" ms (software timing)");
  }
  doorState.observe(driveio_getcurrentdoorstatus(), pulseActiveAtSample);
  show_door_state();
  hmi_loop();
  sensors_loop();
  if (!driveio_doorcommandactive() && !mqtt_isrestartrequested())
  {
    watchdog_reset();
    network_loop();
    watchdog_reset();
    mqtt_loop();
  }

  // gets the current sensor values and sends them via mqtt
  publish_sensor_values();

  // trigger the watchdog if there is no restart requested
  // note: the restart is executed after the watchdog timeout has reached (default 16s)
  if (!mqtt_isrestartrequested())
  {
    watchdog_reset();
  }

  int oldInput = 0;
  int newInput = 0;
  if (driveio_doorstatuschanged(&oldInput, &newInput) && newInput == DOORSTATUSEXTERNAL)
    mqtt_publish(MQTT_TOPICCONTROLCOMMANDSOURCE, MQTT_COMMANDSOURCEEXTERNAL, false);

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

  if (driveio_doorcommandactive()) return;

  if (displayIsOn)
  {
    show_systeminfo();
    if (time_elapsed(millis(), prev_displayTimeout_ms, displayTimeout_ms))
    {
      displayIsOn = false;
      hmi_display_off(displayIsOn);
    }
  }
  mainFirstRun = false;
}

/*
 * Issue a direction pulse without assuming that a repeated command stops motion.
 */
void command_door(int direction, String fromSource)
{
  if (mqtt_isrestartrequested()) {
    Serial.println("RUN: Command ignored: restart armed");
    return;
  }
  if (driveio_doorcommandactive()) {
    Serial.println("RUN: Command ignored: drive pulse active");
    return;
  }
  if (!doorState.command(direction)) {
    Serial.println("RUN: Command ignored: target end position already reached");
    return;
  }
  Serial.print("RUN: Command: ");
  Serial.print(direction == DOORCOMMANDOPEN ? "DOOROPEN" : "DOORCLOSE");
  Serial.println(" (source=" + fromSource + ")");
  mqtt_publish(MQTT_TOPICCONTROLCOMMANDSOURCE, fromSource, false);
  driveio_setdoorcommand(direction);
}

void command_open(String fromSource)
{
  command_door(DOORCOMMANDOPEN, fromSource);
}

void command_close(String fromSource)
{
  command_door(DOORCOMMANDCLOSE, fromSource);
}

// LEDs follow the same logical state as MQTT. Unknown: both off.
void show_door_state()
{
  static DoorState displayed = DoorState::Unknown;
  static bool initialized = false;
  if (initialized && displayed == doorState.state) return;
  displayed = doorState.state;
  initialized = true;
  hmi_setled_blinking(HMI_LED_DOOROPEN, displayed == DoorState::Opening);
  hmi_setled_blinking(HMI_LED_DOORCLOSED, displayed == DoorState::Closing);
  if (displayed != DoorState::Opening)
    hmi_setled(HMI_LED_DOOROPEN, displayed == DoorState::Open ? HIGH : LOW);
  if (displayed != DoorState::Closing)
    hmi_setled(HMI_LED_DOORCLOSED, displayed == DoorState::Closed ? HIGH : LOW);
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
    const bool valid = sensors_isvalid();
    mqtt_publish("gdc/system/sensors/status", valid ? "available" : "unavailable", true);
    if (!valid) return;
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
    mqtt_publish("gdc/system/sensors", jsonSensorValuesBuffer, false);
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
  if (!sensors_isvalid()) {
    String text[2] = {"Sensor values", "unavailable"};
    hmi_display_frame("Sensors", text, 2);
    return;
  }
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
  secs = uptime_in_secs;
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
