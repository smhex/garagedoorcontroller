// Include libraries
#include <Ethernet.h>

/* exports */
void mqtt_init();
void mqtt_loop();
void mqtt_publish(String topic, String payload);
