/* 
* File:     main.cpp
* Date:     04.01.2021
* Version:  v0.0.2
* Author:   smhex
*/

// Include libraries
#include <Arduino.h>
#include <Arduino_MKRENV.h>
#include <MQTTPubSubClient.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WDTZero.h>

// MAC address from shield
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// Network configuration
IPAddress ip(192, 168, 30, 241);
IPAddress dns(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 30, 1);
EthernetClient client;

// MQTT broker/topic configuration

// Watchdog
WDTZero watchdog;   

// Heartbeat counter
static unsigned long uptime_in_sec = 0;
static unsigned long last_milliseconds;  

// Forward declarations
void watchdog_handler();

// setup the board an all variables
void setup() {
  
  // Init serial line with 9600 baud
  Serial.begin(9600);
  while (!Serial);

   // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  last_milliseconds = millis();

  // Initial delay to get the serial monitor attached after port is availabe for host
  delay(1000);

  // setup watchdog 16s interval
  watchdog.attachShutdown(watchdog_handler);
  watchdog.setup(WDT_SOFTCYCLE16S);

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
}

// main loop - reads/writes commands and sensor values
void loop() {
  // read all the sensor values
  float temperature = ENV.readTemperature();
  float humidity    = ENV.readHumidity();
  float pressure    = ENV.readPressure();
  float illuminance = ENV.readIlluminance();  

  // print each of the sensor values
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity    = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Pressure    = ");
  Serial.print(pressure);
  Serial.println(" kPa");

  Serial.print("Illuminance = ");
  Serial.print(illuminance);
  Serial.println(" lx");

  // print an empty line
  Serial.println();

  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second

  // calculate uptime
  uptime_in_sec = (millis()-last_milliseconds)/1000;
  Serial.print("RUN: Uptime ");
  Serial.println(uptime_in_sec);

  // Trigger watchdog
  watchdog.clear();
}

/*
 * This function is called of the watchdog is not cleared. This usally happens if
 * the processor is stalled.
 */
void watchdog_handler()
{
  Serial.print("\nERROR: watchdog not cleared. Controller reboot initiated");
}