#include "ConfigPortal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_SSD1306.h>
#include "Settings.h"

extern Adafruit_SSD1306 display;
extern bool isOledConnected;

static WebServer portalServer(80);
static DNSServer dnsServer;

static void handleRoot() {
    Serial.println("[Portal] Setup page requested");
    String html = "<html><body>"
                  "<h1>Fermenter Setup</h1>"
                  "<form action='/save' method='POST'>"
                  "Device Name: <input type='text' name='devName'><br>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='pass'><br>"
                  "<input type='submit' value='Save'>"
                  "</form><br>"
                  "<a href='https://github.com/Geistbraeu/FermentPressureTool' target='_blank' rel='noopener noreferrer' style='color:#0057ff;font-weight:700;'>"
                  "GitHub: FermentPressureTool"
                  "</a></body></html>";
    portalServer.send(200, "text/html", html);
}

static void handleSave() {
    Serial.println("[Portal] Settings submission received");
    if (portalServer.hasArg("ssid") && portalServer.hasArg("pass") && portalServer.hasArg("devName")) {
        wifiSettings.save(
            portalServer.arg("ssid"),
            portalServer.arg("pass"),
            portalServer.arg("devName")
        );

        Serial.println("[Portal] Wi-Fi settings saved; restarting");
        portalServer.send(200, "text/html", "Settings saved. Restarting...");
        delay(1000);
        ESP.restart();
    } else {
        Serial.println("[Portal] ERROR: required settings are missing");
        portalServer.send(400, "text/plain", "Invalid arguments");
    }
}

bool ConfigPortal::connect() {
    Serial.println("[Wi-Fi] Loading saved network settings");
    wifiSettings.load();

    if (wifiSettings.ssid == "") {
        Serial.println("[Wi-Fi] No saved SSID; starting setup portal");
        return false;
    }

    WiFi.disconnect();
    WiFi.setHostname(wifiSettings.devName.c_str());
    WiFi.begin(wifiSettings.ssid.c_str(), wifiSettings.pass.c_str());
    Serial.println("[Wi-Fi] Connecting to saved network");
    
    // Wait up to 10 seconds for connection
    for (int i = 0; i < 20; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[Wi-Fi] Подключено: " + String(WiFi.localIP().toString()));
            return true;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[Wi-Fi] Connection timed out; starting setup portal");
    return false;
}

bool ConfigPortal::begin() {
    if (!connect()) {
        startSetupMode();
        return false;
    }
    return true;
}

void ConfigPortal::startSetupMode() {
    WiFi.mode(WIFI_AP);
    for (;;) {
        Serial.println("[Portal] Starting open access point: Ferment_Setup");
        if (WiFi.softAP("Ferment_Setup")) {
            break;
        }
        Serial.println("[Portal] ERROR: access point start failed; retrying in 5 seconds");
        delay(5000);
    }

    const IPAddress portalIp = WiFi.softAPIP();
    Serial.println("[Portal] Access point active; connect and open http://" +
                   portalIp.toString() + "/");
    if (!dnsServer.start(53, "*", portalIp)) {
        Serial.println("[Portal] ERROR: captive DNS server start failed");
    } else {
        Serial.println("[Portal] Captive DNS server active");
    }

    portalServer.on("/", handleRoot);
    portalServer.on("/save", handleSave);
    portalServer.onNotFound(handleRoot);
    portalServer.begin();
    Serial.println("[Portal] HTTP setup page active");

    if (isOledConnected) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("AP Mode: Ferment_Setup");
        display.print("IP: ");
        display.println(WiFi.softAPIP());
        display.println("Connect to config");
        display.display();
    }

    unsigned long lastHeartbeat = millis();
    for (;;) {
        dnsServer.processNextRequest();
        portalServer.handleClient();
        const unsigned long now = millis();
        if (now - lastHeartbeat >= 10000) {
            Serial.printf("[Portal] Waiting for setup client; connected stations: %u\n",
                          static_cast<unsigned>(WiFi.softAPgetStationNum()));
            lastHeartbeat = now;
        }
        delay(10);
    }
}
