#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

inline bool format_ipv4(char* buffer, size_t capacity,
                        uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    if (capacity == 0) return false;
    const int length = snprintf(buffer, capacity, "%u.%u.%u.%u", a, b, c, d);
    buffer[capacity - 1] = '\0';
    return length >= 0 && static_cast<size_t>(length) < capacity;
}
