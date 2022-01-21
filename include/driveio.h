// defines the different door status values
#define DOORSTATUSEXTERNAL          0
#define DOORSTATUSOPEN              1
#define DOORSTATUSCLOSED            2
#define DOORSTATUSMOVINGORSTOPPED   3

// define the door commands (open and close)
#define DOORCOMMANDOPEN             1
#define DOORCOMMANDCLOSE            2

// define the input and output pins to control the drive
// those pin numbers must match the circuit/schematic
#define CMD_OPENDOOR_OUTPUT       0
#define STATUS_DOORISOPEN_INPUT   1
#define CMD_CLOSEDOOR_OUTPUT      2
#define STATUS_DOORISCLOSED_INPUT 3

/* exports */
void driveio_init();
void driveio_loop();
bool driveio_doorstatuschanged(int* oldStatus, int* newStatus);
void driveio_setdoorcommand(int Command);
int driveio_getiostatus(int io);