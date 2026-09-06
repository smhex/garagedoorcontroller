#pragma once
#include <stdint.h>
#include <stddef.h>
constexpr int A2 = 2;
constexpr int INPUT = 0;
unsigned long millis();
void yield();
void pinMode(int, int);
int analogRead(int);
struct SerialStub { void println(const char*) {} };
extern SerialStub Serial;
