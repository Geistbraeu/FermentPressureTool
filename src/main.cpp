#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include <ESPmDNS.h>
#include "config.h"
#include "ConfigPortal.h"
#include "web_server.h"
#include "CloudManager.h"
#include "Settings.h"
#include "RuntimeState.h"
#include "debug.h"
#include "device/DisplayManager.h"
#include "device/OledMirror.h"
#include "device/SensorManager.h"
#include "device/SolenoidController.h"

// Глобальные переменные для обмена данными между ядрами
RuntimeState runtimeState;

// Прототипы функций
void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);
void processSampledData();

void setup() {
  Serial.begin(NetworkConfig::SERIAL_BAUD_RATE);
  delay(300);
  Serial.println("\n[BOOT] FermentPressureTool starting");
  Serial.printf("[BOOT] Reset reason: %d\n", static_cast<int>(esp_reset_reason()));

  Serial.println("[BOOT] Initializing I2C on GPIO 5 (SDA), GPIO 4 (SCL)");
  Wire.begin(DisplayConfig::OLED_I2C_SDA_PIN, DisplayConfig::OLED_I2C_SCL_PIN);
  Wire.setTimeOut(50);

  // Настройка АЦП и чтение калибровки из eFuse
  Serial.println("[BOOT] Initializing pressure ADC; ADS1115 is optional");
  sensorManager.initAdc(&runtimeState.adc_chars);

  // Инициализация OLED
  Serial.println("[BOOT] Initializing local OLED; display is optional");
  displayManager.init();

  // Инициализация Wi-Fi через ConfigPortal
  Serial.println("[BOOT] Starting Wi-Fi connection/configuration");
  const bool wifiConnected = ConfigPortal::begin();
  Serial.println(wifiConnected
                     ? "[BOOT] Wi-Fi connected; setup portal not needed"
                     : "[BOOT] Wi-Fi unavailable; setup portal was entered");

  // Инициализация клапана
  solenoidController.init();

  // Инициализация mDNS
  const String mdnsHost = wifiSettings.devName.isEmpty() ? String(NetworkConfig::HOSTNAME) : wifiSettings.devName;
  if (!MDNS.begin(mdnsHost.c_str())) {
    DBG("mDNS ERROR");
  } else {
    DBG("mDNS responder started: " + mdnsHost + ".local");
    MDNS.addService("http", "tcp", NetworkConfig::WEBSERVER_PORT);
  }
 
  // Чтение настроек
  settings.load();

  // Инициализация DS18B20
  Serial.println("[BOOT] Initializing DS18B20; temperature sensor is optional");
  sensorManager.initTempSensor(settings.useTempSensor);

  // Создаем мьютекс для защиты общих данных
  Serial.println("[BOOT] Creating runtime mutexes");
  runtimeState.dataMutex = xSemaphoreCreateMutex();
  runtimeState.settingsMutex = xSemaphoreCreateMutex();
  if (runtimeState.dataMutex == NULL || runtimeState.settingsMutex == NULL) {
    Serial.println("FATAL: mutex creation failed");
    for (;;) {
      delay(1000);
    }
  }

  // Инициализация веб-сервера
  Serial.println("[BOOT] Starting web server");
  initWebServer();

  if (!OledMirror::begin()) {
    Serial.println("OLED mirror initialization failed");
  }

  // Задача для чтения сенсора (Core 1)
  const BaseType_t sensorTaskResult = xTaskCreatePinnedToCore(
    sensorTask,
    "SensorTask",
    TaskConfig::SENSOR_TASK_STACK_SIZE,
    NULL,
    TaskConfig::SENSOR_TASK_PRIORITY,
    NULL,
    TaskConfig::SENSOR_TASK_CORE
  );
  if (sensorTaskResult != pdPASS) {
    Serial.println("[BOOT] FATAL: failed to create sensor task");
    for (;;) {
      delay(1000);
    }
  }

  // Задача для Wi-Fi и облаков (Core 0)
  const BaseType_t networkTaskResult = xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    TaskConfig::NETWORK_TASK_STACK_SIZE,
    NULL,
    TaskConfig::NETWORK_TASK_PRIORITY,
    NULL,
    TaskConfig::NETWORK_TASK_CORE
  );
  if (networkTaskResult != pdPASS) {
    Serial.println("[BOOT] FATAL: failed to create network task");
    for (;;) {
      delay(1000);
    }
  }

  DBG("\n--- Система запущена на двух ядрах ---");
  Serial.println("[BOOT] Startup complete");
}

void loop() {
  // Пустой цикл, всё работает в задачах FreeRTOS
  vTaskDelete(NULL); 
}

void processSampledData() {
  bool useTempSensor = false;
  float maxPressureThreshold = 0.0f;
  float hysteresis = 0.0f;
  float tempOffset = 0.0f;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    useTempSensor = settings.useTempSensor;
    maxPressureThreshold = settings.maxPressureThreshold;
    hysteresis = settings.hysteresis;
    tempOffset = settings.tempOffset;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  bool isTempConnected = false;
  float t = sensorManager.readTemperature(useTempSensor, tempOffset, &isTempConnected);
  runtimeState.isTempSensorConnected = isTempConnected;

  if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    if (runtimeState.hasSampledData) {
      runtimeState.currentVoltage = runtimeState.sampledVoltage;
      runtimeState.currentPressure = runtimeState.sampledPressure;
      runtimeState.isPressureSensorConnected = runtimeState.currentVoltage >= SensorConfig::PRESSURE_DISCONNECTED_THRESHOLD_V;
      runtimeState.currentTemp = t;
      runtimeState.isDataReady = true;

        if (runtimeState.manualOverride &&
          (unsigned long)(millis() - runtimeState.manualStartTime) > ControlConfig::MANUAL_OVERRIDE_TIMEOUT_MS) {
          runtimeState.manualOverride = false;
      }

      if (runtimeState.manualOverride) {
          solenoidController.applyState(true, runtimeState.manualOn, runtimeState.currentPressure, maxPressureThreshold, hysteresis);
      } else {
          solenoidController.applyState(false, false, runtimeState.currentPressure, maxPressureThreshold, hysteresis);
      }
    }
    xSemaphoreGive(runtimeState.dataMutex);
  }
}

// --- ЛОГИКА СЕНСОРА (Core 1) ---
void sensorTask(void *pvParameters) {
  unsigned long lastProcessAt = 0;

  for (;;) {
    unsigned int sampleCount = ControlConfig::DEFAULT_MEDIAN_SAMPLE_COUNT;
    unsigned long medianSampleDelayMs = ControlConfig::DEFAULT_MEDIAN_SAMPLE_DELAY_MS;
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    uint8_t pressureAdc = SensorConfig::PRESSURE_ADC_ESP32;
    bool adaptiveFilterEnabled = true;
    float offsetVoltage = SensorConfig::PRESSURE_OFFSET_DEFAULT;
    float adaptiveAlphaMin = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MIN;
    float adaptiveAlphaMax = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MAX;
    float adaptiveDeltaRefPsi = ControlConfig::DEFAULT_ADAPTIVE_DELTA_REF_PSI;
    float adaptiveJitterDeadbandPsi = ControlConfig::DEFAULT_ADAPTIVE_JITTER_DEADBAND_PSI;
    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      sampleCount = settings.medianSampleCount;
      medianSampleDelayMs = settings.medianSampleDelayMs;
      updateIntervalMs = settings.updateIntervalMs;
      pressureAdc = settings.pressureAdc;
      adaptiveFilterEnabled = settings.adaptiveFilterEnabled;
      offsetVoltage = settings.offsetVoltage;
      adaptiveAlphaMin = settings.adaptiveAlphaMin;
      adaptiveAlphaMax = settings.adaptiveAlphaMax;
      adaptiveDeltaRefPsi = settings.adaptiveDeltaRefPsi;
      adaptiveJitterDeadbandPsi = settings.adaptiveJitterDeadbandPsi;
      xSemaphoreGive(runtimeState.settingsMutex);
    }

    bool isValveOpen = digitalRead(HardwareConfig::SOLENOID_PIN) == HIGH;
    SensorReading reading = sensorManager.readFilteredPressure(sampleCount,
                                   medianSampleDelayMs,
                                   pressureAdc,
                                   offsetVoltage,
                                   isValveOpen,
                                   adaptiveFilterEnabled,
                                                               adaptiveAlphaMin,
                                                               adaptiveAlphaMax,
                                                               adaptiveDeltaRefPsi,
                                                               adaptiveJitterDeadbandPsi,
                                   &runtimeState.adc_chars);

    // 4. Непрерывно публикуем фильтрованные значения в RuntimeState
    if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      runtimeState.sampledVoltage = reading.voltage;
      runtimeState.sampledPressure = reading.pressure;
      runtimeState.hasSampledData = true;
      xSemaphoreGive(runtimeState.dataMutex);
    }

    // 5. Периодическая обработка sampled данных по updateIntervalMs
    unsigned long now = millis();
    if (lastProcessAt == 0 || (now - lastProcessAt) >= updateIntervalMs) {
      processSampledData();
      lastProcessAt = now;
    }
  }
}

// --- ЛОГИКА СЕТИ (Core 0) ---
void networkTask(void *pvParameters) {
  initCloud();

  for (;;) {
    float vLocal = 0.0, pLocal = 0.0, tLocal = 0.0;
    uint32_t valveActivationsPerHourLocal = 0;
    bool isTempSensorConnected = false;
    bool isPressureSensorConnected = false;
    bool ready = false;
    int pressureUnit = 0;
    float maxPressureThreshold = 0.0f;
    bool useTempSensor = false;
    unsigned long oledMetricSwitchSeconds = ControlConfig::DEFAULT_OLED_METRIC_SWITCH_SECONDS;

    // Проверка подключения
    if (WiFi.status() != WL_CONNECTED) {
      ConfigPortal::connect();
    }

    // Получаем копию данных и статус готовности
    if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      vLocal = runtimeState.currentVoltage;
      pLocal = runtimeState.currentPressure;
      tLocal = runtimeState.currentTemp;
      valveActivationsPerHourLocal = runtimeState.valveActivationsPerHour;
      isTempSensorConnected = runtimeState.isTempSensorConnected;
      isPressureSensorConnected = runtimeState.isPressureSensorConnected;
      ready = runtimeState.isDataReady;
      xSemaphoreGive(runtimeState.dataMutex);
    }

    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      pressureUnit = settings.pressureUnit;
      maxPressureThreshold = settings.maxPressureThreshold;
      useTempSensor = settings.useTempSensor;
      oledMetricSwitchSeconds = settings.oledMetricSwitchSeconds;
      xSemaphoreGive(runtimeState.settingsMutex);
    }

    // Обновление дисплея
    String ipStr = "Connecting...";
    if (WiFi.status() == WL_CONNECTED) {
      ipStr = WiFi.localIP().toString();
    }
    float pBar = pLocal * SensorConfig::PSI_TO_BAR;
    displayManager.update(ipStr,
                          vLocal,
                          pBar,
                          pressureUnit,
                          maxPressureThreshold,
                          tLocal,
                          useTempSensor,
                          isTempSensorConnected,
                          oledMetricSwitchSeconds);

    // Выполняем отправку только если данные уже были считаны сенсором
    if (ready && isPressureSensorConnected) {
      sendDataToThingSpeak(vLocal, pLocal, pBar, tLocal, valveActivationsPerHourLocal);
      sendDataToBrewfather(vLocal, pLocal, tLocal, valveActivationsPerHourLocal);
      sendDataViaCustomHTTP(vLocal, pLocal, pBar, tLocal, valveActivationsPerHourLocal);
    }
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      updateIntervalMs = settings.updateIntervalMs;
      xSemaphoreGive(runtimeState.settingsMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(updateIntervalMs));
  }
}
