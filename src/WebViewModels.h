#ifndef WEB_VIEW_MODELS_H
#define WEB_VIEW_MODELS_H

#include <Arduino.h>
#include "config.h"

struct RuntimeSnapshot {
    float pressure = 0.0f;
    float voltage = 0.0f;
    float temperature = 0.0f;
    uint32_t valveActivationsPerHour = 0;
    bool manualOverride = false;
    bool manualOn = false;
    unsigned long manualStartTime = 0;
    bool isTempSensorConnected = false;
    bool isPressureSensorConnected = false;
};

struct SettingsSnapshot {
    float maxPressureThreshold = 0.0f;
    int pressureUnit = 0;
    float hysteresis = 0.0f;
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    unsigned long oledMetricSwitchSeconds = ControlConfig::DEFAULT_OLED_METRIC_SWITCH_SECONDS;
    unsigned int medianSampleCount = ControlConfig::DEFAULT_MEDIAN_SAMPLE_COUNT;
    unsigned long medianSampleDelayMs = ControlConfig::DEFAULT_MEDIAN_SAMPLE_DELAY_MS;
    float adaptiveAlphaMin = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MIN;
    float adaptiveAlphaMax = ControlConfig::DEFAULT_ADAPTIVE_ALPHA_MAX;
    float adaptiveDeltaRefPsi = ControlConfig::DEFAULT_ADAPTIVE_DELTA_REF_PSI;
    float adaptiveJitterDeadbandPsi = ControlConfig::DEFAULT_ADAPTIVE_JITTER_DEADBAND_PSI;
    unsigned long tsIntervalSeconds = CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC;
    unsigned long bfIntervalMinutes = CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN;
    float offsetVoltage = SensorConfig::PRESSURE_OFFSET_DEFAULT;
    float tempOffset = 0.0f;
    bool useTempSensor = true;
    String tsApiKey;
    String bfStreamId;
    String bfDeviceName;
    int bfTempSource = 0;
    bool tsEnabled = false;
    bool bfEnabled = false;
    bool httpEnabled = false;
    String httpServer;
    String httpPath = "/";
    String httpBodyTemplate;
    unsigned long httpIntervalSeconds = CloudConfig::CUSTOM_HTTP_DEFAULT_INTERVAL_SEC;
    String devName;
    String ssid;
    String pass;
    String firmwareVersion;
    String firmwareBuildDate;
};

#endif // WEB_VIEW_MODELS_H
