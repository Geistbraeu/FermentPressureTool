#include "device/SensorManager.h"
#include "config.h"
#include "debug.h"
#include <DallasTemperature.h>
#include <OneWire.h>
#include <esp_adc_cal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const int sensorPin = HardwareConfig::ADC_PRESSURE_PIN;
static OneWire oneWireBus(HardwareConfig::TEMP_SENSOR_PIN);
static DallasTemperature tempSensor(&oneWireBus);
static bool tempSensorInitialized = false;

SensorManager sensorManager;

void SensorManager::initAdc(esp_adc_cal_characteristics_t* adcChars) {
  analogSetAttenuation(ADC_11db);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, SensorConfig::ADC_VREF, adcChars);
}

void SensorManager::initTempSensor(bool useTempSensor) {
  (void)useTempSensor;

  if (!tempSensorInitialized) {
    tempSensor.begin();
    tempSensorInitialized = true;
  }
}

float SensorManager::readTemperature(bool isEnabled, float tempOffset, bool* isConnected) {
  if (!isEnabled) {
    if (isConnected != NULL) {
      *isConnected = false;
    }
    return 0.0f;
  }

  if (!tempSensorInitialized) {
    initTempSensor(true);
  }

  if (tempSensor.getDeviceCount() == 0) {
    if (isConnected != NULL) {
      *isConnected = false;
    }
    return 0.0f;
  }

  tempSensor.requestTemperatures();
  float t = tempSensor.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C || t <= SensorConfig::TEMP_DISCONNECTED_C) {
    DBG("DS18B20 disconnected or read failed");
    if (isConnected != NULL) {
      *isConnected = false;
    }
    return 0.0f;
  }

  if (isConnected != NULL) {
    *isConnected = true;
  }
  return t - tempOffset;
}

SensorReading SensorManager::readFilteredPressure(unsigned int sampleCount, unsigned long sampleDelayMs, float offsetVoltage,
                                                  const esp_adc_cal_characteristics_t* adcChars) {
  int samples[ControlConfig::MAX_MEDIAN_SAMPLES];

  if (sampleCount < ControlConfig::MIN_MEDIAN_SAMPLES) {
    sampleCount = ControlConfig::MIN_MEDIAN_SAMPLES;
  }
  if (sampleCount > ControlConfig::MAX_MEDIAN_SAMPLES) {
    sampleCount = ControlConfig::MAX_MEDIAN_SAMPLES;
  }
  if ((sampleCount % 2) == 0) {
    sampleCount++;
    if (sampleCount > ControlConfig::MAX_MEDIAN_SAMPLES) {
      sampleCount = ControlConfig::MAX_MEDIAN_SAMPLES;
    }
  }

  for (unsigned int i = 0; i < sampleCount; i++) {
    samples[i] = analogRead(sensorPin);
    vTaskDelay(pdMS_TO_TICKS(sampleDelayMs));
  }

  for (unsigned int i = 0; i < sampleCount - 1; i++) {
    for (unsigned int j = 0; j < sampleCount - i - 1; j++) {
      if (samples[j] > samples[j + 1]) {
        int temp = samples[j];
        samples[j] = samples[j + 1];
        samples[j + 1] = temp;
      }
    }
  }



  int medianRaw = samples[sampleCount / 2];
  uint32_t voltageMv = esp_adc_cal_raw_to_voltage(medianRaw, adcChars);
  float measuredVoltage = voltageMv / 1000.0f;
  float sensorVoltage = measuredVoltage * SensorConfig::ADC_VOLTAGE_DIVIDER;

  float pressure = 0.0f;
  if (sensorVoltage > offsetVoltage) {
    pressure = (sensorVoltage - offsetVoltage) * SensorConfig::PRESSURE_PSI_RANGE / SensorConfig::PRESSURE_VOLTAGE_RANGE;
  }

  SensorReading reading;
  reading.voltage = sensorVoltage;
  reading.pressure = pressure;
  return reading;
}
