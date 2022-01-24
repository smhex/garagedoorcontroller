// Include libraries
#include <Arduino.h>
#include <MQTTPubSubClient.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "mqtt.h"

// Global variables defined in main.cpp
extern unsigned long uptime_in_sec;
extern String application;
extern String version;
extern String author;
extern EthernetClient ethClient;

// MQTT broker/topic configuration
// 256 bytes need to publish the sensors topic
MQTTPubSub::PubSubClient<256> mqttClient;
const char mqttBrokerAddress[] = "mosquitto.debes-online.com";
const unsigned int mqttBrokerPort = 1883;
String mqttClientID = "arduino-gdc";
String mqttUsername = "mosquitto";
String mqttPassword = "mosquitto";
String mqttLastWillMsg = "offline";
String mqttFirstWillMsg = "online";

String command ="";

int numPacketsReceived = 0;
int numPacketsSent = 0;

bool mqttFirstRun = true;
bool mqttInitialized = false;

// handler for mqtt receive
void onTopicControlSetNewDoorStateReceived(const String &payload, const size_t size);
void mqtt_publish(String topic, String payload);

/*
* Creates the connection to the MQTT broker and performs the authentificaton. The 
* topics will be initially published and the subscriptions will be created
*/
void mqtt_init()
{
    // set mqtt client options
    mqttClient.setKeepAliveTimeout(60);
    mqttClient.setCleanSession(true);

    // Lastwill topic is equal to system status topic
    String mqttLastWillTopic = MQTT_TOPICSYSTEMSTATUS;
    mqttClient.setWill(mqttLastWillTopic, mqttLastWillMsg, true, 0);

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

    mqttInitialized = true;
    mqttFirstRun = true;

    // Subscribe command topic
    mqttClient.subscribe(MQTT_TOPICCONTROLSETNEWDOORSTATE, &onTopicControlSetNewDoorStateReceived);
}

/*
 * This handler is called when a subscribed topic (the command) is received.
 */
void onTopicControlSetNewDoorStateReceived(const String &payload, const size_t size)
{
    char buffer[80];
    numPacketsReceived++;

    // Copy command topic back if payload is valid
    if (!(payload == MQTT_COMMANDDOOROPEN || payload == MQTT_COMMANDDOORCLOSE))
    {
        sprintf(buffer,"RUN: Subscribe: set %s to %s (invalid)",MQTT_TOPICCONTROLSETNEWDOORSTATE, payload.c_str());
        Serial.println(buffer);
    }
    else
    {
        sprintf(buffer,"RUN: Subscribe: set %s to %s",MQTT_TOPICCONTROLSETNEWDOORSTATE, payload.c_str());
        Serial.println(buffer);
        command = payload;
      }
}

/*
 * This function publishes a topic. It passes the parameters without change to the
 * underlying mqtt client but adds a serial print for logging purposes
 */
void mqtt_publish(String topic, String payload, bool retain)
{
    Serial.println("RUN: Publish: set " + topic + " to " + payload);
    mqttClient.publish(topic, payload, false, 0);
    numPacketsSent++;
}

/*
 * Returns the
 */
String mqtt_getcommand()
{
    String retval = command;
    command = "";
    return retval;
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
        if (mqttFirstRun)
        {
            // global buffer for dealing with json packets
            DynamicJsonDocument jsonDoc(128);
            char jsonBuffer[128];

            // prepare json payload for info topic
            jsonDoc["application"] = application;
            jsonDoc["version"] = version;
            jsonDoc["author"] = author;

            // serialize json document into global buffer and publish
            // attention: size of buffer is limited to 256 bytes
            serializeJson(jsonDoc, jsonBuffer);
            mqttClient.publish(MQTT_TOPICSYSTEMINFO, jsonBuffer, true, 0);
        }

        // publish uptime message and online status every 1s
        static uint32_t prev_ms = millis();
        char buffer[12];
        sprintf(buffer, "%lu", uptime_in_sec);
        if (millis() > prev_ms + 1000)
        {
            prev_ms = millis();
            mqttClient.publish(MQTT_TOPICSYSTEMUPTIME, buffer);
            numPacketsSent++;
            
            mqttClient.publish(MQTT_TOPICSYSTEMSTATUS, mqttFirstWillMsg, true, 0);
            numPacketsSent++;
        }
        mqttFirstRun = false;
    }

    // let other loops run
    yield();
}

/*
* Returns the number of packets received since start
*/
int mqtt_getpacketsreceived()
{
    return numPacketsReceived;
}

/*
* Returns the number of packets sent since start
*/
int mqtt_getpacketssent()
{
    return numPacketsSent;
}

/*
* Retruns true if the client is connected to a broker
*/
bool mqtt_isconnected()
{
    bool retval = false;
    if (mqttInitialized){
        retval = mqttClient.isConnected();
    }
    return retval;
}