#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "Arduino.h"
#include "config.h"
#include "driveio.h"
#include "mqtt_log.h"

static unsigned long now_ms = 1000;
static int pins[4] = {};
int commandDuration_ms = 500;
unsigned long millis() { return now_ms; }
void pinMode(int, int) {}
void yield() {}
int digitalRead(int pin) { return pins[pin]; }
void digitalWrite(int pin, int value) {
    pins[pin] = value;
    // Check every output transition, including transient overlap.
    assert(!(pins[CMD_OPENDOOR_OUTPUT] && pins[CMD_CLOSEDOOR_OUTPUT]));
}

static void test_pulse(int first, int other, int firstPin, int otherPin) {
    const unsigned long start = now_ms;
    driveio_setdoorcommand(first);
    // A competing command even before the first loop must be rejected.
    driveio_setdoorcommand(other);
    driveio_loop();
    assert(pins[firstPin] == HIGH && pins[otherPin] == LOW);
    now_ms = start + 250;
    driveio_setdoorcommand(first); // Duplicate must not extend the pulse.
    driveio_setdoorcommand(other);
    driveio_loop();
    assert(pins[firstPin] == HIGH && pins[otherPin] == LOW);
    now_ms = start + 501;
    driveio_loop();
    assert(!driveio_doorcommandactive());
    assert(pins[firstPin] == LOW && pins[otherPin] == LOW);
    // Rejected commands must not be queued for later execution.
    now_ms += 1000;
    driveio_loop();
    assert(!driveio_doorcommandactive());
    assert(pins[firstPin] == LOW && pins[otherPin] == LOW);
}

static void test_logging() {
    struct GuardedBuffer {
        unsigned char before[16];
        char text[80];
        unsigned char after[16];
    } buffer;
    const char* topics[] = {"gdc/control/setnewdoorstate", "gdc/system/restart"};
    const size_t lengths[] = {0, 4, 30, 79, 80, 200, 256, 4096};
    for (const char* topic : topics) {
        for (size_t length : lengths) {
            memset(&buffer, 0x5a, sizeof(buffer));
            const std::string payload(length, 'x');
            mqtt_format_received(buffer.text, sizeof(buffer.text), topic, payload.c_str(), false);
            assert(memchr(buffer.text, '\0', sizeof(buffer.text)) != nullptr);
            assert(strstr(buffer.text, "(invalid)") != nullptr);
            for (unsigned char value : buffer.before) assert(value == 0x5a);
            for (unsigned char value : buffer.after) assert(value == 0x5a);
        }
    }
    mqtt_format_received(buffer.text, sizeof(buffer.text), topics[0], "open", true);
    assert(strcmp(buffer.text, "RUN: Subscribe: set gdc/control/setnewdoorstate to open") == 0);
    mqtt_format_received(buffer.text, sizeof(buffer.text), topics[1], "true", true);
    assert(strcmp(buffer.text, "RUN: Subscribe: set gdc/system/restart to true") == 0);
    char tiny = 'x';
    mqtt_format_received(&tiny, 1, topics[0], "open", true);
    assert(tiny == '\0');
}

int main() {
    driveio_init();
    driveio_setdoorcommand(99);
    assert(!driveio_doorcommandactive());
    test_pulse(DOORCOMMANDOPEN, DOORCOMMANDCLOSE, CMD_OPENDOOR_OUTPUT, CMD_CLOSEDOOR_OUTPUT);
    test_pulse(DOORCOMMANDCLOSE, DOORCOMMANDOPEN, CMD_CLOSEDOOR_OUTPUT, CMD_OPENDOOR_OUTPUT);
    test_logging();
    puts("PASS: command interlock, duplicate pulses, invalid commands, bounded MQTT logging");
}
