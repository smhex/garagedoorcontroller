// Include libraries
#include <Arduino.h>
#include <MQTTPubSubClient.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

// Global variables defined in main.cpp
extern unsigned long uptime_in_sec;
extern String application;
extern String version;
extern String author;
extern EthernetClient ethClient;

// MQTT broker/topic configuration
MQTTPubSubClient mqttClient;
const char mqttBrokerAddress[] = "mosquitto.debes-online.com";
const unsigned int mqttBrokerPort = 1883;
String mqttClientID = "arduino-gdc";
String mqttUsername = "mosquitto";
String mqttPassword = "mosquitto";
String mqttLastWillMsg = "offline";
String mqttFirstWillMsg = "online";

// list of supported mqtt topic
String mqttTopicSystemUptime = "gdc/system/uptime";
String mqttTopicSystemInfo = "gdc/system/info";
String mqttTopicSystemStatus = "gdc/system/status";
String mqttTopicControlSetNewDoorState = "gdc/control/setnewdoorstate";
String mqttTopicControlGetNewDoorState = "gdc/control/getnewdoorstate";
String mqttTopicControlGetCurrentDoorState = "gdc/control/getcurrentdoorstate";
String mqttTopicControlCommandSource = "gdc/control/commandsource";

// list of support commands and states
String commandDoorOpen = "open";
String commandDoorClose = "close";
String statusDoorOpen = "open";
String statusDoorClosed = "closed";
String statusDoorOpening = "opening";
String statusDoorClosing = "closing";
String statusDoorStopped = "stopped";
String statusDoorUnknown = "unknown";

// list of command sources
String commandSourceRemote = "remote";
String commandSourceLocal = "local";
String comanndSourceRC = "rc";

// global buffer for dealing with json packets
StaticJsonDocument<128> jsonDoc;
char jsonBuffer[128];

// handler for mqtt receive
void onTopicControlSetNewDoorStateReceived(const String &payload, const size_t size);
void mqtt_publish(String topic, String payload);

/*
* Creates the connection to the MQTT broker and performs the authentificaton. The 
* topics will be initially published and the subscriptions will created
*/
void mqtt_init()
{
    // set mqtt client options
    mqttClient.setKeepAliveTimeout(60);
    mqttClient.setCleanSession(true);
    mqttClient.setWill(mqttTopicSystemStatus, mqttLastWillMsg, true, 0);

    // Connect to the broker
    Serial.print("INIT: Connecting mqtt broker...");
    while (!ethClient.connect(mqttBrokerAddress, mqttBrokerPort))
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" success");

    // Authenticate the client
    mqttClient.begin(ethClient);
    Serial.print("INIT: Logging into mqtt broker...");
    while (!mqttClient.connect(mqttClientID, mqttUsername, mqttPassword))
    {
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
    mqttClient.publish(mqttTopicSystemInfo, jsonBuffer, true, 0);
    mqttClient.publish(mqttTopicControlSetNewDoorState, commandDoorOpen);
    mqttClient.publish(mqttTopicControlGetNewDoorState, statusDoorOpen);
    mqttClient.publish(mqttTopicControlGetCurrentDoorState, statusDoorOpen);

    // Subscribe command topic
    mqttClient.subscribe(mqttTopicControlSetNewDoorState, &onTopicControlSetNewDoorStateReceived);
}

/*
 * This handler is called when a subscribed topic is received.
 */
void onTopicControlSetNewDoorStateReceived(const String &payload, const size_t size)
{

    // Copy command topic back
    if
        (!(payload == commandDoorOpen || payload == commandDoorClose))
        {
            Serial.println("RUN: recv: set " + mqttTopicControlSetNewDoorState + " to " + payload + " (invalid)");
        }
    else
    {
        Serial.println("RUN: recv: set " + mqttTopicControlSetNewDoorState + " to " + payload);
        mqtt_publish(mqttTopicControlGetNewDoorState, payload);
        mqtt_publish(mqttTopicControlGetCurrentDoorState, payload);
        mqtt_publish(mqttTopicControlCommandSource, commandSourceRemote);
    }
}

/*
 * This function publishes a topic.
 */
void mqtt_publish(String topic, String payload)
{
    Serial.println("RUN: send: set " + topic + " to " + payload);
    mqttClient.publish(topic, payload);
}

/*
 * This function is manages the mqtt connection and publishes the uptime every sec.
 */
void mqtt_loop()
{
    // if connection to the broker is lost, try to reconnect
    if (!mqttClient.isConnected())
    {
        Serial.println("RUN: Lost connection to mqtt broker. Trying to reconnect");
        mqtt_init();
    }
    else
    {
        mqttClient.update();
    }

    // publish uptime message and online status every 1s
    static uint32_t prev_ms = millis();
    char buffer[12];
    sprintf(buffer, "%lu", uptime_in_sec);
    if (millis() > prev_ms + 1000)
    {
        prev_ms = millis();
        mqttClient.publish(mqttTopicSystemUptime, buffer);
        mqttClient.publish(mqttTopicSystemStatus, mqttFirstWillMsg, true, 0);
    }

    // let other loops run
    yield();
}
