// Include libraries
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_MCP23008.h>

#include "config.h"
#include "hmi.h"

// internal defines
#define BUTTONSTATUS_PRESSED 0 // inputs use internal pullup's

// button states
int buttonPressed = 0;
int debounce_button_ms = 100;

bool doorOpenLedBlink = false;
bool doorClosedLedBlink = false;


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

    // configure 3 leds as output
    mcp.pinMode(HMI_LED_DOORCLOSED, OUTPUT);
    mcp.pinMode(HMI_LED_SYSTEMINFO, OUTPUT);
    mcp.pinMode(HMI_LED_DOOROPEN, OUTPUT);

    // Beeper
    mcp.pinMode(HMI_BEEPER, OUTPUT);

    // set all leds to OFF
    mcp.digitalWrite(HMI_LED_DOOROPEN, LOW);
    mcp.digitalWrite(HMI_LED_SYSTEMINFO, LOW);
    mcp.digitalWrite(HMI_LED_DOORCLOSED, LOW);
}

/*
* Activates the powersave mode for the display. The content is preserved but
* not printed on the display. 
*/
void hmi_display_off(bool enable)
{
    u8g2.setPowerSave(!enable);
}

/*
* Checks for button press
*/
void hmi_loop()
{
    // check button pressed states
    buttonPressed = HMI_BUTTON_NONE;
    static uint32_t prev_ms_debounce = millis();
    if (millis() > prev_ms_debounce + debounce_button_ms)
    {
        prev_ms_debounce = millis();
        if (mcp.digitalRead(HMI_BUTTON_OPENDOOR) == BUTTONSTATUS_PRESSED)
        {
            buttonPressed = HMI_BUTTON_OPENDOOR;
        }

        if (mcp.digitalRead(HMI_BUTTON_CLOSEDOOR) == BUTTONSTATUS_PRESSED)
        {
            buttonPressed = HMI_BUTTON_CLOSEDOOR;
        }

        if (mcp.digitalRead(HMI_BUTTON_SYSTEMINFO) == BUTTONSTATUS_PRESSED)
        {
            buttonPressed = HMI_BUTTON_SYSTEMINFO;
            hmi_setled(HMI_LED_SYSTEMINFO, HIGH);
        }
        else
        {
            hmi_setled(HMI_LED_SYSTEMINFO, LOW);
        }
    }

    // activate blinking
    if (doorOpenLedBlink)
    {
        static uint32_t prev_ms_on = millis();
        if (millis() > prev_ms_on + ledBlinkDuration_ms)
        {
            prev_ms_on = millis();
            int ledState = hmi_getled(HMI_LED_DOOROPEN);
            ledState = (ledState == LOW) ? HIGH : LOW;
            hmi_setled(HMI_LED_DOOROPEN, ledState);
        }
    }
    if (doorClosedLedBlink)
    {
        static uint32_t prev_ms_on = millis();
        if (millis() > prev_ms_on + ledBlinkDuration_ms)
        {
            prev_ms_on = millis();
            int ledState = hmi_getled(HMI_LED_DOORCLOSED);
            ledState = (ledState == LOW) ? HIGH : LOW;
            hmi_setled(HMI_LED_DOORCLOSED, ledState);
        }
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

/*
* sets the status of a led to either ON or OFF
*/
void hmi_setled(int led, int status)
{
    mcp.digitalWrite(led, status);
}

/*
* gets the status of a led
*/
int hmi_getled(int led)
{
    return mcp.digitalRead(led);
}

/*
* Sets the status of a led to blink. Blinking needs to be started and stopped
* by the enable parameter. In order not to block the main loop this function 
* only sets a flag for the given led. The blinking itself is done via write
* command in hmi_loop(). Setting parameter enable to false has the same effect
* like calling hmi_setled(led, OFF). This is used to maintain a definitive ON/OFF
* state for the led
*/
void hmi_setled_blinking(int led, bool enable)
{
    hmi_setled(led, enable);
    if (led == HMI_LED_DOOROPEN)
    {
        doorOpenLedBlink = enable;
    }
    if (led == HMI_LED_DOORCLOSED)
    {
        doorClosedLedBlink = enable;
    }
}

/*
* draws a full frame with title and text information. The title has a horizontal line
* as a separator between the text
*/
void hmi_display_frame(String title, String text[], int numlines)
{
    int startPos = (numlines==4) ? 18 : 27;
    if (numlines==2) {startPos=36;}
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontRefHeightExtendedText();
    u8g2.setDrawColor(1);
    u8g2.setFontPosTop();
    u8g2.setFontDirection(0);
    u8g2.drawStr(ALIGN_CENTER(title.c_str()), 2, title.c_str());
    u8g2.drawHLine(0, 13, displayWidth);
    for (int i = 0; i < numlines; i++)
    {
        u8g2.drawStr(ALIGN_CENTER(text[i].c_str()), startPos + (i * 12), text[i].c_str());
    }
    u8g2.sendBuffer();
}