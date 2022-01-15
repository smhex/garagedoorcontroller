#include <Arduino.h>

#include "driveio.h"

// define the input and output pins to control the drive
// those pin numbers must match the circuit/schematic
#define CMD_OPENDOOR_OUTPUT 0
#define STATUS_DOORISOPEN_INPUT 1
#define CMD_CLOSEDOOR_OUTPUT 2
#define STATUS_DOORISCLOSED_INPUT 3

bool doorStatusIsUnknwon = true;
bool doorStatusIsOpen = false;
bool doorStatusIsClosed = false;
bool doorStatusIsMovingOrStopped = false;
bool doorStatusIsExternal = false;

int commandDuration = 500;
bool commandOpenDoorActive = false;
bool commandCloseDoorActive = false;

int currentDoorStatus = DOORSTATUSEXTERNAL;
int previousDoorStatus = DOORSTATUSEXTERNAL;

unsigned long prev_ms_open = 0;  
unsigned long prev_ms_close = 0;  

// forward declarations
void driveio_readiosignals();

/*
* inits the baseboard and IO for the drive
*/
void driveio_init()
{
    // setup pins and modes
    pinMode(CMD_OPENDOOR_OUTPUT, OUTPUT);
    pinMode(STATUS_DOORISOPEN_INPUT, INPUT_PULLDOWN);
    pinMode(CMD_CLOSEDOOR_OUTPUT, OUTPUT);
    pinMode(STATUS_DOORISCLOSED_INPUT, INPUT_PULLDOWN);
}

/*
* controls the io for the drive (part of the custom schematic)
*/
void driveio_loop()
{
    // read signals
    driveio_readiosignals();

    // send command
    if (commandOpenDoorActive)
    {
        digitalWrite(CMD_OPENDOOR_OUTPUT, HIGH);
        if (millis() > prev_ms_open + commandDuration){
                digitalWrite(CMD_OPENDOOR_OUTPUT, LOW);
                commandOpenDoorActive = false;
        }
    }
    if (commandCloseDoorActive)
    {  
         digitalWrite(CMD_CLOSEDOOR_OUTPUT, HIGH);  
        if (millis() > prev_ms_close + commandDuration){
                digitalWrite(CMD_CLOSEDOOR_OUTPUT, LOW);
                commandCloseDoorActive = false;      
        }
    }

    // let the other loops run
    yield();
}

/*
* returns the current state of the door
*/
bool driveio_doorstatuschanged(int* oldStatus, int* newStatus){
    if (currentDoorStatus!=previousDoorStatus){
        *oldStatus = previousDoorStatus;
        *newStatus = currentDoorStatus;
    }
    return (currentDoorStatus==previousDoorStatus) ? false : true;
}

/*
* reads the IO signals to update internal status
*/
void driveio_readiosignals(){
    
    // preserve previous status
    previousDoorStatus = currentDoorStatus;

    // get current door status
    currentDoorStatus = DOORSTATUSEXTERNAL;
    doorStatusIsOpen = digitalRead(STATUS_DOORISOPEN_INPUT);
    doorStatusIsClosed = digitalRead(STATUS_DOORISCLOSED_INPUT);
    doorStatusIsExternal = (!doorStatusIsOpen && !doorStatusIsClosed);
    doorStatusIsMovingOrStopped = (doorStatusIsOpen && doorStatusIsClosed);

    // put into doorStatus variable
    if (doorStatusIsMovingOrStopped){
        currentDoorStatus = DOORSTATUSMOVINGORSTOPPED;
    }
    else{
        if (doorStatusIsOpen){currentDoorStatus = DOORSTATUSOPEN;}
        if (doorStatusIsClosed){currentDoorStatus = DOORSTATUSCLOSED;}
    }
}

/*
* Sets the IO signals to request the new door status (open or close).
* The output is actually set during the loop() function to make it
* non blocking. 
*/
void driveio_setdoorcommand(int Command)
{
    if ((Command == DOORCOMMANDOPEN) && (!commandOpenDoorActive))
    {
        commandOpenDoorActive = true;
        prev_ms_open = millis();  
    }
    if ((Command == DOORCOMMANDCLOSE) && (!commandCloseDoorActive))
    {
        commandCloseDoorActive = true;
        prev_ms_close = millis(); 
    }
}