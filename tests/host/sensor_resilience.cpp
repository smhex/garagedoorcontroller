#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
#include "Arduino_MKRENV.h"
#include "sensors.h"

TwoWire Wire;
SerialStub Serial;
unsigned long now = 1000;
unsigned long millis() { return now; }
void yield() { ++now; }
void pinMode(int, int) {}
int analogRead(int) { return 100; }

void hardware() {
    Wire.present = true;
    Wire.registers[0x5f][0x0f] = 0xbc;
    Wire.registers[0x5c][0x0f] = 0xb1;
    Wire.registers[0x5f][0x31] = 200;
    Wire.registers[0x5f][0x33] = 80;
    Wire.registers[0x5f][0x3a] = 100;
    Wire.registers[0x5f][0x3e] = 100;
    Wire.registers[0x5f][0x27] = 3;
    Wire.registers[0x5f][0x28] = 50;
    Wire.registers[0x5f][0x2a] = 100;
    Wire.registers[0x5c][0x2a] = 63;
}

int main() {
    sensors_init();
    assert(!sensors_isvalid() && isnan(sensors_get_temperature()));
    assert(Wire.ended == 0); // Absent shield must not shut down the HMI bus.
    const int attempts = Wire.transactions;
    now += 29999;
    sensors_loop();
    assert(Wire.transactions == attempts);
    hardware();
    ++now;
    sensors_loop(); // Reinitialize after the retry interval.
    sensors_loop(); // First sample immediately due.
    assert(sensors_isvalid());
    assert(fabs(sensors_get_temperature() - 10.0f) < 0.01f);
    assert(fabs(sensors_get_humidity() - 50.0f) < 0.01f);
    const int reads = Wire.transactions;
    now += 9999;
    sensors_loop();
    assert(Wire.transactions == reads);
    ++now;
    Wire.failRead = true;
    sensors_loop();
    assert(!sensors_isvalid() && isnan(sensors_get_pressure()));
    Wire.failRead = false;
    now += 30000;
    sensors_loop(); sensors_loop();
    assert(sensors_isvalid());
    now += 30000;
    assert(!sensors_isvalid()); // Old data cannot masquerade as fresh data.
    sensors_loop();
    assert(sensors_isvalid());
    now += 10000;
    Wire.registers[0x5f][0x28] = 150;
    sensors_loop();
    assert(!sensors_isvalid()); // Invalid humidity invalidates the snapshot.
    Wire.registers[0x5f][0x28] = 50;

    Wire.stuck = true;
    Wire.registers[0x5f][0x21] = 1;
    now = ULONG_MAX - 100;
    unsigned long started = now;
    assert(isnan(ENV.readTemperature()));
    assert(now - started == 250);
    started = now;
    assert(isnan(ENV.readHumidity()));
    assert(now - started == 250);
    started = now;
    assert(isnan(ENV.readPressure()));
    assert(now - started == 250);
    Wire.stuck = false;
    Wire.registers[0x5f][0x21] = 0;
    Wire.registers[0x5f][0x27] = 0;
    started = now;
    assert(isnan(ENV.readTemperature()));
    assert(now - started == 250); // Data-ready wait, separate from busy wait.
    Wire.registers[0x5f][0x3e] = 0;
    assert(!ENV.begin()); // Degenerate calibration is rejected.
    assert(Wire.ended == 0);
    puts("PASS: absent shield, shared bus, retry, cadence, stale/invalid values, recovery, conversion deadlines and wrap");
}
