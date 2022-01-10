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

int currentDoorStatus = DOORSTATUSUNKNOWN;
int previousDoorStatus = DOORSTATUSUNKNOWN;

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

    // read IO signals to
    driveio_readiosignals();
}

/*
* controls the io for the drive (part of the custom schematic)
*/
void driveio_loop()
{
    // read signals
    driveio_readiosignals();

    //digitalWrite(CMD_OPENDOOR_OUTPUT, doorStatusIsOpen);
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
    currentDoorStatus = DOORSTATUSUNKNOWN;
    doorStatusIsOpen = digitalRead(STATUS_DOORISOPEN_INPUT);
    doorStatusIsClosed = digitalRead(STATUS_DOORISCLOSED_INPUT);
    doorStatusIsExternal = (!doorStatusIsOpen && !doorStatusIsClosed);
    doorStatusIsMovingOrStopped = (doorStatusIsOpen && doorStatusIsClosed);

    // put into doorStatus variable
    if (doorStatusIsMovingOrStopped){currentDoorStatus = DOORSTATUSMOVINGORSTOPPED;}
    if (doorStatusIsExternal){currentDoorStatus = DOORSTATUSEXTERNAL;}
    if (doorStatusIsOpen){currentDoorStatus = DOORSTATUSOPEN;}
    if (doorStatusIsClosed){currentDoorStatus = DOORSTATUSCLOSED;}
}

/*
* sets the IO signals to request the new door status (open or close)
*/
void driveio_setdoorcommand(int Command)
{
    if (Command == DOORCOMMANDOPEN)
    {
        digitalWrite(CMD_OPENDOOR_OUTPUT, HIGH);
        digitalWrite(CMD_CLOSEDOOR_OUTPUT, LOW);
    }
    if (Command == DOORCOMMANDCLOSE)
    {
        digitalWrite(CMD_CLOSEDOOR_OUTPUT, HIGH);
        digitalWrite(CMD_OPENDOOR_OUTPUT, LOW);
    }
}