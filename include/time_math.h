#pragma once

#include <stdint.h>

// Unsigned subtraction handles the 32-bit millis()/micros() counter wrapping.
inline bool time_elapsed(uint32_t now, uint32_t started, uint32_t duration)
{
    return uint32_t(now - started) >= duration;
}
