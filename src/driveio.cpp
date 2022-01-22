#include <Arduino.h>
#include "driveio.h"

// internal variables holding the different door states
bool doorStatusIsUnknwon = true;
bool doorStatusIsOpen = false;
bool doorStatusIsClosed = false;
bool doorStatusIsMovingOrStopped = false;
bool doorStatusIsExternal = false;

// length in ms for the command pulse
int commandDuration = 500;
bool commandOpenDoorActive = false;
bool commandCloseDoorActive = false;

// helper variables to maintain the door status
int currentDoorStatus = DOORSTATUSEXTERNAL;
int previousDoorStatus = DOORSTATUSEXTERNAL;

// helper variables to create the pulse asynchronously
unsigned long prev_ms_open = 0;  
unsigned long prev_ms_close = 0;  

// forward declarations
void driveio_readiosignals();

/*
* Inits the IO interface pins to the drive (2x Input, 2x Output)
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
* The door status is read and if a command is requested the
* necessary pulse at the corresponding pin is created.
*/
void driveio_loop()
{
    // read signals
    driveio_readiosignals();

    /* To open or close the door a 500ms pulse is required at the 
     * output pin(s). The pulse width can be configured via the
     * variable "commandDuration". The rising edge is generated a 
     * soon as a driveio_setdoorcommand() is called. 
     */
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
* Returns the current state of the door and also a flag, indicating
* whether is has changed or not. The params "oldStatus" and "newStatus"
* are provided by the caller. 
*/
bool driveio_doorstatuschanged(int* oldStatus, int* newStatus){
    if (currentDoorStatus!=previousDoorStatus){
        *oldStatus = previousDoorStatus;
        *newStatus = currentDoorStatus;
    }
    return (currentDoorStatus==previousDoorStatus) ? false : true;
}

/*
* Reads the IO signals to update internal status variables
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

/*
 * Returns true if a command (open/close) is active at the moment. It is
 * only true during the command pulse
 * */
bool driveio_doorcommandactive()
{
    return (commandOpenDoorActive || commandCloseDoorActive);
}

/*
* Returns the state of the IO. Parameter "io" corresponds with one of
* input or output pins
*/
int driveio_getiostatus(int io)
{
    return digitalRead(io);
}