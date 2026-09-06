#pragma once
#include "Arduino.h"

class TwoWire {
public:
    bool present = false, failRead = false, stuck = false;
    int ended = 0, transactions = 0;
    uint8_t registers[128][256] = {};
    void begin() {}
    void end() { ++ended; }
    void beginTransmission(uint8_t value) { address = value; written = 0; }
    void write(uint8_t value) {
        if (written++ == 0) reg = value;
        else {
            registers[address][reg++] = value;
            if ((address == 0x5f && reg - 1 == 0x21) ||
                (address == 0x5c && reg - 1 == 0x11))
                registers[address][reg - 1] = stuck ? 1 : 0;
        }
    }
    int endTransmission(bool = true) { ++transactions; return present ? 0 : 2; }
    int requestFrom(uint8_t, int count) { return failRead ? 0 : count; }
    int read() { return registers[address][reg++]; }
private:
    uint8_t address = 0, reg = 0;
    int written = 0;
};
extern TwoWire Wire;
