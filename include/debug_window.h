#pragma once
#include <stdint.h>

class DebugWindow {
public:
    bool enabled(uint32_t now) {
        if (now >= 30000) expired = true;
        return continuous || !expired;
    }
    void enable() { continuous = true; }
private:
    bool expired = false;
    bool continuous = false;
};
