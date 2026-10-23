#include <cassert>
#include <cstdint>
#include <vector>
#include <Arduino.h>
#include "config.h"
#include "device/FerminatorButtons.h"

namespace {
  struct Event { int pin; int level; uint32_t at; };
  uint32_t now = 0;
  std::vector<Event> events;
}

unsigned long millis() { return now; }
void delay(unsigned long ms) { now += ms; }
void digitalWrite(int pin, int level) { events.push_back({pin, level, now}); }
void pinMode(int pin, int mode) {
  assert(mode == OUTPUT);
  assert(events.back().pin == pin && events.back().level == LOW);
}

int main() {
  using namespace FerminatorButtons;
  assert(press(Button::StartStop) == Result::NotInitialized);
  assert(events.empty());
  begin();
  assert(events.size() == 4);
  const int pins[] = {10, 11, 12, 13};
  for (int i = 0; i < 4; ++i) {
    assert(events[i].pin == pins[i] && events[i].level == LOW);
  }

  const Button buttons[] = {Button::TempUp, Button::TempDown, Button::StartStop, Button::FanSpeed};
  for (int i = 0; i < 4; ++i) {
    events.clear();
    const uint32_t start = now;
    assert(press(buttons[i]) == Result::Pressed);
    assert(events.size() == 2);
    assert(events[0].pin == pins[i] && events[0].level == HIGH && events[0].at == start);
    assert(events[1].pin == pins[i] && events[1].level == LOW);
    assert(events[1].at - start == FerminatorButtonConfig::PRESS_MS);
    assert(press(Button::StartStop) == Result::Busy);
    now += FerminatorButtonConfig::RELEASE_MS - 1;
    assert(press(Button::TempUp) == Result::Busy);
    assert(events.size() == 2);
    now += 1;
  }

  events.clear();
  assert(press(static_cast<Button>(99)) == Result::InvalidButton);
  assert(events.empty());

  now = UINT32_MAX - 100;
  begin();
  assert(press(Button::StartStop) == Result::Pressed);
  assert(now == 99);
  assert(press(Button::StartStop) == Result::Busy);
  now += FerminatorButtonConfig::RELEASE_MS;
  assert(press(Button::StartStop) == Result::Pressed);

  now = UINT32_MAX - 300;
  begin();
  assert(press(Button::FanSpeed) == Result::Pressed);
  now += FerminatorButtonConfig::RELEASE_MS;
  assert(press(Button::FanSpeed) == Result::Pressed);
}
