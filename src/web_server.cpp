#include <WebServer.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "web_server.h"
#include "html_template.h"

extern float currentVoltage;
extern float currentPressure;
extern SemaphoreHandle_t dataMutex;
extern float maxPressureThreshold;
extern float offsetVoltage;
extern bool manualOverride;
extern bool manualOn;
extern unsigned long manualStartTime;

WebServer server(80);

void handleRoot() {
    float p = 0.0, v = 0.0;
    bool mOverride = false, mOn = false;
    unsigned long mStart = 0;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        p = currentPressure;
        v = currentVoltage;
        mOverride = manualOverride;
        mOn = manualOn;
        mStart = manualStartTime;
        xSemaphoreGive(dataMutex);
    }
    
    server.send(200, "text/html", getHtml(p, v, mOverride, mOn, mStart, maxPressureThreshold, offsetVoltage));
}

void handleApi() {
    if (server.method() == HTTP_GET) {
        float p = 0.0, v = 0.0;
        bool mOverride = false, mOn = false;
        unsigned long mStart = 0;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            p = currentPressure;
            v = currentVoltage;
            mOverride = manualOverride;
            mOn = manualOn;
            mStart = manualStartTime;
            xSemaphoreGive(dataMutex);
        }
        long remaining = -1;
        if (mOverride) {
            remaining = 10000L - (long)(millis() - mStart);
            if (remaining < 0) remaining = 0;
        }
        
        String json = "{\"pressure\":" + String(p, 2) + 
                      ",\"voltage\":" + String(v, 2) + 
                      ",\"maxPressure\":" + String(maxPressureThreshold, 2) + 
                      ",\"offsetVoltage\":" + String(offsetVoltage, 3) + 
                      ",\"manualOverride\":" + (mOverride ? "true" : "false") +
                      ",\"manualOn\":" + (mOn ? "true" : "false") +
                      ",\"remainingTime\":" + String(remaining) + "}";
        server.send(200, "application/json", json);
    } else if (server.method() == HTTP_POST) {
        if (server.hasArg("cmd")) {
            String cmd = server.arg("cmd");
            if (cmd == "manual_on") {
                manualOverride = true;
                manualOn = true;
                manualStartTime = millis();
            } else if (cmd == "manual_off") {
                manualOverride = false;
            }
        }
        if (server.hasArg("pressure")) {
            maxPressureThreshold = server.arg("pressure").toFloat();
            Preferences prefs;
            prefs.begin("config", false);
            prefs.putFloat("maxPressure", maxPressureThreshold);
            prefs.end();
        }
        if (server.hasArg("offset")) {
            offsetVoltage = server.arg("offset").toFloat();
            Preferences prefs;
            prefs.begin("config", false);
            prefs.putFloat("offsetVoltage", offsetVoltage);
            prefs.end();
        }
        server.sendHeader("Location", "/", true);
        server.send(303, "text/plain", "OK");
    }
}

void webServerTask(void *pvParameters) {
    server.on("/", handleRoot);
    server.on("/api", handleApi);
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
