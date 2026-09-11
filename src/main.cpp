#include "debug_console.h"
#include "lan_update.h"
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
void command_door(int direction, String fromSource);
void show_door_state();
void show_systeminfo();
void show_page_sensors();
void show_page_overview();
void show_page_driveio();
void show_page_network();
void show_page_system();
void show_page_update();

const char* door_state_name(DoorState state)
{
  switch (state)
  {
  case DoorState::Open: return "OPEN";
  case DoorState::Closed: return "CLOSED";
  case DoorState::Opening: return "OPENING";
  case DoorState::Closing: return "CLOSING";
  case DoorState::Stopped: return "STOPPED";
  default: return "UNKNOWN";
  }
}

String network_status()
{
  if (Ethernet.hardwareStatus() == EthernetNoHardware) return "NO HW";
  if (Ethernet.linkStatus() == LinkOFF) return "LINK DOWN";
  if (Ethernet.linkStatus() == Unknown) return "LINK UNKNOWN";
  if (!network_isready()) return "DHCP...";
  return "OK " + IPAddressToString(Ethernet.localIP());
}

String mqtt_status()
{
  if (!network_isready()) return "WAIT NET";
  return mqtt_getstatus_text();
}

String info_page_title(const char* name, int page)
{
  const int pageCount = lan_update_available() ? 6 : 5;
  return String(name) + " " + String(page) + "/" + String(pageCount);
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
    Debug.println(line);
  }
  if (dropped) {
    Debug.print("IO: dropped transitions: ");
    Debug.println(dropped);
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
  Debug.println("INIT: Starting...");
  Debug.print("INIT: Firmware version: ");
  Debug.println(version);
  Debug.println("INIT: Sketch built on " __DATE__ " at " __TIME__);

  // check if all the hardware is installed/present
  // start with MKR ENV shield
  sensors_init();

  // init baseboard
  driveio_init();

  watchdog_reset();
  network_init();
  watchdog_reset();
  mqtt_init();
  lan_update_init();

}

// main loop - reads/writes commands and sensor values
void loop()
{
  // calculate uptime in seconds
  uptime_in_secs = (millis() - millisWhenStarted_ms) / 1000;
  // loop over all modules
  // driveio_loop samples inputs before releasing an expired command output.
  const bool pulseActiveAtSample = driveio_doorcommandactive();
  driveio_loop();
  int startedPin;
  unsigned long startedAt;
  if (driveio_takepulsestartreport(&startedPin, &startedAt))
    doorState.travelStarted(startedPin == CMD_OPENDOOR_OUTPUT ? DOORCOMMANDOPEN : DOORCOMMANDCLOSE, startedAt);
  trace_drive_inputs(pulseActiveAtSample);
  if (driveio_doorcommandactive()) {
    if (!mqtt_isrestartrequested()) watchdog.clear();
    return;
  }
  int completedPin;
  unsigned long pulseDuration;
  if (driveio_takepulsereport(&completedPin, &pulseDuration)) {
    Debug.print("IO: pulse complete Arduino D");
    Debug.print(completedPin);
    Debug.print(" HIGH duration=");
    Debug.print(pulseDuration);
    Debug.println(" ms (software timing)");
  }
  doorState.observe(driveio_getcurrentdoorstatus(), pulseActiveAtSample);
  show_door_state();
  hmi_loop();
  const bool updateWasBusy = lan_update_busy();
  lan_update_loop();
  if (updateWasBusy || lan_update_busy()) {
    static unsigned long lastUpdateFrame = 0;
    currentSystemInfoPage = PAGE_UPDATE;
    displayIsOn = true;
    prev_displayTimeout_ms = millis();
    if (!updateWasBusy) hmi_display_off(true);
    if (!updateWasBusy || !lan_update_busy() || millis() - lastUpdateFrame >= 250) {
      show_page_update();
      lastUpdateFrame = millis();
    }
    watchdog_reset();
    mqtt_loop();
    debug_console_loop();
    return;
  }
  sensors_loop();
  if (!driveio_doorcommandactive() && !mqtt_isrestartrequested())
  {
    watchdog_reset();
    network_loop();
    watchdog_reset();
    mqtt_loop();
    debug_console_loop();
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
    mqtt_note_door_command(MQTT_COMMANDSOURCEEXTERNAL);

  // check for user command (button press on HMI)
  int buttonPressed = hmi_getbuttonpressed();
  if (hmi_take_info_long_press() && currentSystemInfoPage == PAGE_UPDATE && lan_update_available())
  {
    lan_update_request_install(MQTT_COMMANDSOURCELOCAL);
    displayIsOn = true;
    prev_displayTimeout_ms = millis();
    hmi_display_off(true);
  }
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
        if (currentSystemInfoPage == PAGE_UPDATE ||
            (currentSystemInfoPage == PAGE_SYSTEM && !lan_update_available()))
        {
          // start over with first page agin
          currentSystemInfoPage = PAGE_OVERVIEW;
        }
        else
        {
          //switch to next page
          ++currentSystemInfoPage;
        }
      }
      else
      {
        currentSystemInfoPage = PAGE_OVERVIEW;
      }
      char buffer[80];
      sprintf(buffer, "RUN: SYSINFO: %d", currentSystemInfoPage);
      Debug.println(buffer);
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
    if (remoteCommand == MQTT_COMMANDDOORSTOP && doorState.isMoving())
    {
      command_door(doorState.target, MQTT_COMMANDSOURCEREMOTE);
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
 * A second command during travel is an implicit stop. The drive accepts either
 * command input for this, but pulse the output that started the current travel.
 */
void command_door(int direction, String fromSource)
{
  if (lan_update_busy()) return;
  lan_update_note_door_command(fromSource);
  if (mqtt_isrestartrequested()) {
    Debug.println("RUN: Command ignored: restart armed");
    return;
  }
  if (driveio_doorcommandactive()) {
    Debug.println("RUN: Command ignored: drive pulse active");
    return;
  }
  bool implicitStop = doorState.isMoving();
  int pulseDirection = direction;
  if (implicitStop) {
    pulseDirection = doorState.stop();
    Debug.print("RUN: Command: DOORSTOP (source=");
    Debug.print(fromSource);
    Debug.println(")");
  } else if (!doorState.command(direction)) {
    Debug.println("RUN: Command ignored: target end position already reached");
    return;
  }
  if (!implicitStop) {
    Debug.print("RUN: Command: ");
    Debug.print(direction == DOORCOMMANDOPEN ? "DOOROPEN" : "DOORCLOSE");
    Debug.println(" (source=" + fromSource + ")");
  }
  mqtt_note_door_command(fromSource);
  driveio_setdoorcommand(pulseDirection);
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
  case PAGE_NETWORK:
    show_page_network();
    break;
  case PAGE_SENSORS:
    show_page_sensors();
    break;
  case PAGE_DRIVEIO:
    show_page_driveio();
    break;
  case PAGE_SYSTEM:
    show_page_system();
    break;
  case PAGE_UPDATE:
    show_page_update();
    break;
  }
}

/*
 * Gets all the sensor values and publishes them as json string
 */
void publish_sensor_values()
{
  // Included in the retained per-controller MQTT state by mqtt_loop().
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
  Debug.print("\nERROR: watchdog not cleared. Controller reboot initiated");
}

/*
* Displays the application overview page on the HMI OLED display
*/
void show_page_overview()
{
  String text[4] = {
    "FW " + version + (lan_update_available() ? " [*]" : ""),
    "Door " + String(door_state_name(doorState.state)),
    "ETH " + network_status(),
    "MQTT " + mqtt_status()
  };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame(application, text, len);
}

void show_page_network()
{
  String text[4] = {
    "IP: " + IPAddressToString(Ethernet.localIP()),
    "Link: " + String(Ethernet.linkStatus() == LinkON ? "UP" :
                        Ethernet.linkStatus() == LinkOFF ? "DOWN" : "UNKNOWN"),
    "DHCP: " + String(network_isready() ? "OK" : "WAIT"),
    "MQTT: " + mqtt_status()
  };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame(info_page_title("Network", 2), text, len);
}

void show_page_update()
{
  String lines[4];
  if (!lan_update_display(lines)) {
    lines[0] = "No update staged";
    hmi_display_frame("Firmware Update", lines, 1);
    return;
  }
  hmi_display_frame("Firmware Update", lines, 4);
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
  hmi_display_frame(info_page_title("Sensors", 3), text, len);
}

void show_page_driveio()
{
  String text[4] = {
      "D0 (Output): " + String(driveio_getiostatus(CMD_OPENDOOR_OUTPUT)),
      "D1 (Input): " + String(driveio_getiostatus(STATUS_DOORISOPEN_INPUT)),
      "D2 (Output): " + String(driveio_getiostatus(CMD_CLOSEDOOR_OUTPUT)),
      "D3 (Input): " + String(driveio_getiostatus(STATUS_DOORISCLOSED_INPUT))};
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame(info_page_title("Door I/O", 4), text, len);
}

/*
* Displays led states
*/
/*
* Display system information
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
      "Uptime: " + String(buffer),
      "Copyright " + author,
      "Update: " + String(lan_update_available() ? "AVAILABLE" : "NONE"),
    };
  int len = sizeof(text) / sizeof(text[0]);
  hmi_display_frame(info_page_title("System", 5), text, len);
}
