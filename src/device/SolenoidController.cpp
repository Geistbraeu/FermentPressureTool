#include "device/SolenoidController.h"
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "debug.h"
#include "RuntimeState.h"

SolenoidController solenoidController;
extern RuntimeState runtimeState;

static constexpr uint32_t kValveActivationWindowSeconds = 3600U;

static void trackValveActivationCycle(bool valveOpen) {
  const uint32_t nowSecond = millis() / 1000U;

  if (runtimeState.valveActivationLastSecond == 0U) {
    runtimeState.valveActivationLastSecond = nowSecond;
  }

  const uint32_t elapsedSeconds = nowSecond - runtimeState.valveActivationLastSecond;
  if (elapsedSeconds >= kValveActivationWindowSeconds) {
    memset(runtimeState.valveActivationBuckets, 0, sizeof(runtimeState.valveActivationBuckets));
    runtimeState.valveActivationsPerHour = 0U;
  } else if (elapsedSeconds > 0U) {
    for (uint32_t step = 1U; step <= elapsedSeconds; ++step) {
      const uint32_t staleIndex = (runtimeState.valveActivationLastSecond + step) % kValveActivationWindowSeconds;
      runtimeState.valveActivationsPerHour -= runtimeState.valveActivationBuckets[staleIndex];
      runtimeState.valveActivationBuckets[staleIndex] = 0;
    }
  }

  runtimeState.valveActivationLastSecond = nowSecond;

  if (valveOpen) {
    const uint32_t currentIndex = nowSecond % kValveActivationWindowSeconds;
    ++runtimeState.valveActivationBuckets[currentIndex];
    ++runtimeState.valveActivationsPerHour;
  }
}

void SolenoidController::init() {
  pinMode(HardwareConfig::SOLENOID_PIN, OUTPUT);
  digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);
}

void SolenoidController::applyState(bool manualOverride, bool manualOn, float currentPressure, float maxPressureThreshold, float hysteresis) {
  static bool initialized = false;
  static bool lastManualOverride = false;
  static bool lastValveOpen = false;

  if (manualOverride) {
    bool valveOpen = manualOn;
    digitalWrite(HardwareConfig::SOLENOID_PIN, valveOpen ? HIGH : LOW);
    if (!initialized || !lastManualOverride || valveOpen != lastValveOpen) {
      DBGF("[Valve] MANUAL %s\n", valveOpen ? "OPEN" : "CLOSED");
    }
    initialized = true;
    lastManualOverride = true;
    lastValveOpen = valveOpen;
    trackValveActivationCycle(valveOpen);
    return;
  }

  bool valveOpen = digitalRead(HardwareConfig::SOLENOID_PIN) == HIGH;
  bool desiredValveOpen = valveOpen;
  if (currentPressure > maxPressureThreshold) {
    desiredValveOpen = true;
  } else if (currentPressure < (maxPressureThreshold - hysteresis)) {
    desiredValveOpen = false;
  }

  if (desiredValveOpen != valveOpen) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, desiredValveOpen ? HIGH : LOW);
    valveOpen = desiredValveOpen;
  }

  if (!initialized || lastManualOverride || valveOpen != lastValveOpen) {
    DBGF("[Valve] AUTO %s (P=%.2f, threshold=%.2f, hysteresis=%.2f)\n",
         valveOpen ? "OPEN" : "CLOSED",
         currentPressure,
         maxPressureThreshold,
         hysteresis);
  }

  initialized = true;
  lastManualOverride = false;
  lastValveOpen = valveOpen;
  trackValveActivationCycle(valveOpen);
}
