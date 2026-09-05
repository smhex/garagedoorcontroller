// Include libraries
#include <Arduino.h>
#include <MQTTPubSubClient.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "config.h"
#include "mqtt.h"
#include "mqtt_log.h"
#include "network.h"
#include "driveio.h"
#include "door_state.h"
#include <Dns.h>

// MQTT broker/topic configuration
// 256 bytes need to publish the sensors topic
MQTTPubSub::PubSubClient<256> mqttClient;

String command ="";

int numPacketsReceived = 0;
int numPacketsSent = 0;

bool mqttFirstRun = true;
bool mqttInitialized = false;
unsigned long lastConnectAttempt_ms = 0;
bool connectAttempted = false;
bool isRestartRequested = false;
bool doorStatePublished = false;
DoorState publishedDoorState = DoorState::Unknown;
int publishedDoorTarget = 0;

// Retain the latest logical state across suppressed sends and network outages.
void mqtt_publish_door_state()
{
    if (!mqttClient.isConnected() || driveio_doorcommandactive()) return;
    if (doorStatePublished && publishedDoorState == doorState.state &&
        publishedDoorTarget == doorState.target) return;
    const char* state = MQTT_STATUSDOORUNKNOWN;
    switch (doorState.state) {
        case DoorState::Open: state = MQTT_STATUSDOOROPEN; break;
        case DoorState::Closed: state = MQTT_STATUSDOORCLOSED; break;
        case DoorState::Opening: state = MQTT_STATUSDOOROPENING; break;
        case DoorState::Closing: state = MQTT_STATUSDOORCLOSING; break;
        case DoorState::Stopped: state = MQTT_STATUSDOORSTOPPED; break;
        case DoorState::Unknown: break;
    }
    if (!mqttClient.publish(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, state, true, 0)) return;
    numPacketsSent++;
    if (doorState.target != 0) {
        if (!mqttClient.publish(MQTT_TOPICCONTROLGETNEWDOORSTATE,
            doorState.target == DOORCOMMANDOPEN ? MQTT_COMMANDDOOROPEN : MQTT_COMMANDDOORCLOSE,
            true, 0)) return;
        numPacketsSent++;
    }
    Serial.print("RUN: Door state published: ");
    Serial.println(state);
    publishedDoorState = doorState.state;
    publishedDoorTarget = doorState.target;
    doorStatePublished = true;
}

// handler for subscribed topics (mqtt receive)
void onTopicControlSetNewDoorStateReceived(const String &payload, const size_t size);
void onTopicSystemRestartReceived(const String &payload, const size_t size);

/*
* Configures the client. Connection attempts are performed by mqtt_loop().
*/
void mqtt_init()
{
    if (mqttInitialized) return;

    // set mqtt client options
    mqttClient.setKeepAliveTimeout(60);
    mqttClient.setCleanSession(true);

    // Lastwill topic is equal to system status topic
    // MQTTPubSubClient 0.1.2 stores the topic's buffer without copying it.
    // Keep it alive for the later connection attempts and reconnects.
    static String mqttLastWillTopic = MQTT_TOPICSYSTEMSTATUS;
    mqttClient.setWill(mqttLastWillTopic, mqttLastWillMsg, true, 0);

    mqttClient.setTimeout(1000);
    ethClient.setConnectionTimeout(1000);
    mqttClient.begin(ethClient);
    mqttInitialized = true;
}

// One bounded attempt; retries are scheduled by mqtt_loop().
void mqtt_connect()
{
    connectAttempted = true;
    ethClient.stop();
    DNSClient resolver;
    IPAddress brokerIP;
    resolver.begin(Ethernet.dnsServerIP());
    Serial.print("MQTT: DNS server: ");
    Serial.println(Ethernet.dnsServerIP());
    int dnsResult = resolver.getHostByName(mqttBrokerAddress, brokerIP, 500);
    if (dnsResult != 1)
    {
        Serial.print("MQTT: DNS lookup failed, code: ");
        Serial.println(dnsResult);
        Serial.println("MQTT: Retry in 10 seconds");
        ethClient.stop();
        lastConnectAttempt_ms = millis();
        return;
    }
    Serial.print("MQTT: Broker IP: ");
    Serial.print(brokerIP);
    Serial.print(":");
    Serial.println(mqttBrokerPort);
    if (!ethClient.connect(brokerIP, mqttBrokerPort))
    {
        Serial.println("MQTT: TCP connection failed; retry in 10 seconds");
        ethClient.stop();
        lastConnectAttempt_ms = millis();
        return;
    }
    Serial.println("MQTT: TCP connected");
    if (!mqttClient.connect(mqttClientID, mqttUsername, mqttPassword))
    {
        Serial.print("MQTT: Handshake failed, library error: ");
        Serial.println(static_cast<int>(mqttClient.getLastError()));
        Serial.println("MQTT: Retry in 10 seconds");
        ethClient.stop();
        lastConnectAttempt_ms = millis();
        return;
    }

    bool controlSubscribed = mqttClient.subscribe(MQTT_TOPICCONTROLSETNEWDOORSTATE, &onTopicControlSetNewDoorStateReceived);
    bool restartSubscribed = mqttClient.subscribe(MQTT_TOPICSYSTEM_RESTART, &onTopicSystemRestartReceived);
    lastConnectAttempt_ms = millis();
    if (!controlSubscribed || !restartSubscribed) {
        Serial.print("MQTT: Subscription failed, library error: ");
        Serial.println(static_cast<int>(mqttClient.getLastError()));
        Serial.println("MQTT: Retry in 10 seconds");
        ethClient.stop();
        return;
    }
    Serial.println("MQTT: Connected");
    mqttFirstRun = true;

    doorStatePublished = false;
    mqtt_publish_door_state();
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
        mqtt_format_received(buffer, sizeof(buffer), MQTT_TOPICCONTROLSETNEWDOORSTATE, payload.c_str(), false);
        Serial.println(buffer);
    }
    else
    {
        mqtt_format_received(buffer, sizeof(buffer), MQTT_TOPICCONTROLSETNEWDOORSTATE, payload.c_str(), true);
        Serial.println(buffer);
        command = payload;
      }
}

/*
 * This handler is called when a subscribed topic (the restart) is received.
 */
void onTopicSystemRestartReceived(const String &payload, const size_t size)
{
    char buffer[80];
    numPacketsReceived++;

    if (payload != MQTT_SYSTEMRESTART)
    {
        mqtt_format_received(buffer, sizeof(buffer), MQTT_TOPICSYSTEM_RESTART, payload.c_str(), false);
        Serial.println(buffer);
    }
    else
    {
        mqtt_format_received(buffer, sizeof(buffer), MQTT_TOPICSYSTEM_RESTART, payload.c_str(), true);
        Serial.println(buffer);
        isRestartRequested = true;
        mqtt_publish(MQTT_TOPICSYSTEM_RESTART, "", false);
      }
}

/*
 * This function publishes a topic. It passes the parameters without change to the
 * underlying mqtt client but adds a serial print for logging purposes
 */
void mqtt_publish(String topic, String payload, bool retain)
{
    if (!mqtt_isconnected() || !network_isready() || driveio_doorcommandactive()) return;
    Serial.println("RUN: Publish: set " + topic + " to " + payload);
    mqttClient.publish(topic, payload, retain, 0);
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
    if (!mqttInitialized || !network_isready() || driveio_doorcommandactive()) return;

    // if connection to the broker is lost, try to reconnect
    if (!mqttClient.isConnected())
    {
        if (!connectAttempted || millis() - lastConnectAttempt_ms >= 10000) mqtt_connect();
    }
    else
    {
        mqttClient.update();
        mqtt_publish_door_state();
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
        sprintf(buffer, "%lu", uptime_in_secs);
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
* Returns true if the client is connected to a broker
*/
bool mqtt_isconnected()
{
    bool retval = false;
    if (mqttInitialized){
        retval = mqttClient.isConnected();
    }
    return retval;
}

/*
* Returns true if a restart request was sent via mqtt
*/
bool mqtt_isrestartrequested()
{
    return isRestartRequested;
}
