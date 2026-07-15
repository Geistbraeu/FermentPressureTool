#ifndef RUNTIMESTATE_H
#define RUNTIMESTATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_adc_cal.h>

struct RuntimeState {
    float sampledVoltage = 0.0;
    float sampledPressure = 0.0;
    bool hasSampledData = false;
    float currentVoltage = 0.0;
    float currentPressure = 0.0;
    float currentTemp = 0.0;
    bool manualOverride = false;
    bool manualOn = false;
    unsigned long manualStartTime = 0;
    uint32_t valveActivationsPerHour = 0;
    uint32_t valveActivationLastSecond = 0;
    uint16_t valveActivationBuckets[3600] = {0};
    bool isDataReady = false;
    bool isTempSensorConnected = false;
    bool isPressureSensorConnected = false;
    SemaphoreHandle_t dataMutex = NULL;
    SemaphoreHandle_t settingsMutex = NULL;
    esp_adc_cal_characteristics_t adc_chars{};
};

#endif // RUNTIMESTATE_H
