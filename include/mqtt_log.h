#ifndef GDC_MQTT_LOG_H
#define GDC_MQTT_LOG_H

#include <stddef.h>
#include <stdio.h>

// Incoming MQTT payloads are untrusted. Truncate diagnostics to the buffer size.
inline void mqtt_format_received(char* buffer, size_t capacity,
                                 const char* topic, const char* payload,
                                 bool valid)
{
    if (capacity == 0) return;
    snprintf(buffer, capacity, "RUN: Subscribe%s: set %s to %s",
             valid ? "" : " (invalid)", topic, payload);
    buffer[capacity - 1] = '\0';
}

#endif
