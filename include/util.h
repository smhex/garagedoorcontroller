#include <Arduino.h>
#include <Ethernet.h>

#define every_second     1000
#define every_10_seconds 10000

/*
* converts IP address to string
*/
String IPAddressToString(IPAddress address)
{
        return String(address[0]) + "." +
               String(address[1]) + "." +
               String(address[2]) + "." +
               String(address[3]);
}

/*
* converts float to string
*/
String toString(float fvalue)
{
        return String(fvalue, 0);
}

/*
* converts float to string with decimal
*/
String toString(float fvalue, unsigned int num)
{
        return String(fvalue, num);
}

/*
* returns true if 1 second is over
*/
bool timespan_one_second(){
        static uint32_t prev_ms = millis();   
        if (millis() > prev_ms + 1000){
                prev_ms = millis();
                return true;
        }
        return false;
}

/*
* returns true if 10 seconds are over
*/
bool timespan_ten_seconds(){
        static uint32_t prev_ms = millis();   
        if (millis() > prev_ms + 10000){
                prev_ms = millis();
                return true;
        }
        return false;
}