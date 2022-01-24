// Include libraries
#include <Ethernet.h>

// list of allowed topic values
#define MQTT_COMMANDDOOROPEN    "open"
#define MQTT_COMMANDDOORCLOSE   "close"
#define MQTT_STATUSDOOROPEN     "open"
#define MQTT_STATUSDOORCLOSED   "closed"
#define MQTT_STATUSDOOROPENING  "opening"
#define MQTT_STATUSDOORCLOSING  "closing"
#define MQTT_STATUSDOORSTOPPED  "stopped"
#define MQTT_STATUSDOORUNKNOWN  "unknown"
#define MQTT_SYSTEMRESTART      "true"

// list of supported topics
#define MQTT_TOPICSYSTEMUPTIME   "gdc/system/uptime"
#define MQTT_TOPICSYSTEMINFO     "gdc/system/info"
#define MQTT_TOPICSYSTEMSTATUS   "gdc/system/status"
#define MQTT_TOPICSYSTEM_RESTART "gdc/system/restart"
#define MQTT_TOPICCONTROLSETNEWDOORSTATE  "gdc/control/setnewdoorstate"
#define MQTT_TOPICCONTROLGETNEWDOORSTATE  "gdc/control/getnewdoorstate"
#define MQTT_TOPICCONTROLGETCURRENTDOORSTATE "gdc/control/getcurrentdoorstate"
#define MQTT_TOPICCONTROLCOMMANDSOURCE  "gdc/control/commandsource"

// list of command sources
#define MQTT_COMMANDSOURCELOCAL     "local"
#define MQTT_COMMANDSOURCEREMOTE    "remote"
#define MQTT_COMMANDSOURCEEXTERNAL  "external"

/* exports */
void mqtt_init();
void mqtt_loop();
void mqtt_publish(String topic, String payload, bool retain);
String mqtt_getcommand();
int mqtt_getpacketsreceived();
int mqtt_getpacketssent();
bool mqtt_isconnected();
bool mqtt_isrestartrequested();
