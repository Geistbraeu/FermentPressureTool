#ifndef HTML_TEMPLATE_H
#define HTML_TEMPLATE_H

#include <Arduino.h>
#include <WiFi.h>

String getHtml(float p, float v, bool mOverride, bool mOn, unsigned long mStart, float maxPressureThreshold, float offsetVoltage) {
    String html = "<html><body>";
    html += "<h1>Pressure Sensor (" + String(HOSTNAME) + ")</h1>";
    html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Pressure: " + String(p, 2) + " PSI</p>";
    html += "<p>Valve: <span id='valveStatus'>" + String(mOverride ? (mOn ? "Manual Open" : "Manual Closed") : "Auto") + "</span></p>";
    
    long remaining = 0;
    if (mOverride) {
        remaining = 10000L - (long)(millis() - mStart);
        if (remaining < 0) remaining = 0;
    }

    html += "<p id='timer'>" + String(mOverride && remaining > 0 ? "Manual mode time remaining: " + String(remaining / 1000) + " s" : "") + "</p>";
    
    html += "<script>";
    if (mOverride && remaining > 0) {
        html += "let remaining = " + String(remaining) + ";";
        html += "const timerEl = document.getElementById('timer');";
        html += "const interval = setInterval(() => {";
        html += "  remaining -= 1000;";
        html += "  if (remaining <= 0) {";
        html += "    clearInterval(interval);";
        html += "    location.reload();";
        html += "  } else {";
        html += "    timerEl.innerText = 'Manual mode time remaining: ' + Math.floor(remaining / 1000) + ' s';";
        html += "  }";
        html += "}, 1000);";
    }
    html += "</script>";

    html += "<form action='/api' method='POST'>";
    html += "<input type='hidden' name='cmd' value='manual_on'>";
    html += "<input type='submit' value='Manual Open'></form>";
    html += "<form action='/api' method='POST'>";
    html += "<input type='hidden' name='cmd' value='manual_off'>";
    html += "<input type='submit' value='Manual Close/Auto'></form><br>";

    html += "<p>Max Pressure Threshold: " + String(maxPressureThreshold, 2) + " PSI</p>";
    html += "<form action='/api' method='POST'>";
    html += "New Max Pressure: <input type='number' step='0.1' name='pressure'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<p>Voltage Offset: " + String(offsetVoltage, 3) + " V</p>";
    html += "<form action='/api' method='POST'>";
    html += "New Voltage Offset: <input type='number' step='0.001' name='offset'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "</body></html>";
    return html;
}

#endif
