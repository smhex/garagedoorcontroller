#pragma once
#include <stdint.h>

// A button held before the request cannot approve it. Consume only fresh,
// debounced HMI samples and require release followed by a new press.
class LocalApproval {
public:
    void begin(uint32_t sample) { lastSample = sample; released = false; }
    bool update(uint32_t sample, bool pressed) {
        if (sample == lastSample) return false;
        lastSample = sample;
        if (!pressed) released = true;
        return released && pressed;
    }
private:
    uint32_t lastSample = 0;
    bool released = false;
};
