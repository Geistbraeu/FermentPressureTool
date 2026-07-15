#include <WebServer.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <cstring>
#include "config.h"
#include "validation.h"
#include "web_server.h"
#include "html_template.h"
#include "WebViewModels.h"
#include "Settings.h"
#include "RuntimeState.h"
#include "CloudManager.h"

extern RuntimeState runtimeState;

WebServer server(80);

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE __DATE__
#endif

static bool firmwareUpdateHasError = false;
static String firmwareUpdateErrorMessage;

static String monthToNumber(const char* month) {
    if (strcmp(month, "Jan") == 0) return "01";
    if (strcmp(month, "Feb") == 0) return "02";
    if (strcmp(month, "Mar") == 0) return "03";
    if (strcmp(month, "Apr") == 0) return "04";
    if (strcmp(month, "May") == 0) return "05";
    if (strcmp(month, "Jun") == 0) return "06";
    if (strcmp(month, "Jul") == 0) return "07";
    if (strcmp(month, "Aug") == 0) return "08";
    if (strcmp(month, "Sep") == 0) return "09";
    if (strcmp(month, "Oct") == 0) return "10";
    if (strcmp(month, "Nov") == 0) return "11";
    if (strcmp(month, "Dec") == 0) return "12";
    return "00";
}

static String formatBuildDateDdMmYyyy(const char* buildDateRaw) {
    if (buildDateRaw == nullptr) {
        return "Unknown";
    }

    // Expected input format: "Mmm dd yyyy" from __DATE__
    if (strlen(buildDateRaw) < 11) {
        return String(buildDateRaw);
    }

    char month[4] = { buildDateRaw[0], buildDateRaw[1], buildDateRaw[2], '\0' };
    String monthNumber = monthToNumber(month);

    char day[3] = { buildDateRaw[4], buildDateRaw[5], '\0' };
    if (day[0] == ' ') {
        day[0] = '0';
    }

    char year[5] = { buildDateRaw[7], buildDateRaw[8], buildDateRaw[9], buildDateRaw[10], '\0' };
    return String(day) + "." + monthNumber + "." + String(year);
}

static String escapeJsonString(const String& value) {
    String escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return escaped;
}

static String getFirmwareVersion() {
    return String(APP_VERSION);
}

static String getFirmwareBuildDate() {
    return formatBuildDateDdMmYyyy(APP_BUILD_DATE);
}

static String updateErrorToMessage() {
    return "Update error code: " + String(Update.getError());
}

static RuntimeSnapshot readRuntimeSnapshot() {
    RuntimeSnapshot snapshot;
    if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
        snapshot.pressure = runtimeState.currentPressure;
        snapshot.voltage = runtimeState.currentVoltage;
        snapshot.temperature = runtimeState.currentTemp;
        snapshot.valveActivationsPerHour = runtimeState.valveActivationsPerHour;
        snapshot.manualOverride = runtimeState.manualOverride;
        snapshot.manualOn = runtimeState.manualOn;
        snapshot.manualStartTime = runtimeState.manualStartTime;
        snapshot.isTempSensorConnected = runtimeState.isTempSensorConnected;
        snapshot.isPressureSensorConnected = runtimeState.isPressureSensorConnected;
        xSemaphoreGive(runtimeState.dataMutex);
    }
    return snapshot;
}

static SettingsSnapshot readSettingsSnapshot() {
    SettingsSnapshot snapshot;
    snapshot.devName = wifiSettings.devName;
    snapshot.ssid = wifiSettings.ssid;
    snapshot.pass = wifiSettings.pass;

    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
        snapshot.maxPressureThreshold = settings.maxPressureThreshold;
        snapshot.pressureUnit = settings.pressureUnit;
        snapshot.hysteresis = settings.hysteresis;
        snapshot.updateIntervalMs = settings.updateIntervalMs;
        snapshot.oledMetricSwitchSeconds = settings.oledMetricSwitchSeconds;
        snapshot.medianSampleCount = settings.medianSampleCount;
        snapshot.medianSampleDelayMs = settings.medianSampleDelayMs;
        snapshot.adaptiveAlphaMin = settings.adaptiveAlphaMin;
        snapshot.adaptiveAlphaMax = settings.adaptiveAlphaMax;
        snapshot.adaptiveDeltaRefPsi = settings.adaptiveDeltaRefPsi;
        snapshot.adaptiveJitterDeadbandPsi = settings.adaptiveJitterDeadbandPsi;
        snapshot.tsIntervalSeconds = settings.tsIntervalSeconds;
        snapshot.bfIntervalMinutes = settings.bfIntervalMinutes;
        snapshot.offsetVoltage = settings.offsetVoltage;
        snapshot.tempOffset = settings.tempOffset;
        snapshot.useTempSensor = settings.useTempSensor;
        snapshot.tsApiKey = settings.tsApiKey;
        snapshot.bfStreamId = settings.bfStreamId;
        snapshot.bfDeviceName = settings.bfDeviceName;
        snapshot.bfTempSource = settings.bfTempSource;
        snapshot.tsEnabled = settings.tsEnabled;
        snapshot.bfEnabled = settings.bfEnabled;
        snapshot.httpEnabled = settings.httpEnabled;
        snapshot.httpServer = settings.httpServer;
        snapshot.httpPath = settings.httpPath;
        snapshot.httpBodyTemplate = settings.httpBodyTemplate;
        snapshot.httpIntervalSeconds = settings.httpIntervalSeconds;
        snapshot.devName = wifiSettings.devName;
        snapshot.ssid = wifiSettings.ssid;
        snapshot.pass = wifiSettings.pass;
        xSemaphoreGive(runtimeState.settingsMutex);
    }

    snapshot.firmwareVersion = getFirmwareVersion();
    snapshot.firmwareBuildDate = getFirmwareBuildDate();

    return snapshot;
}

void handleRoot() {
    RuntimeSnapshot runtime = readRuntimeSnapshot();
    SettingsSnapshot cfg = readSettingsSnapshot();
    
    server.send(200, "text/html", getHtml(runtime, cfg));
}

void handleApi() {
    if (server.method() == HTTP_GET) {
        RuntimeSnapshot runtime = readRuntimeSnapshot();
        SettingsSnapshot cfg = readSettingsSnapshot();

        long remaining = -1;
        if (runtime.manualOverride) {
            remaining = 10000L - (long)(millis() - runtime.manualStartTime);
            if (remaining < 0) remaining = 0;
        }

        String json = "{\"pressure\":" + String(runtime.pressure, 2) + 
                       ",\"voltage\":" + String(runtime.voltage, 2) + 
                       ",\"temperature\":" + String(runtime.temperature, 2) +
                       ",\"valveActivationsPerHour\":" + String(runtime.valveActivationsPerHour) +
                       ",\"tempConnected\":" + (runtime.isTempSensorConnected ? "true" : "false") +
                       ",\"pressureConnected\":" + (runtime.isPressureSensorConnected ? "true" : "false") +
                       ",\"maxPressure\":" + String(cfg.maxPressureThreshold, 2) + 
                       ",\"pressureUnit\":" + String(cfg.pressureUnit) +
                       ",\"oledSwapSec\":" + String(cfg.oledMetricSwitchSeconds) +
                       ",\"offsetVoltage\":" + String(cfg.offsetVoltage, 3) + 
                       ",\"useTempSensor\":" + (cfg.useTempSensor ? "true" : "false") +
                       ",\"devName\":\"" + cfg.devName + "\"" +
                       ",\"manualOverride\":" + (runtime.manualOverride ? "true" : "false") +
                       ",\"manualOn\":" + (runtime.manualOn ? "true" : "false") +
                       ",\"remainingTime\":" + String(remaining) +
                       ",\"nowMs\":" + String(millis()) +
                       ",\"lastTsSyncMs\":" + String(cloudLastTsSyncMs()) +
                       ",\"lastBfSyncMs\":" + String(cloudLastBfSyncMs()) +
                       ",\"lastHttpSyncMs\":" + String(cloudLastHttpSyncMs()) +
                       ",\"firmwareVersion\":\"" + escapeJsonString(cfg.firmwareVersion) + "\"" +
                       ",\"firmwareBuildDate\":\"" + escapeJsonString(cfg.firmwareBuildDate) + "\"}";
        server.send(200, "application/json", json);
    } else if (server.method() == HTTP_POST) {
        String errorsJson = "";
        bool hasErrors = false;
        bool wifiChanged = false;

        auto addError = [&](const String& field, const String& message) {
            if (hasErrors) {
                errorsJson += ",";
            }
            errorsJson += "{\"field\":\"" + field + "\",\"message\":\"" + message + "\"}";
            hasErrors = true;
        };

        auto saveFailed = [&](const char* fieldName) {
            addError(fieldName, "save_failed");
        };

        auto lockFailed = [&](const char* lockName) {
            addError(lockName, "mutex_timeout");
        };
        
        if (server.hasArg("cmd")) {
            String cmd = Validation::trim(server.arg("cmd"));
            if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (cmd == "manual_on") {
                    runtimeState.manualOverride = true;
                    runtimeState.manualOn = true;
                    runtimeState.manualStartTime = millis();
                } else if (cmd == "manual_off") {
                    runtimeState.manualOverride = false;
                }
                xSemaphoreGive(runtimeState.dataMutex);
            } else {
                lockFailed("dataMutex");
            }
        }
        
        if (server.hasArg("pressure")) {
            float val = server.arg("pressure").toFloat();
            if (!Validation::isValidPressure(val)) {
                addError("pressure", "invalid_range_0_5_34_0");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setMaxPressureThreshold(val)) saveFailed("pressure");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("devName")) {
            String newDevName = Validation::trim(server.arg("devName"));
            if (!Validation::isValidHostname(newDevName)) {
                addError("devName", "invalid_hostname");
            } else {
                wifiSettings.save(wifiSettings.ssid, wifiSettings.pass, newDevName);
                WiFi.setHostname(newDevName.c_str());
            }
        }

        if (server.hasArg("ssid") || server.hasArg("pass")) {
            String newSsid = server.hasArg("ssid") ? Validation::trim(server.arg("ssid")) : wifiSettings.ssid;
            String newPass = server.hasArg("pass") ? Validation::trim(server.arg("pass")) : wifiSettings.pass;

            if (newSsid.isEmpty()) {
                addError("ssid", "invalid_ssid");
            } else {
                wifiChanged = (newSsid != wifiSettings.ssid) || (newPass != wifiSettings.pass);
                wifiSettings.save(newSsid, newPass, wifiSettings.devName);
            }
        }
        
        if (server.hasArg("pUnit")) {
            int val = server.arg("pUnit").toInt();
            if (!Validation::isValidPressureUnit(val)) {
                addError("pUnit", "must_be_0_or_1");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setPressureUnit(val)) saveFailed("pUnit");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("hysteresis")) {
            float val = server.arg("hysteresis").toFloat();
            if (!Validation::isValidHysteresis(val)) {
                addError("hysteresis", "invalid_range_0_2_0");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHysteresis(val)) saveFailed("hysteresis");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("updateInterval")) {
            unsigned long val = server.arg("updateInterval").toInt();
            if (!Validation::isValidUpdateInterval(val)) {
                addError("updateInterval", "invalid_range_50_5000");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setUpdateIntervalMs(val)) saveFailed("updateInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }

        if (server.hasArg("oledSwapSec")) {
            unsigned long val = server.arg("oledSwapSec").toInt();
            if (!Validation::isValidOledMetricSwitchSeconds(val)) {
                addError("oledSwapSec", "invalid_range_1_60");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setOledMetricSwitchSeconds(val)) saveFailed("oledSwapSec");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("medianSampleCount")) {
            unsigned int val = server.arg("medianSampleCount").toInt();
            if (!Validation::isValidMedianSampleCount(val)) {
                addError("medianSampleCount", "must_be_odd_3_31");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setMedianSampleCount(val)) saveFailed("medianSampleCount");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("medianSampleDelay")) {
            unsigned long val = server.arg("medianSampleDelay").toInt();
            if (!Validation::isValidMedianSampleDelay(val)) {
                addError("medianSampleDelay", "invalid_range_1_1000");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setMedianSampleDelayMs(val)) saveFailed("medianSampleDelay");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }

        if (server.hasArg("pfAlphaMin")) {
            float val = server.arg("pfAlphaMin").toFloat();
            if (!Validation::isValidAdaptiveAlphaMin(val)) {
                addError("pfAlphaMin", "invalid_range_0_1");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setAdaptiveAlphaMin(val)) saveFailed("pfAlphaMin");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }

        if (server.hasArg("pfAlphaMax")) {
            float val = server.arg("pfAlphaMax").toFloat();
            if (!Validation::isValidAdaptiveAlphaMax(val)) {
                addError("pfAlphaMax", "invalid_range_0_1");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setAdaptiveAlphaMax(val)) saveFailed("pfAlphaMax");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }

        if (server.hasArg("pfDeltaRef")) {
            float val = server.arg("pfDeltaRef").toFloat();
            if (!Validation::isValidAdaptiveDeltaRefPsi(val)) {
                addError("pfDeltaRef", "invalid_range_0_01_10");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setAdaptiveDeltaRefPsi(val)) saveFailed("pfDeltaRef");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }

        if (server.hasArg("pfDeadband")) {
            float val = server.arg("pfDeadband").toFloat();
            if (!Validation::isValidAdaptiveJitterDeadbandPsi(val)) {
                addError("pfDeadband", "invalid_range_0_2");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setAdaptiveJitterDeadbandPsi(val)) saveFailed("pfDeadband");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("tsInterval")) {
            unsigned long val = server.arg("tsInterval").toInt();
            if (!Validation::isValidTsInterval(val)) {
                addError("tsInterval", "invalid_range_15_3600");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setTsIntervalSeconds(val)) saveFailed("tsInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("bfInterval")) {
            unsigned long val = server.arg("bfInterval").toInt();
            if (!Validation::isValidBfInterval(val)) {
                addError("bfInterval", "invalid_range_15_1440");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfIntervalMinutes(val)) saveFailed("bfInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("offset")) {
            float val = server.arg("offset").toFloat();
            if (!Validation::isValidOffsetVoltage(val)) {
                addError("offset", "invalid_range_0_4_5");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setOffsetVoltage(val)) saveFailed("offset");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("tempOffset")) {
            float val = server.arg("tempOffset").toFloat();
            if (!Validation::isValidTempOffset(val)) {
                addError("tempOffset", "invalid_range_minus50_plus50");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setTempOffset(val)) saveFailed("tempOffset");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("useTemp")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setUseTempSensor(server.arg("useTemp").toInt() == 1)) saveFailed("useTemp");
                xSemaphoreGive(runtimeState.settingsMutex);
            } else {
                lockFailed("settingsMutex");
            }
        }
        
        if (server.hasArg("tsApiKey")) {
            String val = Validation::trim(server.arg("tsApiKey"));
            if (!Validation::isValidApiKey(val)) {
                addError("tsApiKey", "invalid_api_key");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setTsApiKey(val)) saveFailed("tsApiKey");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("bfStreamId")) {
            String val = Validation::trim(server.arg("bfStreamId"));
            if (!Validation::isValidApiKey(val)) {
                addError("bfStreamId", "invalid_stream_id");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfStreamId(val)) saveFailed("bfStreamId");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("bfDeviceName")) {
            String val = Validation::trim(server.arg("bfDeviceName"));
            if (!Validation::isValidDeviceName(val)) {
                addError("bfDeviceName", "invalid_device_name");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfDeviceName(val)) saveFailed("bfDeviceName");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }

        if (server.hasArg("bfTempSource")) {
            int val = server.arg("bfTempSource").toInt();
            if (!Validation::isValidBfTempSource(val)) {
                addError("bfTempSource", "must_be_0_or_1");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfTempSource(val)) saveFailed("bfTempSource");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("tsEnabled")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setTsEnabled(server.arg("tsEnabled").toInt() == 1)) saveFailed("tsEnabled");
                xSemaphoreGive(runtimeState.settingsMutex);
            } else {
                lockFailed("settingsMutex");
            }
        }
        
        if (server.hasArg("bfEnabled")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setBfEnabled(server.arg("bfEnabled").toInt() == 1)) saveFailed("bfEnabled");
                xSemaphoreGive(runtimeState.settingsMutex);
            } else {
                lockFailed("settingsMutex");
            }
        }
        
        if (server.hasArg("httpEnabled")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setHttpEnabled(server.arg("httpEnabled").toInt() == 1)) saveFailed("httpEnabled");
                xSemaphoreGive(runtimeState.settingsMutex);
            } else {
                lockFailed("settingsMutex");
            }
        }
        
        if (server.hasArg("httpServer")) {
            String val = Validation::trim(server.arg("httpServer"));
            if (!Validation::isValidHttpServer(val)) {
                addError("httpServer", "invalid_http_server");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpServer(val)) saveFailed("httpServer");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("httpPath")) {
            String val = Validation::trim(server.arg("httpPath"));
            if (!Validation::isValidHttpPath(val)) {
                addError("httpPath", "invalid_http_path");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpPath(val)) saveFailed("httpPath");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("httpBodyTemplate")) {
            String val = server.arg("httpBodyTemplate");
            if (!Validation::isValidHttpBodyTemplate(val)) {
                addError("httpBodyTemplate", "invalid_http_body_template");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpBodyTemplate(val)) saveFailed("httpBodyTemplate");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (server.hasArg("httpInterval")) {
            unsigned long val = server.arg("httpInterval").toInt();
            if (!Validation::isValidCustomHttpInterval(val)) {
                addError("httpInterval", "invalid_range_15_3600");
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpIntervalSeconds(val)) saveFailed("httpInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                } else {
                    lockFailed("settingsMutex");
                }
            }
        }
        
        if (hasErrors) {
            String response = "{\"success\":false,\"errors\":[" + errorsJson + "]}";
            server.send(400, "application/json", response);
        } else {
            server.send(200, "application/json", "{\"success\":true}");
            if (wifiChanged) {
                delay(200);
                ESP.restart();
            }
        }
    }
}

void handleFirmwareUpdateUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        firmwareUpdateHasError = false;
        firmwareUpdateErrorMessage = "";

        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            firmwareUpdateHasError = true;
            firmwareUpdateErrorMessage = updateErrorToMessage();
        }
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE) {
        if (firmwareUpdateHasError) {
            return;
        }

        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            firmwareUpdateHasError = true;
            firmwareUpdateErrorMessage = updateErrorToMessage();
        }
        return;
    }

    if (upload.status == UPLOAD_FILE_END) {
        if (!firmwareUpdateHasError && !Update.end(true)) {
            firmwareUpdateHasError = true;
            firmwareUpdateErrorMessage = updateErrorToMessage();
        }
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED) {
        firmwareUpdateHasError = true;
        firmwareUpdateErrorMessage = "Upload aborted";
        Update.abort();
    }
}

void handleFirmwareUpdateResult() {
    if (firmwareUpdateHasError) {
        String json = "{\"success\":false,\"message\":\"" + escapeJsonString(firmwareUpdateErrorMessage) + "\"}";
        server.send(400, "application/json", json);
        return;
    }

    server.send(200, "application/json", "{\"success\":true,\"message\":\"Firmware uploaded. Rebooting...\"}");
    delay(250);
    ESP.restart();
}

void webServerTask(void *pvParameters) {
    server.on("/", handleRoot);
    server.on("/api", handleApi);
    server.on("/update", HTTP_POST, handleFirmwareUpdateResult, handleFirmwareUpdateUpload);
    server.begin();
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initWebServer() {
    xTaskCreatePinnedToCore(
        webServerTask,
        "WebServerTask",
        4096,
        NULL,
        1,
        NULL,
        0 // Core 0
    );
}
