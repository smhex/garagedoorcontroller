#define DOORSTATUSUNKNOWN           0
#define DOORSTATUSOPEN              1
#define DOORSTATUSCLOSED            2
#define DOORSTATUSMOVINGORSTOPPED   3
#define DOORSTATUSEXTERNAL          4

#define DOORCOMMANDOPEN             1
#define DOORCOMMANDCLOSE            2

/* exports */
void driveio_init();
void driveio_loop();
bool driveio_doorstatuschanged(int* oldStatus, int* newStatus);
void driveio_setdoorcommand(int Command);