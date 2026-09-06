#pragma once

#include <stdint.h>

// Accept a new digital input state only after it has remained unchanged for the
// configured interval. Unsigned subtraction also works across millis() wrap.
class InputStateFilter {
public:
    explicit InputStateFilter(uint32_t intervalMs) : interval(intervalMs) {}

    void reset(int initialState, uint32_t now) {
        stable = candidate = initialState;
        candidateSince = now;
    }

    int update(int rawState, uint32_t now) {
        if (rawState != candidate) {
            candidate = rawState;
            candidateSince = now;
        }
        if (candidate != stable && uint32_t(now - candidateSince) >= interval)
            stable = candidate;
        return stable;
    }

    // Keep sampling raw values while the controller drives the shared bus, but
    // do not let command-coupled levels become a stable door state.
    void discard(int rawState, uint32_t now) {
        candidate = rawState;
        candidateSince = now;
    }

private:
    const uint32_t interval;
    int stable = 0;
    int candidate = 0;
    uint32_t candidateSince = 0;
};
