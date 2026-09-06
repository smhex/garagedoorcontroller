#pragma once
#include <stdint.h>
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLDOWN = 2;
unsigned long millis();
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
void yield();
