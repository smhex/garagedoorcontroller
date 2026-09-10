#pragma once
#include <stdint.h>

class DebugWindow {
public:
    static const uint32_t automaticDurationMs = 60000;

    bool enabled(uint32_t) const { return continuous || !expired; }
    bool shouldExpire(uint32_t now) const { return !continuous && !expired && now >= automaticDurationMs; }
    void expire() { expired = true; }
    void enable() { continuous = true; }
private:
    bool expired = false;
    bool continuous = false;
};
