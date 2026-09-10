#pragma once
#include <Arduino.h>

#ifdef ARDUINO
class DebugOutput : public Print {
public:
    using Print::write;
    size_t write(uint8_t value) override;
};
extern DebugOutput Debug;
void debug_console_loop();
#else
// Host tests substitute the Arduino Serial interface.
#define Debug Serial
#endif
