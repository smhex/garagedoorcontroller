// Include libraries
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_MCP23008.h>

#include "hmi.h"

// internal defines
#define BUTTONSTATUS_PRESSED  0     // inputs use internal pullup's

// button states
int buttonPressed = 0;


// Global variables
extern unsigned long uptime_in_sec;
extern String application;
extern String version;
extern String author;

// Display shield with button
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
Adafruit_MCP23008 mcp;
const unsigned int displayWidth = 128;
const unsigned int displayHeight = 64;

#define ALIGN_CENTER(t) ((displayWidth - (u8g2.getUTF8Width(t))) / 2)
#define ALIGN_RIGHT(t) (displayWidth - u8g2.getUTF8Width(t))
#define ALIGN_LEFT 0

/*
* Initializes the display and its buttons. After bootup the application informaton is
* shown as a splashscreen for 5 secons.
*/
void hmi_init()
{
    // initialize display and mcp23008 chip
    u8g2.begin();

    // initialize mcp23008 chip at default address 0 - 3 buttons as input
    mcp.begin();
    mcp.pinMode(HMI_BUTTON_OPENDOOR, INPUT);
    mcp.pullUp(HMI_BUTTON_OPENDOOR, HIGH);
    mcp.pinMode(HMI_BUTTON_SYSTEMINFO, INPUT);
    mcp.pullUp(HMI_BUTTON_SYSTEMINFO, HIGH);
    mcp.pinMode(HMI_BUTTON_CLOSEDOOR, INPUT);
    mcp.pullUp(HMI_BUTTON_CLOSEDOOR, HIGH);

    // configure L3 EDs as output
    mcp.pinMode(HMI_LED_DOORCLOSED, OUTPUT);
    mcp.pinMode(HMI_LED_DOORMOVING, OUTPUT);
    mcp.pinMode(HMI_LED_DOOROPEN, OUTPUT);

    // Beeper
    mcp.pinMode(HMI_BEEPER, OUTPUT);
}

/*
* Shows the splashscreen using the application information
*/
void hmi_display_splashscreen(String status)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontRefHeightExtendedText();
    u8g2.setDrawColor(1);
    u8g2.setFontPosTop();
    u8g2.setFontDirection(0);
    u8g2.drawStr(ALIGN_CENTER(application.c_str()), 6, application.c_str());
    u8g2.drawStr(ALIGN_CENTER(version.c_str()), 18, version.c_str());
    u8g2.drawStr(ALIGN_CENTER(author.c_str()), 30, author.c_str());
    u8g2.drawStr(ALIGN_CENTER(status.c_str()), 50, status.c_str());
    u8g2.sendBuffer();
}

/*
* Clears display buffer -> switch off
*/
void hmi_display_off()
{
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

/*
* Clears display buffer -> switch off
*/
void hmi_display_text(String text)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontRefHeightExtendedText();
    u8g2.setDrawColor(1);
    u8g2.setFontPosTop();
    u8g2.setFontDirection(0);
    //u8g2.drawStr(ALIGN_CENTER(application.c_str()),6, application.c_str());
    //u8g2.drawStr(ALIGN_CENTER(version.c_str()),18, version.c_str());
    //u8g2.drawStr(ALIGN_CENTER(author.c_str()),30, author.c_str());
    u8g2.drawStr(ALIGN_CENTER(text.c_str()), 50, text.c_str());
    u8g2.sendBuffer();
}

/*
* Checks for button press
*/
void hmi_loop()
{
    // check button pressed states
    buttonPressed = HMI_BUTTON_NONE;
    if (mcp.digitalRead(HMI_BUTTON_OPENDOOR) == BUTTONSTATUS_PRESSED)
    {
        mcp.digitalWrite(HMI_LED_DOOROPEN, HIGH);
        buttonPressed = HMI_BUTTON_OPENDOOR;
    }
    else
    {
        mcp.digitalWrite(HMI_LED_DOOROPEN, LOW);
    }

    if (mcp.digitalRead(HMI_BUTTON_CLOSEDOOR) == BUTTONSTATUS_PRESSED)
    {
        mcp.digitalWrite(HMI_LED_DOORCLOSED, HIGH);
        buttonPressed = HMI_BUTTON_CLOSEDOOR;
    }
    else
    {
        mcp.digitalWrite(HMI_LED_DOORCLOSED, LOW);
    }
    if (mcp.digitalRead(HMI_BUTTON_SYSTEMINFO) == BUTTONSTATUS_PRESSED)
    {
        buttonPressed = HMI_BUTTON_SYSTEMINFO;
    }

    // let other loops run
    yield();
}

/*
* returns button pressed status for button<id>
*/
int hmi_getbuttonpressed()
{
    return buttonPressed;
}