#include <Arduino.h>
#include "FerminatorButtons.h"
#include "config.h"

namespace FerminatorButtons {
  namespace {
    bool initialized = false;
    bool hasPressed = false;
    uint32_t releasedAt = 0;

    int pinFor(Button button) {
      switch (button) {
        case Button::TempUp: return FerminatorButtonConfig::TEMP_UP_PIN;
        case Button::TempDown: return FerminatorButtonConfig::TEMP_DOWN_PIN;
        case Button::StartStop: return FerminatorButtonConfig::START_STOP_PIN;
        case Button::FanSpeed: return FerminatorButtonConfig::FAN_SPEED_PIN;
      }
      return -1;
    }
  }

  void begin() {
    const int pins[] = {
      FerminatorButtonConfig::TEMP_UP_PIN,
      FerminatorButtonConfig::TEMP_DOWN_PIN,
      FerminatorButtonConfig::START_STOP_PIN,
      FerminatorButtonConfig::FAN_SPEED_PIN
    };
    for (int pin : pins) {
      digitalWrite(pin, LOW);
      pinMode(pin, OUTPUT);
    }
    hasPressed = false;
    initialized = true;
  }

  // Called only by the serialized HTTP task; release precedes the HTTP response.
  Result press(Button button) {
    const int pin = pinFor(button);
    if (pin < 0) return Result::InvalidButton;
    if (!initialized) return Result::NotInitialized;
    if (hasPressed &&
        static_cast<uint32_t>(millis() - releasedAt) < FerminatorButtonConfig::RELEASE_MS) {
      return Result::Busy;
    }

    digitalWrite(pin, HIGH);
    delay(FerminatorButtonConfig::PRESS_MS);
    digitalWrite(pin, LOW);
    releasedAt = millis();
    hasPressed = true;
    return Result::Pressed;
  }
}
