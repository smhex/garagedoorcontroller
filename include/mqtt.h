// Include libraries
#include <Ethernet.h>

// list of allowed topic values
#define MQTT_COMMANDDOOROPEN    "open"
#define MQTT_COMMANDDOORCLOSE   "close"
#define MQTT_COMMANDDOORSTOP    "stop"
#define MQTT_STATUSDOOROPEN     "open"
#define MQTT_STATUSDOORCLOSED   "closed"
#define MQTT_STATUSDOOROPENING  "opening"
#define MQTT_STATUSDOORCLOSING  "closing"
#define MQTT_STATUSDOORSTOPPED  "stopped"
#define MQTT_STATUSDOORUNKNOWN  "unknown"
#define MQTT_SYSTEMRESTART      "restart"
#define MQTT_UPDATEINSTALL      "install"

// list of command sources
#define MQTT_COMMANDSOURCELOCAL     "local"
#define MQTT_COMMANDSOURCEREMOTE    "remote"
#define MQTT_COMMANDSOURCEEXTERNAL  "external"

/* exports */
void mqtt_init();
void mqtt_loop();
String mqtt_getcommand();
void mqtt_note_door_command(const String& source);
uint32_t mqtt_getpacketsreceived();
uint32_t mqtt_getpacketssent();
bool mqtt_isconnected();
bool mqtt_isrestartrequested();
