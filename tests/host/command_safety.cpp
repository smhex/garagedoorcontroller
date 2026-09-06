#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <limits.h>
#include "Arduino.h"
#include "config.h"
#include "driveio.h"
#include "mqtt_log.h"
#include "door_state.h"
#include "button_edge.h"
#include "input_capture.h"
#include "mqtt_delivery.h"
#include "time_math.h"

static void test_time_math() {
    assert(!time_elapsed(100, 100, 1));
    assert(!time_elapsed(199, 100, 100));
    assert(time_elapsed(200, 100, 100));
    assert(time_elapsed(201, 100, 100));
    assert(!time_elapsed(UINT32_MAX - 1, UINT32_MAX - 50, 50));
    assert(time_elapsed(UINT32_MAX, UINT32_MAX - 50, 50));
    assert(!time_elapsed(0, UINT32_MAX - 50, 52));
    assert(time_elapsed(0, UINT32_MAX - 50, 51));
    assert(!time_elapsed(49, UINT32_MAX - 50, 101));
    assert(time_elapsed(50, UINT32_MAX - 50, 101));
    assert(time_elapsed(UINT32_MAX, 0, UINT32_MAX));
}

static void test_mqtt_delivery() {
    MqttRestart restart;
    int calls = 0;
    bool accepted = false;
    auto send = [&](const char* topic, const char* payload, bool retained, int qos) {
        ++calls;
        assert(strcmp(topic, "gdc/system/restart") == 0);
        assert(payload[0] == '\0' && retained && qos == 1);
        return accepted;
    };
    assert(!restart.service(0, true, send) && calls == 0);
    restart.request();
    assert(!restart.service(0, false, send) && calls == 0);
    const uint32_t start = UINT32_MAX - 1000;
    assert(!restart.service(start, true, send));
    assert(calls == 1 && !restart.requested());
    restart.request(); // Duplicate delivery must not reset retry timing.
    assert(!restart.service(uint32_t(start + 9999), true, send) && calls == 1);
    accepted = true;
    assert(!restart.service(uint32_t(start + 10000), false, send) && calls == 1);
    assert(restart.service(uint32_t(start + 10000), true, send));
    assert(restart.requested() && calls == 2);
    restart.request();
    assert(!restart.service(50000, true, send) && calls == 2);
    uint32_t count = 0;
    assert(!mqtt_counted_send(count, []() { return false; }) && count == 0);
    assert(mqtt_counted_send(count, []() { return true; }) && count == 1);
    count = UINT32_MAX;
    assert(mqtt_counted_send(count, []() { return true; }) && count == 0);
}

static void test_input_capture() {
    InputCapture<3> capture;
    capture.reset(UINT32_MAX - 10);
    capture.record(UINT32_MAX - 10, 1);
    capture.record(UINT32_MAX - 5, 1);
    capture.record(4, 0);
    capture.record(9, 3);
    capture.record(14, 2);
    capture.record(19, 2);
    assert(capture.count == 3 && capture.dropped == 1 && capture.reads == 6);
    assert(capture.samples[0].us == 0 && capture.samples[0].levels == 1);
    assert(capture.samples[1].us == 15 && capture.samples[1].levels == 0);
    assert(capture.samples[2].levels == 3 && capture.maxGapUs == 10);
    capture.reset(100);
    capture.record(100, 2);
    assert(capture.count == 1 && capture.dropped == 0 && capture.reads == 1);
}

static void test_repeated_commands_preserve_motion() {
    for (int startDirection : {DOORCOMMANDOPEN, DOORCOMMANDCLOSE}) {
        for (int stopDirection : {DOORCOMMANDOPEN, DOORCOMMANDCLOSE}) {
            for (int resumeDirection : {DOORCOMMANDOPEN, DOORCOMMANDCLOSE}) {
                DoorStateTracker tracker;
                int startInput = startDirection == DOORCOMMANDOPEN ? DOORSTATUSCLOSED : DOORSTATUSOPEN;
                tracker.observe(startInput);
                assert(tracker.command(startDirection));
                const DoorState moving = startDirection == DOORCOMMANDOPEN ? DoorState::Opening : DoorState::Closing;
                assert(tracker.state == moving);
                // Command-coupled samples must not masquerade as end positions.
                tracker.observe(startDirection == DOORCOMMANDOPEN ? DOORSTATUSOPEN : DOORSTATUSCLOSED, true);
                tracker.observe(DOORSTATUSEXTERNAL, true);
                assert(tracker.state == moving);
                // The starting end switch remains active briefly after the command.
                tracker.observe(startInput);
                assert(tracker.state == moving);
                tracker.observe(DOORSTATUSMOVINGORSTOPPED);
                assert(tracker.state == moving);
                assert(tracker.command(stopDirection));
                assert(tracker.state == moving);
                assert(tracker.target == startDirection);
                tracker.observe(stopDirection == DOORCOMMANDOPEN ? DOORSTATUSOPEN : DOORSTATUSCLOSED, true);
                assert(tracker.state == moving);
                tracker.observe(DOORSTATUSMOVINGORSTOPPED);
                assert(tracker.state == moving);
                assert(tracker.command(resumeDirection));
                assert(tracker.state == moving);
                assert(tracker.target == startDirection);
                tracker.observe(resumeDirection == DOORCOMMANDOPEN ? DOORSTATUSOPEN : DOORSTATUSCLOSED);
                assert(tracker.state == (resumeDirection == DOORCOMMANDOPEN ? DoorState::Open : DoorState::Closed));
                assert(!tracker.command(resumeDirection));
            }
        }
    }
    DoorStateTracker tracker;
    tracker.observe(DOORSTATUSMOVINGORSTOPPED);
    assert(tracker.state == DoorState::Unknown);
    assert(!tracker.command(99));
    tracker.observe(DOORSTATUSCLOSED);
    tracker.observe(DOORSTATUSMOVINGORSTOPPED);
    assert(tracker.state == DoorState::Unknown); // External motion is not guessed.
    tracker.observe(DOORSTATUSOPEN);
    assert(tracker.state == DoorState::Open);

    // An end switch reached during an active pulse is retained in the model.
    tracker.command(DOORCOMMANDCLOSE);
    tracker.observe(DOORSTATUSMOVINGORSTOPPED);
    tracker.observe(DOORSTATUSCLOSED, true);
    assert(tracker.state == DoorState::Closing);
    tracker.observe(DOORSTATUSCLOSED);
    assert(tracker.state == DoorState::Closed);

    ButtonEdge button;
    assert(!button.update(false));
    assert(button.update(true));
    for (int i = 0; i < 100; ++i) assert(!button.update(true));
    assert(!button.update(false));
    assert(button.update(true));
}

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

static void test_pulse_timing() {
    for (int command : {DOORCOMMANDOPEN, DOORCOMMANDCLOSE}) {
        const int pin = command == DOORCOMMANDOPEN ? CMD_OPENDOOR_OUTPUT : CMD_CLOSEDOOR_OUTPUT;
        for (unsigned long start : {10000UL, ULONG_MAX - 200UL}) {
            now_ms = start - 1000UL;
            driveio_setdoorcommand(command);
            // A delayed first loop must not shorten the actual HIGH pulse.
            assert(pins[pin] == LOW);
            now_ms = start;
            driveio_loop();
            assert(pins[pin] == HIGH);
            now_ms = start + 499UL;
            driveio_loop();
            assert(pins[pin] == HIGH);
            now_ms = start + 500UL;
            driveio_loop();
            assert(pins[pin] == LOW);
            assert(!driveio_doorcommandactive());
            int reportedPin = -1;
            unsigned long duration = 0;
            assert(driveio_takepulsereport(&reportedPin, &duration));
            assert(reportedPin == pin && duration == 500);
            assert(!driveio_takepulsereport(&reportedPin, &duration));
        }
    }
}

int main() {
    driveio_init();
    driveio_setdoorcommand(99);
    assert(!driveio_doorcommandactive());
    test_pulse(DOORCOMMANDOPEN, DOORCOMMANDCLOSE, CMD_OPENDOOR_OUTPUT, CMD_CLOSEDOOR_OUTPUT);
    test_pulse(DOORCOMMANDCLOSE, DOORCOMMANDOPEN, CMD_CLOSEDOOR_OUTPUT, CMD_OPENDOOR_OUTPUT);
    test_logging();
    test_repeated_commands_preserve_motion();
    test_pulse_timing();
    test_input_capture();
    test_mqtt_delivery();
    test_time_math();
    puts("PASS: elapsed timers at boundaries and across 32-bit counter wrap");
    puts("PASS: retained restart deletion, ACK gating, failure retry/wrap, duplicate requests and send counts");
    puts("PASS: input capture, unchanged levels, overflow, timer wrap and reset");
    puts("PASS: delayed pulse start, exact duration, timer wrap, one-shot duration reports");
    puts("PASS: no inferred stop/reversal from repeated commands, end switches, unknown state, held buttons");
    puts("PASS: command interlock, duplicate pulses, invalid commands, bounded MQTT logging");
}
