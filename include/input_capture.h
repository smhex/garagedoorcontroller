#pragma once
#include <stdint.h>
#include <stddef.h>

// Fixed-size transition recorder; no allocation or output during acquisition.
template <size_t Capacity>
class InputCapture {
public:
    struct Sample { uint32_t us; uint8_t levels; };
    Sample samples[Capacity];
    size_t count = 0;
    uint32_t dropped = 0;
    uint32_t reads = 0;
    uint32_t maxGapUs = 0;

    void reset(uint32_t now) {
        count = 0;
        dropped = reads = maxGapUs = 0;
        started = previousTime = now;
        previousLevels = 255;
    }
    void record(uint32_t now, uint8_t levels) {
        const uint32_t gap = now - previousTime;
        if (gap > maxGapUs) maxGapUs = gap;
        previousTime = now;
        ++reads;
        if (levels == previousLevels) return;
        previousLevels = levels;
        if (count < Capacity) samples[count++] = {now - started, levels};
        else ++dropped;
    }
private:
    uint32_t started = 0;
    uint32_t previousTime = 0;
    uint8_t previousLevels = 255;
};
