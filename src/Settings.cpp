#include "Settings.h"
#include <Preferences.h>

Settings settings;
WifiSettings wifiSettings;

void Settings::load() {
    Preferences prefs;
    prefs.begin("config", true);
    maxPressureThreshold = prefs.getFloat("maxPressure", 14.0);
    pressureUnit = prefs.getInt("pUnit", 0);
    hysteresis = prefs.getFloat("hysteresis", 0.5);
    sensorInterval = prefs.getULong("sInterval", 200);
    tsIntervalSeconds = prefs.getULong("tsInterval", 120);
    bfIntervalMinutes = prefs.getULong("bfInterval", 15);
    offsetVoltage = prefs.getFloat("offsetVoltage", 0.515);
    useTempSensor = prefs.getBool("useTemp", true);
    prefs.end();
}

void Settings::setMaxPressureThreshold(float val) {
    maxPressureThreshold = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putFloat("maxPressure", val);
    prefs.end();
}

void Settings::setPressureUnit(int val) {
    pressureUnit = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putInt("pUnit", val);
    prefs.end();
}

void Settings::setHysteresis(float val) {
    hysteresis = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putFloat("hysteresis", val);
    prefs.end();
}

void Settings::setSensorInterval(unsigned long val) {
    sensorInterval = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putULong("sInterval", val);
    prefs.end();
}

void Settings::setTsIntervalSeconds(unsigned long val) {
    tsIntervalSeconds = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putULong("tsInterval", val);
    prefs.end();
}

void Settings::setBfIntervalMinutes(unsigned long val) {
    bfIntervalMinutes = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putULong("bfInterval", val);
    prefs.end();
}

void Settings::setOffsetVoltage(float val) {
    offsetVoltage = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putFloat("offsetVoltage", val);
    prefs.end();
}

void Settings::setUseTempSensor(bool val) {
    useTempSensor = val;
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putBool("useTemp", val);
    prefs.end();
}

void WifiSettings::load() {
    Preferences prefs;
    prefs.begin("wifi_conf", true);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    devName = prefs.getString("devName", "ferment01");
    prefs.end();
}

void WifiSettings::save(const String& newSsid, const String& newPass, const String& newDevName) {
    ssid = newSsid;
    pass = newPass;
    devName = newDevName;
    Preferences prefs;
    prefs.begin("wifi_conf", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("devName", devName);
    prefs.end();
}
