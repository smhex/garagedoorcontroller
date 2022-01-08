#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_MCP23008.h> 

// Display shield with button
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
Adafruit_MCP23008 mcp; 
const unsigned int displayWidth = 128;
const unsigned int displayHeight = 64;

#define ALIGN_CENTER(t)((displayWidth - (u8g2.getUTF8Width(t))) / 2)
#define ALIGN_RIGHT(t) (displayWidth -  u8g2.getUTF8Width(t))
#define ALIGN_LEFT 0

/*
* Initializes the display and its buttons. After bootup the application informaton is
* shown as a splashscreen for 5 secons.
*/
void display_init()
{
  // initialize display and mcp23008 chip
  u8g2.begin();

  // initialize mcp23008 chip at default address 0 - 3 buttons as input
  mcp.begin();      
  mcp.pinMode(0, INPUT);
  mcp.pullUp(0, HIGH);  
  mcp.pinMode(1, INPUT);
  mcp.pullUp(1, HIGH);  
  mcp.pinMode(2, INPUT);
  mcp.pullUp(2, HIGH);

  // configure L3 EDs as output
  mcp.pinMode(4, OUTPUT);
  mcp.pinMode(5, OUTPUT);
  mcp.pinMode(6, OUTPUT); 

  // Beeper
  mcp.pinMode(7, OUTPUT); 
}

/*
* Shows the splashscreen using the application information
*/
void display_splashscreen(String application, String version, String author, String status)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.drawStr(ALIGN_CENTER(application.c_str()),6, application.c_str());
  u8g2.drawStr(ALIGN_CENTER(version.c_str()),18, version.c_str());
  u8g2.drawStr(ALIGN_CENTER(author.c_str()),30, author.c_str());
  u8g2.drawStr(ALIGN_CENTER(status.c_str()),50, status.c_str());
  u8g2.sendBuffer(); 
}