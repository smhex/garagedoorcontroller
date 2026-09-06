// Include libraries
#include <Arduino.h>
#include <MQTTPubSubClient.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "config.h"
#include "mqtt.h"
#include "mqtt_log.h"
#include "mqtt_delivery.h"
#include "network_info.h"
#include "network.h"
#include "driveio.h"
#include "door_state.h"
#include <Dns.h>

// MQTT broker/topic configuration
// 256 bytes need to publish the sensors topic
MQTTPubSub::PubSubClient<256> mqttClient;

String command ="";

uint32_t numPacketsReceived = 0;
uint32_t numPacketsSent = 0;

bool mqttFirstRun = true;
bool mqttInitialized = false;
unsigned long lastConnectAttempt_ms = 0;
bool connectAttempted = false;
MqttRestart restartRequest;
bool doorStatePublished = false;
DoorState publishedDoorState = DoorState::Unknown;
int publishedDoorTarget = 0;

bool mqtt_send(const String& topic, const String& payload, bool retain, int qos = 0)
{
    if (!mqtt_isconnected() || !network_isready() || driveio_doorcommandactive() || restartRequest.requested()) return false;
    if (mqtt_counted_send(numPacketsSent, [&]() {
        return mqttClient.publish(topic, payload, retain, qos);
    })) return true;
    Serial.print("MQTT: Publish failed: ");
    Serial.print(topic);
    Serial.print("; library error: ");
    Serial.println(static_cast<int>(mqttClient.getLastError()));
    return false;
}

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
        case DoorState::Unknown: break;
    }
    if (!mqtt_send(MQTT_TOPICCONTROLGETCURRENTDOORSTATE, state, true)) return;
    if (doorState.target != 0) {
        if (!mqtt_send(MQTT_TOPICCONTROLGETNEWDOORSTATE,
            doorState.target == DOORCOMMANDOPEN ? MQTT_COMMANDDOOROPEN : MQTT_COMMANDDOORCLOSE,
            true, 0)) return;
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
    if (payload.length() == 0) return; // Echo of retained-command deletion.

    if (payload != MQTT_SYSTEMRESTART)
    {
        mqtt_format_received(buffer, sizeof(buffer), MQTT_TOPICSYSTEM_RESTART, payload.c_str(), false);
        Serial.println(buffer);
    }
    else
    {
        mqtt_format_received(buffer, sizeof(buffer), MQTT_TOPICSYSTEM_RESTART, payload.c_str(), true);
        Serial.println(buffer);
        restartRequest.request(); // Do not publish recursively inside the callback.
      }
}

/*
 * This function publishes a topic. It passes the parameters without change to the
 * underlying mqtt client but adds a serial print for logging purposes
 */
bool mqtt_publish(String topic, String payload, bool retain)
{
    if (!mqtt_send(topic, payload, retain)) return false;
    Serial.println("RUN: Publish sent (QoS0): set " + topic + " to " + payload);
    return true;
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
    if (!mqttInitialized || !network_isready() || driveio_doorcommandactive() || restartRequest.requested()) return;

    // if connection to the broker is lost, try to reconnect
    if (!mqttClient.isConnected())
    {
        if (!connectAttempted || millis() - lastConnectAttempt_ms >= 10000) mqtt_connect();
    }
    else
    {
        mqttClient.update();
        if (restartRequest.service(millis(), mqttClient.isConnected(),
            [](const char* topic, const char* payload, bool retain, int qos) {
                return mqtt_send(topic, payload, retain, qos);
            })) {
            command = "";
            Serial.println("MQTT: Retained restart command cleared (PUBACK); watchdog restart armed");
            return;
        }
        mqtt_publish_door_state();
        if (mqttFirstRun)
        {
            // global buffer for dealing with json packets
            // The system-info schema is fixed; a stack document cannot fragment the heap.
            StaticJsonDocument<256> jsonDoc;
            char jsonBuffer[256];
            char ipBuffer[16];
            const IPAddress ip = Ethernet.localIP();

            // prepare json payload for info topic
            jsonDoc["application"] = application;
            jsonDoc["version"] = version;
            jsonDoc["author"] = author;
            if (!format_ipv4(ipBuffer, sizeof(ipBuffer), ip[0], ip[1], ip[2], ip[3])) {
                Serial.println("MQTT: Could not format DHCP address for system info");
                return;
            }
            jsonDoc["ip"] = ipBuffer;

            // serialize json document into global buffer and publish
            // attention: size of buffer is limited to 256 bytes
            const size_t bytes = serializeJson(jsonDoc, jsonBuffer, sizeof(jsonBuffer));
            if (jsonDoc.overflowed() || bytes >= sizeof(jsonBuffer)) {
                Serial.println("MQTT: System info JSON did not fit its fixed buffer");
                return;
            }
            if (mqtt_send(MQTT_TOPICSYSTEMINFO, jsonBuffer, true)) mqttFirstRun = false;
        }

        // publish uptime message and online status every 1s
        static uint32_t prev_ms = millis();
        char buffer[12];
        sprintf(buffer, "%lu", uptime_in_secs);
        if (uint32_t(millis() - prev_ms) >= 1000)
        {
            prev_ms = millis();
            mqtt_send(MQTT_TOPICSYSTEMUPTIME, buffer, false);
            
            mqtt_send(MQTT_TOPICSYSTEMSTATUS, mqttFirstWillMsg, true);
        }
    }

    // let other loops run
    yield();
}

/*
* Returns the number of packets received since start
*/
uint32_t mqtt_getpacketsreceived()
{
    return numPacketsReceived;
}

/*
* Returns the number of packets sent since start
*/
uint32_t mqtt_getpacketssent()
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
    return restartRequest.requested();
}
