#include "debug_console.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Dns.h>
#include <Ethernet.h>
#include <MQTTPubSubClient.h>
#include "config.h"
#include "door_state.h"
#include "driveio.h"
#include "lan_update.h"
#include "mqtt.h"
#include "mqtt_delivery.h"
#include "network.h"
#include "sensors.h"

MQTTPubSub::PubSubClient<1536> mqttClient;
namespace {
String command, source = "unknown";
uint32_t received = 0, sent = 0, lastState = 0;
bool initialized = false, attempted = false, discovery = true, dirty = true, bootPending = true;
uint32_t lastAttempt = 0;
MqttRestart restart;
DoorState previousState = DoorState::Unknown;
int previousTarget = 0;

String id() { static String v; if (!v.length()) { char b[18]; snprintf(b,sizeof(b),"gdc-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]); v=b; } return v; }
String root() { return String("gdc/")+id(); }
String top(const char* suffix) { return root()+"/"+suffix; }
String macString() { char b[18]; snprintf(b,sizeof(b),"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]); return String(b); }
const char* stateName(DoorState s) { switch(s) { case DoorState::Open:return "open"; case DoorState::Closed:return "closed"; case DoorState::Opening:return "opening"; case DoorState::Closing:return "closing"; case DoorState::Stopped:return "stopped"; default:return "unknown"; } }
const char* doorStatusName(DoorState s) { switch(s) { case DoorState::Open:return "open"; case DoorState::Closed:return "closed"; case DoorState::Opening: case DoorState::Closing:return "moving"; case DoorState::Stopped:return "stopped"; default:return "unknown"; } }
bool send(const String& t,const String& p,bool retain,int qos=0) { if(!mqtt_isconnected()||!network_isready()||driveio_doorcommandactive()||restart.requested()) return false; if(mqttClient.publish(t,p,retain,qos)){++sent;return true;} Debug.println("MQTT: publish failed");return false; }
String dev() { const String hardwareId=id(); return String("\"device\":{\"identifiers\":[\"")+hardwareId+"\"],\"name\":\"Garage Door Controller "+macString().substring(9)+"\",\"manufacturer\":\"smhex\",\"model\":\"Garage Door Controller\",\"model_id\":\"GDC-MKRZERO\",\"serial_number\":\""+hardwareId+"\",\"hw_version\":\"Arduino MKR Zero\",\"sw_version\":\""+version+"\"}"; }
bool discover(const char* component,const char* object,const String& config) { String uid=id()+"_"+object; String p=String("{\"~\":\"")+root()+"\",\"unique_id\":\""+uid+"\","+config+",\"availability_topic\":\"~/availability\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","+dev()+",\"origin\":{\"name\":\"Garage Door Controller\",\"sw_version\":\""+version+"\"}}"; return send(String("homeassistant/")+component+"/"+uid+"/config",p,true); }
bool publishDiscovery() { return
  discover("cover","door","\"name\":null,\"device_class\":\"garage\",\"command_topic\":\"~/command/door\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.door.state }}\",\"payload_open\":\"open\",\"payload_close\":\"close\",\"payload_stop\":\"stop\",\"state_open\":\"open\",\"state_closed\":\"closed\",\"state_opening\":\"opening\",\"state_closing\":\"closing\",\"state_stopped\":\"stopped\"") &&
  discover("sensor","door_status","\"name\":\"Door status\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.door.status }}\",\"device_class\":\"enum\",\"options\":[\"open\",\"closed\",\"moving\",\"stopped\",\"unknown\"]") &&
  discover("update","firmware","\"name\":\"Firmware\",\"device_class\":\"firmware\",\"command_topic\":\"~/command/update\",\"payload_install\":\"install\",\"state_topic\":\"~/update/state\",\"entity_category\":\"config\"") &&
  discover("button","restart","\"name\":\"Restart\",\"device_class\":\"restart\",\"command_topic\":\"~/command/restart\",\"payload_press\":\"restart\",\"entity_category\":\"config\"") &&
  discover("sensor","temperature","\"name\":\"Temperature\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.sensors.temperature_c }}\",\"device_class\":\"temperature\",\"unit_of_measurement\":\"°C\",\"state_class\":\"measurement\",\"suggested_display_precision\":1,\"entity_category\":\"diagnostic\"") &&
  discover("sensor","humidity","\"name\":\"Humidity\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.sensors.humidity_pct }}\",\"device_class\":\"humidity\",\"unit_of_measurement\":\"%\",\"state_class\":\"measurement\",\"suggested_display_precision\":0,\"entity_category\":\"diagnostic\"") &&
  discover("sensor","pressure","\"name\":\"Pressure\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.sensors.pressure_hpa }}\",\"device_class\":\"atmospheric_pressure\",\"unit_of_measurement\":\"hPa\",\"state_class\":\"measurement\",\"suggested_display_precision\":1,\"entity_category\":\"diagnostic\"") &&
  discover("sensor","illuminance","\"name\":\"Illuminance\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.sensors.illuminance_lx }}\",\"device_class\":\"illuminance\",\"unit_of_measurement\":\"lx\",\"state_class\":\"measurement\",\"suggested_display_precision\":1,\"entity_category\":\"diagnostic\"") &&
  discover("binary_sensor","env_shield","\"name\":\"ENV shield\",\"state_topic\":\"~/state\",\"value_template\":\"{{ 'ON' if value_json.sensors.available else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"connectivity\",\"entity_category\":\"diagnostic\"") &&
  discover("sensor","ip_address","\"name\":\"IP address\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.system.ip }}\",\"icon\":\"mdi:ip-network\",\"entity_category\":\"diagnostic\"") &&
  discover("sensor","last_boot","\"name\":\"Last boot\",\"state_topic\":\"~/system/boot\",\"value_template\":\"{{ (now() - timedelta(seconds=value_json.uptime_s)).isoformat() }}\",\"device_class\":\"timestamp\",\"entity_category\":\"diagnostic\"") &&
  discover("sensor","last_command_source","\"name\":\"Last command source\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.door.last_command_source }}\",\"device_class\":\"enum\",\"options\":[\"local\",\"" MQTT_COMMANDSOURCEREMOTE "\",\"external\",\"unknown\"],\"entity_category\":\"diagnostic\"") &&
  discover("sensor","open_travel_time","\"name\":\"Last opening travel time\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.door.last_open_travel_s }}\",\"device_class\":\"duration\",\"unit_of_measurement\":\"s\",\"suggested_display_precision\":0,\"entity_category\":\"diagnostic\"") &&
  discover("sensor","close_travel_time","\"name\":\"Last closing travel time\",\"state_topic\":\"~/state\",\"value_template\":\"{{ value_json.door.last_close_travel_s }}\",\"device_class\":\"duration\",\"unit_of_measurement\":\"s\",\"suggested_display_precision\":0,\"entity_category\":\"diagnostic\""); }
void publishBootState() {
  StaticJsonDocument<64> json;
  char payload[64];
  json["uptime_s"] = uptime_in_secs;
  const size_t length = serializeJson(json, payload, sizeof(payload));
  if (!json.overflowed() && length < sizeof(payload) && send(top("system/boot"), payload, true))
    bootPending = false;
}

void publishState() {
  StaticJsonDocument<1024> json;
  char payload[1200], ip[16];
  const bool sensorsAvailable = sensors_isvalid();

  JsonObject door = json.createNestedObject("door");
  door["state"] = stateName(doorState.state);
  door["status"] = doorStatusName(doorState.state);
  door["target"] = doorState.target == DOORCOMMANDOPEN ? "open" :
                   doorState.target == DOORCOMMANDCLOSE ? "close" : nullptr;
  door["last_command_source"] = source;
  door["open_endstop"] = driveio_getiostatus(STATUS_DOORISOPEN_INPUT) != 0;
  door["closed_endstop"] = driveio_getiostatus(STATUS_DOORISCLOSED_INPUT) != 0;
  door["open_output"] = driveio_getiostatus(CMD_OPENDOOR_OUTPUT) != 0;
  door["close_output"] = driveio_getiostatus(CMD_CLOSEDOOR_OUTPUT) != 0;
  if (doorState.lastOpenTravelMs())
    door["last_open_travel_s"] = (doorState.lastOpenTravelMs() + 500) / 1000;
  else
    door["last_open_travel_s"] = nullptr;
  if (doorState.lastCloseTravelMs())
    door["last_close_travel_s"] = (doorState.lastCloseTravelMs() + 500) / 1000;
  else
    door["last_close_travel_s"] = nullptr;

  JsonObject sensors = json.createNestedObject("sensors");
  sensors["available"] = sensorsAvailable;
  sensors["temperature_c"] = sensorsAvailable ? roundf(sensors_get_temperature() * 10.0f) / 10.0f : NAN;
  sensors["humidity_pct"] = sensorsAvailable ? roundf(sensors_get_humidity()) : NAN;
  sensors["pressure_hpa"] = sensorsAvailable ? roundf(sensors_get_pressure() * 100.0f) / 10.0f : NAN;
  sensors["illuminance_lx"] = sensorsAvailable ? roundf(sensors_get_illuminance() * 10.0f) / 10.0f : NAN;

  JsonObject system = json.createNestedObject("system");
  system["firmware"] = version;
  system["uptime_s"] = uptime_in_secs;
  system["network_link"] = Ethernet.linkStatus() != LinkOFF;
  system["mqtt_messages_sent"] = sent;
  system["mqtt_messages_received"] = received;
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", Ethernet.localIP()[0], Ethernet.localIP()[1],
           Ethernet.localIP()[2], Ethernet.localIP()[3]);
  system["ip"] = ip;

  JsonObject update = json.createNestedObject("update");
  update["installed_version"] = version;
  update["latest_version"] = (lan_update_available() || lan_update_installing()) ? lan_update_version() : version;
  update["in_progress"] = lan_update_installing();
  if (lan_update_installing())
    update["update_percentage"] = 100;
  else
    update["update_percentage"] = nullptr;
  update["can_install"] = lan_update_can_install();
  const char* blockedReason = lan_update_install_block_reason();
  update["blocked_reason"] = blockedReason ? blockedReason : nullptr;
  json["schema"] = 1;

  const size_t length = serializeJson(json, payload, sizeof(payload));
  if (!json.overflowed() && length < sizeof(payload) && send(top("state"), payload, true)) {
    dirty = false;
    lastState = millis();
  }
}
void publishUpdateState() { StaticJsonDocument<256> j; char p[256]; j["installed_version"]=version; j["latest_version"]=(lan_update_available()||lan_update_installing())?lan_update_version():version; j["in_progress"]=lan_update_installing(); if(lan_update_installing())j["update_percentage"]=100;else j["update_percentage"]=nullptr; size_t n=serializeJson(j,p,sizeof(p)); if(!j.overflowed()&&n<sizeof(p)) send(top("update/state"),p,true); }
void doorCallback(const String& p,const size_t){++received;if(p=="open"||p=="close"||p=="stop"){command=p;source=MQTT_COMMANDSOURCEREMOTE;dirty=true;}}
void updateCallback(const String& p,const size_t){++received;if(p=="install"){lan_update_request_install("MQTT");dirty=true;}}
void restartCallback(const String& p,const size_t){++received;if(p=="restart")restart.request();}
}
void mqtt_init(){if(initialized)return;mqttClient.setKeepAliveTimeout(60);mqttClient.setCleanSession(true);static String availability=top("availability");mqttClient.setWill(availability,mqttLastWillMsg,true,0);mqttClient.setTimeout(1000);ethClient.setConnectionTimeout(1000);mqttClient.begin(ethClient);initialized=true;}
void mqtt_connect(){attempted=true;ethClient.stop();DNSClient dns;IPAddress broker;dns.begin(Ethernet.dnsServerIP());if(dns.getHostByName(mqttBrokerAddress,broker,500)!=1||!ethClient.connect(broker,mqttBrokerPort)||!mqttClient.connect(mqttClientID,mqttUsername,mqttPassword)){ethClient.stop();lastAttempt=millis();return;}static String door=top("command/door"),update=top("command/update"),reboot=top("command/restart");if(!mqttClient.subscribe(door,&doorCallback)||!mqttClient.subscribe(update,&updateCallback)||!mqttClient.subscribe(reboot,&restartCallback)){ethClient.stop();lastAttempt=millis();return;}discovery=dirty=true;bootPending=true;previousState=DoorState::Unknown;}
void mqtt_loop(){if(!initialized||!network_isready()||driveio_doorcommandactive()||restart.requested())return;if(!mqttClient.isConnected()){if(!attempted||millis()-lastAttempt>=10000)mqtt_connect();return;}mqttClient.update();static String reboot=top("command/restart");if(restart.service(millis(),true,reboot.c_str(),[](const char*t,const char*p,bool r,int q){return send(t,p,r,q);}))return;if(doorState.state!=previousState||doorState.target!=previousTarget){previousState=doorState.state;previousTarget=doorState.target;dirty=true;}if(lan_update_state_changed())dirty=true;if(discovery&&publishDiscovery())discovery=false;if(bootPending)publishBootState();if(dirty||millis()-lastState>=10000){publishState();publishUpdateState();}static uint32_t lastAvailability=0;if(millis()-lastAvailability>=1000){send(top("availability"),mqttFirstWillMsg,true);lastAvailability=millis();}}
String mqtt_getcommand(){String r=command;command="";return r;}void mqtt_note_door_command(const String& s){source=s;dirty=true;}uint32_t mqtt_getpacketsreceived(){return received;}uint32_t mqtt_getpacketssent(){return sent;}bool mqtt_isconnected(){return initialized&&mqttClient.isConnected();}bool mqtt_isrestartrequested(){return restart.requested();}
