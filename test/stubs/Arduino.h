#pragma once
#include <cstdint>

constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 3;

unsigned long millis();
void delay(unsigned long ms);
void digitalWrite(int pin, int level);
void pinMode(int pin, int mode);
