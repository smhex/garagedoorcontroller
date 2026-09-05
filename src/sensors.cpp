#include <Arduino_MKRENV.h>
#include <math.h>
#include "sensors.h"

namespace {
bool initialized = false;
bool valid = false;
unsigned long lastAttempt = 0;
unsigned long lastSample = 0;
float temperature = NAN, humidity = NAN, pressure = NAN, illuminance = NAN;
void invalidate() {
    valid = false;
    temperature = humidity = pressure = illuminance = NAN;
}
void initialize() {
    initialized = ENV.begin() != 0;
    lastAttempt = millis();
    lastSample = millis() - 10000UL;
    Serial.println(initialized ? "SENSORS: shield ready" :
                   "SENSORS: unavailable; controller continues; retry in 30 seconds");
}
}
void sensors_init() {
    invalidate();
    initialize();
}
void sensors_loop() {
    if (!initialized) {
        if (millis() - lastAttempt >= 30000UL) initialize();
        return;
    }
    if (millis() - lastSample < 10000UL) return;
    const float t = ENV.readTemperature();
    const float h = isfinite(t) ? ENV.readHumidity() : NAN;
    const float p = isfinite(h) ? ENV.readPressure() : NAN;
    const float l = isfinite(p) ? ENV.readIlluminance() : NAN;
    lastSample = millis();
    if (!isfinite(t) || !isfinite(h) || !isfinite(p) || !isfinite(l) ||
        h < 0 || h > 100 || p <= 0 || l < 0) {
        invalidate();
        initialized = false;
        lastAttempt = millis();
        Serial.println("SENSORS: invalid reading; values unavailable; retry in 30 seconds");
        return;
    }
    temperature = t;
    humidity = h;
    pressure = p;
    illuminance = l < 0.0001f ? 0.0001f : l;
    valid = true;
}
bool sensors_isvalid() { return valid && millis() - lastSample < 30000UL; }
float sensors_get_temperature() { return sensors_isvalid() ? temperature : NAN; }
float sensors_get_humidity() { return sensors_isvalid() ? humidity : NAN; }
float sensors_get_pressure() { return sensors_isvalid() ? pressure : NAN; }
float sensors_get_illuminance() { return sensors_isvalid() ? illuminance : NAN; }
