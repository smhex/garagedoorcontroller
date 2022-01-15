// Include libraries
#include <Arduino.h>

#define HMI_BUTTON_NONE          -1
#define HMI_BUTTON_CLOSEDOOR     0
#define HMI_BUTTON_SYSTEMINFO    1
#define HMI_BUTTON_OPENDOOR      2

#define HMI_LED_DOORCLOSED       4
#define HMI_LED_SYSTEMINFO       5
#define HMI_LED_DOOROPEN         6 

#define HMI_BEEPER               7

/* exports */
void hmi_init();
void hmi_loop();
void hmi_display_splashscreen(String status);
void hmi_display_off(bool enable);
int hmi_getbuttonpressed();
void hmi_setled(int led, int status);
int hmi_getled(int led);
void hmi_setled_blinking(int led, bool enable);
void hmi_showpage(int page);
