#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <esp_adc_cal.h>
#include "config.h"

struct SensorReading {
  float voltage = 0.0f;
  float pressure = 0.0f;
};

class SensorManager {
public:
  void initAdc(esp_adc_cal_characteristics_t* adcChars);
  void initTempSensor(bool useTempSensor);
  float readTemperature(bool isEnabled, float tempOffset, bool* isConnected);
  SensorReading readFilteredPressure(unsigned int sampleCount, unsigned long sampleDelayMs,
                                    uint8_t adcSource, float offsetVoltage,
                                    bool isValveOpen,
                                    bool adaptiveFilterEnabled,
                                    float adaptiveAlphaMin,
                                    float adaptiveAlphaMax,
                                    float adaptiveDeltaRefPsi,
                                    float adaptiveJitterDeadbandPsi,
                                    const esp_adc_cal_characteristics_t* adcChars);

private:
  float readEsp32AdcVoltage(const esp_adc_cal_characteristics_t* adcChars);
  float readAds1115Voltage();
  void resetAdaptivePressure();

  bool pressureAdcInitialized = false;
  uint8_t activePressureAdc = SensorConfig::PRESSURE_ADC_ESP32;
  bool adaptivePressureInitialized = false;
  float adaptivePressureFiltered = 0.0f;
  float adaptivePreviousError = 0.0f;
  uint8_t adaptiveTrendCounter = 0;
};

extern SensorManager sensorManager;

#endif // SENSOR_MANAGER_H
