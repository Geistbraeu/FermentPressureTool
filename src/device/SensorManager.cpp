#include "device/SensorManager.h"
#include "config.h"
#include "debug.h"
#include <Adafruit_ADS1X15.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>
#include <esp_adc_cal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const int sensorPin = HardwareConfig::ADC_PRESSURE_PIN;
static Adafruit_ADS1115 ads1115;
static bool ads1115Available = false;
static unsigned long lastAds1115DebugAt = 0;
static bool ads1115UnavailableReported = false;
static OneWire oneWireBus(HardwareConfig::TEMP_SENSOR_PIN);
static DallasTemperature tempSensor(&oneWireBus);
static bool tempSensorInitialized = false;

SensorManager sensorManager;

void SensorManager::initAdc(esp_adc_cal_characteristics_t* adcChars) {
  analogSetAttenuation(ADC_11db);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, SensorConfig::ADC_VREF, adcChars);

  DBG("Scanning I2C bus for ADS1115...");
  bool ads1115AddressFound = false;
  for (uint8_t address = 0x03; address <= 0x77; address++) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      DBGF("I2C device found at 0x%02X\n", address);
      if (address == SensorConfig::ADS1115_I2C_ADDRESS) {
        ads1115AddressFound = true;
      }
    }
  }

  DBGF("ADS1115 probe address=0x%02X present=%s\n",
       SensorConfig::ADS1115_I2C_ADDRESS,
       ads1115AddressFound ? "yes" : "no");
  ads1115Available = ads1115.begin(SensorConfig::ADS1115_I2C_ADDRESS, &Wire);
  if (ads1115Available) {
    ads1115.setGain(GAIN_TWOTHIRDS);
    ads1115.setDataRate(RATE_ADS1115_16SPS);
    DBGF("ADS1115 initialized: address=0x%02X channel=A%d gain=2/3 range=6.144V rate=16SPS\n",
         SensorConfig::ADS1115_I2C_ADDRESS,
         SensorConfig::ADS1115_PRESSURE_CHANNEL);
  } else {
    DBGF("ADS1115 initialization FAILED: address=0x%02X\n", SensorConfig::ADS1115_I2C_ADDRESS);
  }
}

float SensorManager::readEsp32AdcVoltage(const esp_adc_cal_characteristics_t* adcChars) {
  const int rawValue = analogRead(sensorPin);
  const uint32_t voltageMv = esp_adc_cal_raw_to_voltage(rawValue, adcChars);
  return voltageMv / 1000.0f;
}

float SensorManager::readAds1115Voltage() {
  if (!ads1115Available) {
    if (!ads1115UnavailableReported) {
      DBG("ADS1115 read skipped: device is unavailable");
      ads1115UnavailableReported = true;
    }
    return -1.0f;
  }
  const int16_t rawValue = ads1115.readADC_SingleEnded(SensorConfig::ADS1115_PRESSURE_CHANNEL);
  const float voltage = ads1115.computeVolts(rawValue);
  const unsigned long now = millis();
  if (lastAds1115DebugAt == 0 || (unsigned long)(now - lastAds1115DebugAt) >= 1000) {
    DBGF("ADS1115 read: channel=A%d raw=%d voltage=%.4fV\n",
         SensorConfig::ADS1115_PRESSURE_CHANNEL, rawValue, voltage);
    lastAds1115DebugAt = now;
  }
  return voltage;
}

void SensorManager::resetAdaptivePressure() {
  adaptivePressureInitialized = false;
  adaptivePressureFiltered = 0.0f;
  adaptivePreviousError = 0.0f;
  adaptiveTrendCounter = 0;
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

SensorReading SensorManager::readFilteredPressure(unsigned int sampleCount, unsigned long sampleDelayMs,
                                                  uint8_t adcSource, float offsetVoltage,
                                                  bool isValveOpen,
                                                  bool adaptiveFilterEnabled,
                                                  float adaptiveAlphaMin,
                                                  float adaptiveAlphaMax,
                                                  float adaptiveDeltaRefPsi,
                                                  float adaptiveJitterDeadbandPsi,
                                                  const esp_adc_cal_characteristics_t* adcChars) {
  float samples[ControlConfig::MAX_MEDIAN_SAMPLES];

  if (!pressureAdcInitialized || activePressureAdc != adcSource) {
    resetAdaptivePressure();
    activePressureAdc = adcSource;
    pressureAdcInitialized = true;
    DBGF("Pressure ADC source: %s\n",
         adcSource == SensorConfig::PRESSURE_ADC_ADS1115 ? "ADS1115" : "ESP32");
  }

  if (adcSource == SensorConfig::PRESSURE_ADC_ADS1115 && !ads1115Available) {
    if (!ads1115UnavailableReported) {
      DBG("Pressure read unavailable: ADS1115 selected but not initialized");
      ads1115UnavailableReported = true;
    }
    SensorReading unavailableReading;
    return unavailableReading;
  }

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
    samples[i] = adcSource == SensorConfig::PRESSURE_ADC_ESP32
                     ? readEsp32AdcVoltage(adcChars) * SensorConfig::ADC_VOLTAGE_DIVIDER
                     : readAds1115Voltage();
    vTaskDelay(pdMS_TO_TICKS(sampleDelayMs));
  }

  for (unsigned int i = 0; i < sampleCount - 1; i++) {
    for (unsigned int j = 0; j < sampleCount - i - 1; j++) {
      if (samples[j] > samples[j + 1]) {
        float temp = samples[j];
        samples[j] = samples[j + 1];
        samples[j + 1] = temp;
      }
    }
  }
  const float sensorVoltage = samples[sampleCount / 2];

  float pressure = 0.0f;
  if (sensorVoltage > offsetVoltage) {
    pressure = (sensorVoltage - offsetVoltage) * SensorConfig::PRESSURE_PSI_RANGE / SensorConfig::PRESSURE_VOLTAGE_RANGE;
  }

  if (!adaptiveFilterEnabled) {
    resetAdaptivePressure();
  } else if (isValveOpen) {
    adaptivePressureInitialized = false;
    adaptivePreviousError = 0.0f;
    adaptiveTrendCounter = 0;
  } else {
    // Adaptive EMA tuned for jitter suppression:
    // very smooth for tiny changes, but fast at >= 1 psi steps.
    float alphaMin = adaptiveAlphaMin;
    float alphaMax = adaptiveAlphaMax;
    float deltaRefPsi = adaptiveDeltaRefPsi;
    float jitterDeadbandPsi = adaptiveJitterDeadbandPsi;
    if (alphaMin <= 0.0f || alphaMin >= 1.0f) {
      alphaMin = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MIN;
    }
    if (alphaMax <= 0.0f || alphaMax > 1.0f) {
      alphaMax = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MAX;
    }
    if (alphaMin >= alphaMax) {
      alphaMin = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MIN;
      alphaMax = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MAX;
    }
    if (deltaRefPsi <= 0.01f) {
      deltaRefPsi = ControlConfig::DEFAULT_ADAPTIVE_DELTA_REF_PSI;
    }
    if (jitterDeadbandPsi < 0.0f || jitterDeadbandPsi >= deltaRefPsi) {
      jitterDeadbandPsi = ControlConfig::DEFAULT_ADAPTIVE_JITTER_DEADBAND_PSI;
    }

    if (!adaptivePressureInitialized) {
      adaptivePressureFiltered = pressure;
      adaptivePressureInitialized = true;
      adaptivePreviousError = 0.0f;
      adaptiveTrendCounter = 0;
    } else {
      const float error = pressure - adaptivePressureFiltered;
      float delta = fabsf(error);

      // If error keeps same direction for many samples, gently raise alpha floor
      // so slow monotonic pressure trends are tracked with less lag.
      constexpr uint8_t kTrendCounterMax = 40;
      constexpr float kTrendBoostMax = 0.10f;
      const bool errorAboveNoise = delta > jitterDeadbandPsi;
      const bool sameDirection = (error * adaptivePreviousError) > 0.0f;
      if (errorAboveNoise && sameDirection) {
        if (adaptiveTrendCounter < kTrendCounterMax) {
          adaptiveTrendCounter++;
        }
      } else if (errorAboveNoise) {
        adaptiveTrendCounter = 1;
      } else if (adaptiveTrendCounter > 0) {
        adaptiveTrendCounter--;
      }
      adaptivePreviousError = error;

      float alphaFloor = alphaMin;
      if (adaptiveTrendCounter > 0) {
        const float trendRatio = (float)adaptiveTrendCounter / (float)kTrendCounterMax;
        alphaFloor += kTrendBoostMax * trendRatio;
      }
      if (alphaFloor >= alphaMax) {
        alphaFloor = alphaMax - 0.01f;
      }

      float adapt = (delta - jitterDeadbandPsi) / (deltaRefPsi - jitterDeadbandPsi);
      if (adapt < 0.0f) {
        adapt = 0.0f;
      }
      if (adapt > 1.0f) {
        adapt = 1.0f;
      }
      adapt = adapt * adapt;
      float alpha = alphaFloor + (alphaMax - alphaFloor) * adapt;
      adaptivePressureFiltered += alpha * (pressure - adaptivePressureFiltered);
    }
    pressure = adaptivePressureFiltered;
  }

  SensorReading reading;
  reading.voltage = sensorVoltage;
  reading.pressure = pressure;
  return reading;
}
