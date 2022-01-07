/* 
* File:   main.cpp
* Date:   04.01.2021
* Author: smhex
*/


// Include libraries
#include <Arduino.h>
#include <Arduino_MKRENV.h>
#include <MQTTPubSubClient.h>
#include <SPI.h>
#include <Ethernet.h>

// MAC address from shield
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// Network configuration
IPAddress ip(192, 168, 40, 19);
IPAddress dns(192, 168, 40, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 40, 1);
EthernetClient client;

// MQTT broker/topic configuration


// setup the board an all variables
void setup() {
  
  // Init serial line with 9600 baud
  Serial.begin(9600);
  while (!Serial);

  // Initial delay to get the serial monitor attached after port is availabe for host
  delay(2000);
  
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
  Ethernet.begin(mac, ip, dns, gateway, subnet);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("ERROR: Ethernet shield was not found");
    while (true) {
      delay(1); 
    }
  }
  else{
    Serial.print("INIT: Ethernet chipset is ");
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
  float uva         = ENV.readUVA();
  float uvb         = ENV.readUVB();
  float uvIndex     = ENV.readUVIndex();     

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

  Serial.print("UVA         = ");
  Serial.println(uva);

  Serial.print("UVB         = ");
  Serial.println(uvb);

  Serial.print("UV Index    = ");
  Serial.println(uvIndex);

  // print an empty line
  Serial.println();

  // wait 1 second to print again
  delay(1000);
}