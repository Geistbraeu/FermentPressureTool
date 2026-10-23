#ifndef HTML_TEMPLATE_H
#define HTML_TEMPLATE_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "WebViewModels.h"

String getHtml(const RuntimeSnapshot& runtime, const SettingsSnapshot& cfg) {
  float pBar = runtime.pressure * SensorConfig::PSI_TO_BAR;

    String html = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Fermenter Control</title>
<style>
  :root {
    --bg: #0f1117;
    --surface: #1a1d27;
    --surface2: #22263a;
    --border: #2e3348;
    --accent: #6c8ef5;
    --accent2: #a78bfa;
    --ok: #34d399;
    --warn: #fbbf24;
    --danger: #f87171;
    --text: #e2e8f0;
    --text-muted: #8892a4;
    --radius: 12px;
    --radius-sm: 8px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    min-height: 100vh;
    padding: 16px;
  }
  .container { max-width: 640px; margin: 0 auto; }

  /* HEADER */
  .header {
    background: linear-gradient(135deg, #1e2340 0%, #16192d 100%);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 20px 24px;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    flex-wrap: wrap;
  }
  .header-left h1 {
    font-size: 1.3rem;
    font-weight: 700;
    background: linear-gradient(90deg, var(--accent), var(--accent2));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
    margin-bottom: 2px;
  }
  .header-left p { color: var(--text-muted); font-size: 0.8rem; }

  /* STATUS CARDS */
  .cards {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 10px;
    margin-bottom: 16px;
  }
  .card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    padding: 14px 16px;
  }
  .card-label { font-size: 0.7rem; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-muted); margin-bottom: 4px; }
  .card-value { font-size: 1.25rem; font-weight: 700; }
  .card-value.accent { color: var(--accent); }
  .card-value.ok { color: var(--ok); }
  .card-value.warn { color: var(--warn); }
  .card-value.danger { color: var(--danger); }

  /* VALVE CONTROLS */
  .controls {
    display: flex;
    gap: 10px;
    margin-bottom: 16px;
    flex-wrap: wrap;
  }
  .status-message {
    margin-bottom: 16px;
    padding: 10px 12px;
    border-radius: var(--radius-sm);
    border: 1px solid transparent;
    font-size: 0.88rem;
    font-weight: 600;
  }
  .status-message.success {
    background: rgba(52, 211, 153, 0.12);
    border-color: rgba(52, 211, 153, 0.35);
    color: #86efac;
  }
  .status-message.error {
    background: rgba(248, 113, 113, 0.12);
    border-color: rgba(248, 113, 113, 0.35);
    color: #fca5a5;
  }
  .btn {
    padding: 10px 20px;
    border: none;
    border-radius: var(--radius-sm);
    font-size: 0.9rem;
    font-weight: 600;
    cursor: pointer;
    transition: opacity 0.15s, transform 0.1s;
  }
  .btn:hover { opacity: 0.85; }
  .btn:active { transform: scale(0.97); }
  .btn-open {
    background: linear-gradient(135deg, #34d399, #059669);
    color: #fff;
  }
  .btn-close {
    background: linear-gradient(135deg, #f87171, #dc2626);
    color: #fff;
  }
  .oled-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    padding: 14px 16px;
    margin-bottom: 16px;
  }
  .oled-screen {
    display: block;
    width: 100%;
    max-width: 512px;
    height: auto;
    aspect-ratio: 2 / 1;
    margin: 10px auto 0;
    background: #000;
    border: 1px solid var(--border);
    image-rendering: pixelated;
  }
  .oled-status {
    color: var(--text-muted);
    font-size: 0.8rem;
    margin-top: 4px;
  }
  .ferminator-controls {
    display: grid;
    grid-template-columns: minmax(64px, 1fr) minmax(0, 2fr);
    gap: 10px;
    max-width: 512px;
    margin: 14px auto 0;
  }
  .ferminator-button {
    background: var(--surface2);
    color: var(--text);
    border: 1px solid var(--border);
    min-height: 44px;
  }
  .ferminator-button:disabled { opacity: 0.5; cursor: wait; }

  /* TABS */
  .tabs-nav {
    display: flex;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius) var(--radius) 0 0;
    overflow: hidden;
  }
  .tab-btn {
    flex: 1;
    padding: 13px 16px;
    background: transparent;
    border: none;
    color: var(--text-muted);
    font-size: 0.88rem;
    font-weight: 600;
    cursor: pointer;
    transition: background 0.2s, color 0.2s;
    border-bottom: 2px solid transparent;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
  }
  .tab-btn:hover { background: var(--surface2); color: var(--text); }
  .tab-btn.active {
    color: var(--accent);
    border-bottom-color: var(--accent);
    background: var(--surface2);
  }
  .tabs-body {
    background: var(--surface);
    border: 1px solid var(--border);
    border-top: none;
    border-radius: 0 0 var(--radius) var(--radius);
    padding: 20px;
    margin-bottom: 16px;
  }
  .tab-panel { display: none; }
  .tab-panel.active { display: block; }

  /* SETTINGS FORM */
  .setting-group { margin-bottom: 14px; }
  .setting-group:last-child { margin-bottom: 0; }
  .setting-label {
    display: block;
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: 0.07em;
    color: var(--text-muted);
    margin-bottom: 6px;
  }
  .setting-row {
    display: flex;
    gap: 8px;
    align-items: stretch;
  }
  .setting-row input,
  .setting-row select {
    flex: 1;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    color: var(--text);
    padding: 9px 12px;
    font-size: 0.9rem;
    outline: none;
    transition: border-color 0.2s;
  }
  .setting-row input:focus,
  .setting-row select:focus { border-color: var(--accent); }
  .setting-row select option { background: var(--surface); }
  .btn-set {
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    color: #fff;
    border: none;
    border-radius: var(--radius-sm);
    padding: 9px 18px;
    font-size: 0.85rem;
    font-weight: 600;
    cursor: pointer;
    white-space: nowrap;
    transition: opacity 0.15s;
  }
  .btn-set:hover { opacity: 0.85; }

  .divider {
    border: none;
    border-top: 1px solid var(--border);
    margin: 16px 0;
  }
  .section-title {
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--accent2);
    margin-bottom: 4px;
    font-weight: 700;
  }
  .cloud-last-sync {
    font-size: 0.72rem;
    color: var(--text-muted);
    margin-bottom: 12px;
  }

  /* TIMER */
  #timer {
    font-size: 0.82rem;
    color: var(--warn);
    min-height: 18px;
  }
</style>
</head>
<body>
<div class="container">

  <!-- HEADER -->
  <div class="header">
    <div class="header-left">
      <h1><span style="-webkit-text-fill-color:initial;">&#127866;</span> Fermenter Control</h1>
      <p><span id="device-name">)rawhtml";
    html += cfg.devName;
    html += R"rawhtml(</span> &nbsp;&#183;&nbsp; <span id="device-ip">)rawhtml";
    html += WiFi.localIP().toString();
    html += R"rawhtml(</span></p>
    </div>
  </div>

  <!-- STATUS CARDS -->
  <div class="cards">
    <div class="card">
      <div class="card-label">Pressure</div>
      <div class="card-value )rawhtml";
    html += runtime.isPressureSensorConnected ? "accent" : "danger";
    html += R"rawhtml(" id="pressure-psi">)rawhtml";
    if (runtime.isPressureSensorConnected) {
      html += String(runtime.pressure, 2) + " PSI";
    } else {
      html += "Disconnected";
    }
    html += R"rawhtml(</div>
    </div>
    <div class="card">
      <div class="card-label">Pressure</div>
      <div class="card-value )rawhtml";
    html += runtime.isPressureSensorConnected ? "accent" : "danger";
    html += R"rawhtml(" id="pressure-bar">)rawhtml";
    if (runtime.isPressureSensorConnected) {
      html += String(pBar, 2) + " Bar";
    } else {
      html += "Disconnected";
    }
    html += R"rawhtml(</div>
    </div>
    <div class="card">
      <div class="card-label">Voltage</div>
      <div class="card-value" id="voltage-v">)rawhtml";
    html += String(runtime.voltage, 3) + " V";
    html += R"rawhtml(</div>
    </div>
    )rawhtml";
    if (cfg.useTempSensor) {
    html += R"rawhtml(<div class="card" id="temp-card">
      <div class="card-label">Temperature</div>
      <div class="card-value )rawhtml";
    html += runtime.isTempSensorConnected ? "ok" : "danger";
    html += R"rawhtml(" id="temp-c">)rawhtml";
    if (runtime.isTempSensorConnected) {
      html += String(runtime.temperature, 2) + " °C";
    } else {
      html += "Disconnected";
    }
    html += R"rawhtml(</div>
    </div>)rawhtml";
    }
    html += R"rawhtml(
    <div class="card">
      <div class="card-label">Valve</div>
      <div class="card-value )rawhtml";
    if (!runtime.manualOverride) html += "ok\" id=\"valve-state\">Auto";
    else if (runtime.manualOn)   html += "warn\" id=\"valve-state\">Manual Open";
    else            html += "danger\" id=\"valve-state\">Manual Closed";
    html += R"rawhtml(</div>
    </div>
    <div class="card">
      <div class="card-label">Valve Activations / Hour</div>
      <div class="card-value" id="valve-activations-per-hour">)rawhtml";
    html += String(runtime.valveActivationsPerHour);
    html += R"rawhtml(</div>
    </div>
  </div>

)rawhtml";
    if (cfg.ferminatorEnabled) {
      html += R"rawhtml(  <section class="oled-card" aria-label="Ferminator OLED mirror" data-debug-info=")rawhtml";
      html += OledMirrorConfig::SHOW_DEBUG_INFO ? "true" : "false";
      html += R"rawhtml(">
    <div class="section-title">Ferminator OLED</div>
    <div id="oled-status" class="oled-status">Waiting for display data...</div>
    <div id="oled-timing" class="oled-status" hidden>Frame timing: waiting for two complete frames...</div>
    <canvas id="oled-screen" class="oled-screen" width="128" height="64"></canvas>
    <div class="ferminator-controls" role="group" aria-label="Ferminator buttons">
      <button class="btn ferminator-button" type="button" data-ferminator-button="temp_up" aria-label="Increase temperature">+</button>
      <button class="btn ferminator-button" type="button" data-ferminator-button="start_stop">Start/Stop</button>
      <button class="btn ferminator-button" type="button" data-ferminator-button="temp_down" aria-label="Decrease temperature">&minus;</button>
      <button class="btn ferminator-button" type="button" data-ferminator-button="fan_speed">Fan Speed</button>
    </div>
    <div id="ferminator-status" class="oled-status" role="status" aria-live="polite"></div>
  </section>
)rawhtml";
    }
    html += R"rawhtml(
  <!-- VALVE CONTROLS -->
  <div class="controls">
    <form action="/api" method="POST">
      <input type="hidden" name="cmd" value="manual_on">
      <button class="btn btn-open" type="submit">&#9654; Manual Open</button>
    </form>
    <form action="/api" method="POST">
      <input type="hidden" name="cmd" value="manual_off">
      <button class="btn btn-close" type="submit">&#9646; Close / Auto</button>
    </form>
  </div>
  <div id="timer"></div>
  <div id="status-message" class="status-message" style="display:none;"></div>
  <br>

  <!-- TABS -->
  <div class="tabs-nav">
    <button class="tab-btn active" type="button" onclick="switchTab('main', this)" id="tab-main">
      &#9881; Main Settings
    </button>
    <button class="tab-btn" type="button" onclick="switchTab('filter', this)" id="tab-filter">
      &#128295; Pressure Filter
    </button>
    <button class="tab-btn" type="button" onclick="switchTab('cloud', this)" id="tab-cloud">
      &#9729; Cloud Settings
    </button>
    <button class="tab-btn" type="button" onclick="switchTab('wifi', this)" id="tab-wifi">
      &#128246; WiFi
    </button>
    <button class="tab-btn" type="button" onclick="switchTab('firmware', this)" id="tab-firmware">
      &#128190; Firmware
    </button>
  </div>

  <div class="tabs-body">
    <!-- TAB: MAIN SETTINGS -->
    <div class="tab-panel active" id="panel-main">

      <div class="section-title">Pressure &amp; Valve</div>

      <div class="setting-group">
        <label class="setting-label">Ferminator Control</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="ferminatorEnabled">
              <option value="0")rawhtml";
    html += (!cfg.ferminatorEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (cfg.ferminatorEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
        <p class="cloud-last-sync">Enable only on the controller connected to the Ferminator. Changing this setting restarts the device.</p>
      </div>

      <div class="setting-group">
        <label class="setting-label">Max Pressure Threshold (PSI)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.1" name="pressure" value=")rawhtml";
    html += String(cfg.maxPressureThreshold, 1);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Device Name</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="devName" value=")rawhtml";
    html += cfg.devName;
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Pressure Unit</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="pUnit">
              <option value="0")rawhtml";
    html += (cfg.pressureUnit == 0 ? " selected" : "");
    html += R"rawhtml(>PSI</option>
              <option value="1")rawhtml";
    html += (cfg.pressureUnit == 1 ? " selected" : "");
    html += R"rawhtml(>Bar</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Hysteresis (PSI)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.1" name="hysteresis" value=")rawhtml";
    html += String(cfg.hysteresis, 1);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <hr class="divider">
      <div class="section-title">Sensor &amp; Calibration</div>

      <div class="setting-group">
        <label class="setting-label">Update Interval (ms)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" name="updateInterval" value=")rawhtml";
    html += String(cfg.updateIntervalMs);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">OLED Switch Interval (s)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" min="1" max="60" name="oledSwapSec" value=")rawhtml";
    html += String(cfg.oledMetricSwitchSeconds);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Pressure ADC</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="pressureAdc">
              <option value="0")rawhtml";
    html += (cfg.pressureAdc == SensorConfig::PRESSURE_ADC_ESP32 ? " selected" : "");
    html += R"rawhtml(>ESP32</option>
              <option value="1")rawhtml";
    html += (cfg.pressureAdc == SensorConfig::PRESSURE_ADC_ADS1115 ? " selected" : "");
    html += R"rawhtml(>ADS1115</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Sensor Max Pressure (PSI)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" min="0" step="any" name="sensorMaxPsi" required value=")rawhtml";
    html += String(cfg.sensorMaxPsi, 6);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Voltage Offset (V)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.001" name="offset" value=")rawhtml";
    html += String(cfg.offsetVoltage, 3);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Temperature Offset (°C)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.1" name="tempOffset" value=")rawhtml";
    html += String(cfg.tempOffset, 1);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Temperature Sensor</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="useTemp">
              <option value="0")rawhtml";
    html += (!cfg.useTempSensor ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (cfg.useTempSensor ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

    </div><!-- /panel-main -->

    <!-- TAB: PRESSURE FILTER -->
    <div class="tab-panel" id="panel-filter">

      <div class="section-title">Median Filter</div>

      <div class="setting-group">
        <label class="setting-label">Median Sample Count (odd, 3-31)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" name="medianSampleCount" value=")rawhtml";
    html += String(cfg.medianSampleCount);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Median Sample Delay (ms)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" min="1" name="medianSampleDelay" value=")rawhtml";
    html += String(cfg.medianSampleDelayMs);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <hr class="divider">
      <div class="section-title">Adaptive Filter (ignored when valve is open)</div>

      <div class="setting-group">
        <label class="setting-label">Adaptive Filter</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="adaptiveFilter">
              <option value="0")rawhtml";
    html += (!cfg.adaptiveFilterEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (cfg.adaptiveFilterEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Alpha Min (0..1)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.01" min="0.01" max="0.99" name="pfAlphaMin" value=")rawhtml";
    html += String(cfg.adaptiveAlphaMin, 2);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Alpha Max (0..1)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.01" min="0.01" max="1.00" name="pfAlphaMax" value=")rawhtml";
    html += String(cfg.adaptiveAlphaMax, 2);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Delta Ref (PSI, max speed point)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.01" min="0.01" max="10.00" name="pfDeltaRef" value=")rawhtml";
    html += String(cfg.adaptiveDeltaRefPsi, 2);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Jitter Deadband (PSI)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.01" min="0.00" max="2.00" name="pfDeadband" value=")rawhtml";
    html += String(cfg.adaptiveJitterDeadbandPsi, 2);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

    </div><!-- /panel-filter -->

    <!-- TAB: CLOUD SETTINGS -->
    <div class="tab-panel" id="panel-cloud">

      <div class="section-title">ThingSpeak</div>
      <p class="cloud-last-sync" id="ts-last-sync">Last sync: —</p>

      <form action="/api" method="POST">
        <div class="setting-group">
          <label class="setting-label">Logging Enabled</label>
          <div class="setting-row">
            <select name="tsEnabled">
              <option value="0")rawhtml";
    html += (!cfg.tsEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (cfg.tsEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">API Key</label>
          <div class="setting-row">
            <input type="text" name="tsApiKey" value=")rawhtml";
    html += cfg.tsApiKey;
    html += R"rawhtml(" placeholder="ThingSpeak Write API Key">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Send Interval (seconds, min 15)</label>
          <div class="setting-row">
            <input type="number" name="tsInterval" value=")rawhtml";
    html += String(cfg.tsIntervalSeconds);
    html += R"rawhtml(">
          </div>
        </div>

        <div class="setting-group">
          <div class="setting-row">
            <button class="btn-set" type="submit">Save ThingSpeak</button>
          </div>
        </div>
      </form>

      <hr class="divider">
      <div class="section-title">Brewfather</div>
      <p class="cloud-last-sync" id="bf-last-sync">Last sync: —</p>

      <form action="/api" method="POST">
        <div class="setting-group">
          <label class="setting-label">Logging Enabled</label>
          <div class="setting-row">
            <select name="bfEnabled">
              <option value="0")rawhtml";
    html += (!cfg.bfEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (cfg.bfEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Stream ID</label>
          <div class="setting-row">
            <input type="text" name="bfStreamId" value=")rawhtml";
    html += cfg.bfStreamId;
    html += R"rawhtml(" placeholder="Brewfather Stream ID">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Device Name in Brewfather</label>
          <div class="setting-row">
            <input type="text" name="bfDeviceName" value=")rawhtml";
    html += cfg.bfDeviceName;
    html += R"rawhtml(" placeholder="e.g. Pressure_Sensor">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Temperature Field</label>
          <div class="setting-row">
            <select name="bfTempSource">
              <option value="0")rawhtml";
    html += (cfg.bfTempSource == 0 ? " selected" : "");
    html += R"rawhtml(>Fermenter (temp)</option>
              <option value="1")rawhtml";
    html += (cfg.bfTempSource == 1 ? " selected" : "");
    html += R"rawhtml(>Room (ext_temp)</option>
              <option value="2")rawhtml";
    html += (cfg.bfTempSource == 2 ? " selected" : "");
    html += R"rawhtml(>Fridge (aux_temp)</option>
            </select>
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Send Interval (minutes, min 15)</label>
          <div class="setting-row">
            <input type="number" name="bfInterval" value=")rawhtml";
    html += String(cfg.bfIntervalMinutes);
    html += R"rawhtml(">
          </div>
        </div>

        <div class="setting-group">
          <div class="setting-row">
            <button class="btn-set" type="submit">Save Brewfather</button>
          </div>
        </div>
      </form>

      <hr class="divider">
      <div class="section-title">Custom HTTP POST</div>
      <p class="cloud-last-sync" id="http-last-sync">Last sync: —</p>

      <form action="/api" method="POST">
        <div class="setting-group">
          <label class="setting-label">HTTP POST Enabled</label>
          <div class="setting-row">
            <select name="httpEnabled">
              <option value="0")rawhtml";
    html += (!cfg.httpEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (cfg.httpEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Server Address (http://... or https://...)</label>
          <div class="setting-row">
            <input type="text" name="httpServer" value=")rawhtml";
    html += cfg.httpServer;
    html += R"rawhtml(" placeholder="e.g. http://192.168.1.100:8080">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">HTTP Path (supports {volt}, {psi}, {bar}, {temp}, {valve_activations_per_hour})</label>
          <div class="setting-row">
            <input type="text" name="httpPath" value=")rawhtml";
    html += cfg.httpPath;
    html += R"rawhtml(" placeholder="e.g. /api/data?volt={volt}&psi={psi}">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">JSON Body Template (optional, supports {volt}, {psi}, {bar}, {temp}, {valve_activations_per_hour})</label>
          <div class="setting-row">
            <input type="text" name="httpBodyTemplate" value=")rawhtml";
    html += cfg.httpBodyTemplate;
    html += R"rawhtml(" placeholder="e.g. {\"voltage\":{volt},\"pressure\":{psi}}">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">Send Interval (seconds, min 15)</label>
          <div class="setting-row">
            <input type="number" name="httpInterval" value=")rawhtml";
    html += String(cfg.httpIntervalSeconds);
    html += R"rawhtml(">
          </div>
        </div>

        <div class="setting-group">
          <div class="setting-row">
            <button class="btn-set" type="submit">Save HTTP</button>
          </div>
        </div>
      </form>

    </div><!-- /panel-cloud -->

    <!-- TAB: WIFI SETTINGS -->
    <div class="tab-panel" id="panel-wifi">
      <div class="section-title">WiFi Connection</div>
      <form action="/api" method="POST">
        <div class="setting-group">
          <label class="setting-label">WiFi SSID</label>
          <div class="setting-row">
            <input type="text" name="ssid" value=")rawhtml";
    html += cfg.ssid;
    html += R"rawhtml(" placeholder="Network name">
          </div>
        </div>

        <div class="setting-group">
          <label class="setting-label">WiFi Password</label>
          <div class="setting-row">
            <input type="password" name="pass" value=")rawhtml";
    html += cfg.pass;
    html += R"rawhtml(" placeholder="Network password">
          </div>
        </div>

        <div class="setting-group">
          <div class="setting-row">
            <button class="btn-set" type="submit">Save WiFi</button>
          </div>
        </div>
      </form>
    </div><!-- /panel-wifi -->

    <!-- TAB: FIRMWARE -->
    <div class="tab-panel" id="panel-firmware">
      <div class="section-title">Installed Firmware</div>

      <div class="setting-group">
        <label class="setting-label">Version</label>
        <div class="setting-row">
          <input type="text" id="fw-version" readonly value=")rawhtml";
    html += cfg.firmwareVersion;
    html += R"rawhtml(">
        </div>
      </div>

      <div class="setting-group">
        <label class="setting-label">Build Date</label>
        <div class="setting-row">
          <input type="text" id="fw-build-date" readonly value=")rawhtml";
    html += cfg.firmwareBuildDate;
    html += R"rawhtml(">
        </div>
      </div>

      <p class="cloud-last-sync" id="fw-build-label">)rawhtml";
    html += cfg.firmwareVersion;
    html += " from ";
    html += cfg.firmwareBuildDate;
    html += R"rawhtml(</p>

      <hr class="divider">
      <div class="section-title">OTA Update</div>
      <form action="/update" method="POST" enctype="multipart/form-data" id="firmware-update-form">
        <div class="setting-group">
          <label class="setting-label">Firmware Binary (.bin)</label>
          <div class="setting-row">
            <input type="file" name="firmware" accept=".bin,application/octet-stream" required>
          </div>
        </div>

        <div class="setting-group">
          <div class="setting-row">
            <button class="btn-set" type="submit">Update</button>
          </div>
        </div>
      </form>

      <p class="cloud-last-sync">After successful upload, device will reboot automatically.</p>
    </div><!-- /panel-firmware -->
  </div><!-- /tabs-body -->

  <div style="margin-top:16px; text-align:center; font-size:0.85rem;">
    <a href="https://github.com/Geistbraeu/FermentPressureTool" target="_blank" rel="noopener noreferrer" style="color:#67e8f9; font-weight:700; text-decoration:none;">GitHub: FermentPressureTool</a>
  </div>

</div><!-- /container -->

<script>
function switchTab(name, btn) {
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('panel-' + name).classList.add('active');
  btn.classList.add('active');
}

const pressurePsiEl = document.getElementById('pressure-psi');
const pressureBarEl = document.getElementById('pressure-bar');
const voltageEl = document.getElementById('voltage-v');
const tempCardEl = document.getElementById('temp-card');
const tempValueEl = document.getElementById('temp-c');
const valveEl = document.getElementById('valve-state');
const valveActivationsPerHourEl = document.getElementById('valve-activations-per-hour');
const timerEl = document.getElementById('timer');
const deviceNameEl = document.getElementById('device-name');
const statusEl = document.getElementById('status-message');
const firmwareVersionEl = document.getElementById('fw-version');
const firmwareBuildDateEl = document.getElementById('fw-build-date');
const firmwareBuildLabelEl = document.getElementById('fw-build-label');
const oledCanvas = document.getElementById('oled-screen');
const oledStatusEl = document.getElementById('oled-status');
const oledTimingEl = document.getElementById('oled-timing');
const oledDebugInfoEnabled = Boolean(oledCanvas &&
  oledCanvas.closest('.oled-card').dataset.debugInfo === 'true');
if (oledTimingEl) oledTimingEl.hidden = !oledDebugInfoEnabled;
const oledContext = oledCanvas ? oledCanvas.getContext('2d') : null;
let lastOledSequence = -1;
let liveDataRefreshInProgress = false;
let deviceRestartPending = false;

function showStatus(message, kind) {
  if (!statusEl) return;

  statusEl.textContent = message;
  statusEl.className = 'status-message ' + kind;
  statusEl.style.display = 'block';
}

function clearStatus() {
  if (!statusEl) return;

  statusEl.textContent = '';
  statusEl.className = 'status-message';
  statusEl.style.display = 'none';
}

function setValveState(manualOverride, manualOn) {
  if (!valveEl) return;

  if (!manualOverride) {
    valveEl.className = 'card-value ok';
    valveEl.textContent = 'Auto';
    return;
  }

  if (manualOn) {
    valveEl.className = 'card-value warn';
    valveEl.textContent = 'Manual Open';
  } else {
    valveEl.className = 'card-value danger';
    valveEl.textContent = 'Manual Closed';
  }
}

function setManualTimer(manualOverride, remainingMs) {
  if (!timerEl) return;

  if (!manualOverride || !Number.isFinite(remainingMs) || remainingMs < 0) {
    timerEl.textContent = '';
    return;
  }

  timerEl.textContent = 'Manual mode: ' + Math.floor(remainingMs / 1000) + 's remaining';
}

function setFirmwareInfo(version, buildDate) {
  const resolvedVersion = (typeof version === 'string' && version.length > 0) ? version : 'Unknown';
  const resolvedBuildDate = (typeof buildDate === 'string' && buildDate.length > 0) ? buildDate : 'Unknown';

  if (firmwareVersionEl) {
    firmwareVersionEl.value = resolvedVersion;
  }
  if (firmwareBuildDateEl) {
    firmwareBuildDateEl.value = resolvedBuildDate;
  }
  if (firmwareBuildLabelEl) {
    firmwareBuildLabelEl.textContent = resolvedVersion + ' from ' + resolvedBuildDate;
  }
}

let ferminatorButtonPending = false;

async function pressFerminatorButton(button) {
  if (ferminatorButtonPending || deviceRestartPending) return;
  ferminatorButtonPending = true;
  const buttons = document.querySelectorAll('[data-ferminator-button]');
  const status = document.getElementById('ferminator-status');
  buttons.forEach(control => { control.disabled = true; });
  status.textContent = 'Sending button press...';
  try {
    const response = await fetch('/ferminator/button', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
      body: new URLSearchParams({ button }).toString(),
      cache: 'no-store',
      signal: AbortSignal.timeout(5000)
    });
    const data = await response.json();
    if (!response.ok || data.success !== true) {
      throw new Error(data.message || 'Button press failed.');
    }
    status.textContent = 'Button pulse sent. Check the Ferminator display for the result.';
  } catch (error) {
    status.textContent = 'Unable to confirm button press: ' + error.message +
      ' Check the display before trying again.';
  } finally {
    await new Promise(resolve => setTimeout(resolve, 250));
    buttons.forEach(control => { control.disabled = false; });
    ferminatorButtonPending = false;
  }
}

document.querySelectorAll('[data-ferminator-button]').forEach(control => {
  control.addEventListener('click', () => pressFerminatorButton(control.dataset.ferminatorButton));
});

function renderOledFrame(frame, data) {
  if (!oledContext || frame.length !== 1024) return;

  oledContext.fillStyle = '#000';
  oledContext.fillRect(0, 0, 128, 64);
  const known = Number(data.oledKnownState) || 0;
  // Unobserved settings mean a raw RAM preview, not an observed display-off.
  const enabled = !(known & 1) || Boolean(data.oledDisplayEnabled);
  const allOn = (known & 64) !== 0 && Boolean(data.oledDisplayAllOn);
  const inverse = (known & 32) !== 0 && Boolean(data.oledInverse);
  const segmentRemap = !(known & 2) || Boolean(data.oledSegmentRemap);
  const comScanReverse = !(known & 4) || Boolean(data.oledComScanReverse);
  const startLine = (known & 8) ? Number(data.oledStartLine) || 0 : 0;
  const displayOffset = (known & 16) ? Number(data.oledDisplayOffset) || 0 : 0;

  if (!enabled) return;

  oledContext.fillStyle = '#fff';
  for (let page = 0; page < 8; page++) {
    for (let column = 0; column < 128; column++) {
      const value = frame[page * 128 + column];
      for (let bit = 0; bit < 8; bit++) {
        const lit = allOn || (((value & (1 << bit)) !== 0) !== inverse);
        if (!lit) continue;

        // The module is mounted for normal RAM order with A1/C8.
        const x = segmentRemap ? column : 127 - column;
        let y = (page * 8 + bit - startLine + displayOffset + 64) % 64;
        if (!comScanReverse) y = 63 - y;
        oledContext.fillRect(x, y, 1, 1);
      }
    }
  }
}

function renderOledTiming(timing) {
  if (!oledTimingEl || !oledDebugInfoEnabled) return;
  if (!timing || !Number.isInteger(timing.frames) ||
      timing.frames < 0 || !Number.isInteger(timing.intervals) ||
      timing.intervals !== Math.max(0, timing.frames - 1) ||
      !Number.isFinite(timing.observedMs) || timing.observedMs < 0 ||
      !Array.isArray(timing.buckets) || timing.buckets.length !== 6 ||
      timing.buckets.some(count => !Number.isInteger(count) || count < 0) ||
      timing.buckets.reduce((sum, count) => sum + count, 0) !== timing.intervals ||
      (timing.intervals > 0 &&
        [timing.lastIntervalMs, timing.minIntervalMs, timing.meanIntervalMs,
         timing.maxIntervalMs, timing.currentGapMs]
          .some(ms => !Number.isFinite(ms) || ms < 0))) {
    oledTimingEl.textContent = 'Frame timing statistics unavailable.';
    return;
  }
  const elapsed = Math.floor(timing.observedMs / 1000);
  const duration = Math.floor(elapsed / 60) + 'm ' + elapsed % 60 + 's';
  const seconds = ms => ms === null ? 'n/a' : (ms / 1000).toFixed(3) + 's';
  const summary = 'Good frames: ' + timing.frames + ' · observed: ' + duration;
  if (timing.intervals === 0) {
    oledTimingEl.textContent = summary + ' · waiting for two complete frames';
    return;
  }
  oledTimingEl.textContent = summary +
    ' · gaps last/min/avg/max: ' +
    [timing.lastIntervalMs, timing.minIntervalMs,
     timing.meanIntervalMs, timing.maxIntervalMs].map(seconds).join(' / ') +
    ' · current wait: ' + seconds(timing.currentGapMs) +
    ' · interval counts [<=1.5s / 1.5-3s / 3-5s / 5-10s / 10-30s / >30s]: ' +
    timing.buckets.join(' / ');
}

async function refreshOledMirror(data) {
  if (!oledCanvas || !data.ferminatorEnabled) return;
  renderOledTiming(data.oledFrameTiming);
  if (!oledStatusEl) return;

  if (!Boolean(data.oledReady)) {
    if (!oledDebugInfoEnabled) {
      oledStatusEl.textContent = 'Waiting for a complete display frame...';
      return;
    }
    oledStatusEl.textContent = 'Waiting for a complete display frame (' +
      Number(data.oledCapturedBytes) + '/1024 bytes)...' +
      (Number(data.oledCaptureErrors) > 0
        ? ' · capture synchronization errors: ' + Number(data.oledCaptureErrors) : '') +
      (Number(data.oledOverflows) > 0
        ? ' · capture buffer overflows: ' + Number(data.oledOverflows) : '');
    return;
  }

  const sequence = Number(data.oledSequence);
  let knownState = Number(data.oledKnownState) || 0;
  if (sequence !== lastOledSequence) {
    try {
      const response = await fetch('/oled', { cache: 'no-store' });
      if (!response.ok) {
        oledStatusEl.textContent = 'Unable to read the captured OLED frame.';
        return;
      }
      const frame = new Uint8Array(await response.arrayBuffer());
      if (frame.length !== 1024) {
        oledStatusEl.textContent = 'Invalid captured OLED frame size.';
        return;
      }
      const flags = Number(response.headers.get('X-OLED-Flags'));
      const frameSequence = Number(response.headers.get('X-OLED-Sequence'));
      const startLine = Number(response.headers.get('X-OLED-Start-Line'));
      const displayOffset = Number(response.headers.get('X-OLED-Display-Offset'));
      knownState = Number(response.headers.get('X-OLED-Known-State'));
      if (!response.headers.has('X-OLED-Flags') ||
          !response.headers.has('X-OLED-Sequence') ||
          !response.headers.has('X-OLED-Start-Line') ||
          !response.headers.has('X-OLED-Display-Offset') ||
          !response.headers.has('X-OLED-Known-State') ||
          !Number.isInteger(flags) || flags < 0 || flags > 31 ||
          !Number.isInteger(frameSequence) || frameSequence < 0 ||
          !Number.isInteger(startLine) || startLine < 0 || startLine > 63 ||
          !Number.isInteger(displayOffset) || displayOffset < 0 || displayOffset > 63 ||
          !Number.isInteger(knownState) || knownState < 0 || knownState > 127) {
        oledStatusEl.textContent = 'Invalid captured OLED frame metadata.';
        return;
      }
      renderOledFrame(frame, {
        oledDisplayEnabled: (flags & 1) !== 0,
        oledDisplayAllOn: (flags & 2) !== 0,
        oledInverse: (flags & 4) !== 0,
        oledSegmentRemap: (flags & 8) !== 0,
        oledComScanReverse: (flags & 16) !== 0,
        oledStartLine: startLine,
        oledDisplayOffset: displayOffset,
        oledKnownState: knownState
      });
      lastOledSequence = frameSequence;
    } catch (error) {
      oledStatusEl.textContent = 'Network error while reading the OLED frame.';
      return;
    }
  }

  oledStatusEl.textContent = Boolean(data.oledRecovering)
    ? 'Holding last complete frame · reacquiring'
    : knownState === 127
      ? 'Mirroring active'
      : 'RAM preview (display initialization not fully captured)';
  if (!oledDebugInfoEnabled) return;

  const nowMs = Number(data.nowMs) || 0;
  const updatedMs = Number(data.oledLastUpdateMs) || 0;
  const ageMs = (nowMs - updatedMs + 4294967296) % 4294967296;
  const overflowCount = Number(data.oledOverflows) || 0;
  const captureErrors = Number(data.oledCaptureErrors) || 0;
  const droppedFrames = Number(data.oledDroppedFrames) || 0;
  oledStatusEl.textContent += (Boolean(data.oledRecovering)
    ? ' (' + Number(data.oledCapturedBytes) + '/1024 bytes)' : '') +
    ' · last display data ' +
    Math.floor(ageMs / 1000) + 's ago' +
    (overflowCount > 0 ? ' · capture buffer overflows: ' + overflowCount : '') +
    (captureErrors > 0 ? ' · capture synchronization errors: ' + captureErrors : '') +
    (droppedFrames > 0 ? ' · rejected refreshes: ' + droppedFrames : '');
}

async function refreshLiveData() {
  if (liveDataRefreshInProgress || deviceRestartPending) return;
  liveDataRefreshInProgress = true;
  try {
    const response = await fetch('/api', { cache: 'no-store' });
    if (!response.ok) return;

    const data = await response.json();
    await refreshOledMirror(data);
    const pressure = Number(data.pressure);
    const voltage = Number(data.voltage);
    const pressureConnected = Boolean(data.pressureConnected);

    if (pressureConnected && Number.isFinite(pressure)) {
      pressurePsiEl.className = 'card-value accent';
      pressureBarEl.className = 'card-value accent';
      pressurePsiEl.textContent = pressure.toFixed(2) + ' PSI';
      pressureBarEl.textContent = (pressure * 0.0689476).toFixed(2) + ' Bar';
    } else {
      pressurePsiEl.className = 'card-value danger';
      pressureBarEl.className = 'card-value danger';
      pressurePsiEl.textContent = 'Disconnected';
      pressureBarEl.textContent = 'Disconnected';
    }

    if (Number.isFinite(voltage)) {
      voltageEl.textContent = voltage.toFixed(3) + ' V';
    }

    if (tempCardEl) {
      const temperatureEnabled = Boolean(data.useTempSensor);
      tempCardEl.style.display = temperatureEnabled ? '' : 'none';

      if (temperatureEnabled && tempValueEl) {
        const temperature = Number(data.temperature);
        if (Boolean(data.tempConnected) && Number.isFinite(temperature)) {
          tempValueEl.className = 'card-value ok';
          tempValueEl.textContent = temperature.toFixed(2) + ' °C';
        } else {
          tempValueEl.className = 'card-value danger';
          tempValueEl.textContent = 'Disconnected';
        }
      }
    }

    if (deviceNameEl && data.devName) {
      deviceNameEl.textContent = data.devName;
    }

    if (valveActivationsPerHourEl) {
      const valveActivationsPerHour = Number(data.valveActivationsPerHour);
      if (Number.isFinite(valveActivationsPerHour)) {
        valveActivationsPerHourEl.textContent = String(Math.max(0, Math.floor(valveActivationsPerHour)));
      }
    }

    setValveState(Boolean(data.manualOverride), Boolean(data.manualOn));
    setManualTimer(Boolean(data.manualOverride), Number(data.remainingTime));

    if (data.nowMs !== undefined) {
      function fmtSync(nowMs, syncMs) {
        if (!syncMs) return '—';
        const s = Math.floor((nowMs - syncMs) / 1000);
        if (s < 60) return s + 's ago';
        if (s < 3600) return Math.floor(s / 60) + 'min ago';
        return Math.floor(s / 3600) + 'h ago';
      }
      const nowMs = Number(data.nowMs);
      const tsEl   = document.getElementById('ts-last-sync');
      const bfEl   = document.getElementById('bf-last-sync');
      const httpEl = document.getElementById('http-last-sync');
      if (tsEl)   tsEl.textContent   = 'Last sync: ' + fmtSync(nowMs, Number(data.lastTsSyncMs));
      if (bfEl)   bfEl.textContent   = 'Last sync: ' + fmtSync(nowMs, Number(data.lastBfSyncMs));
      if (httpEl) httpEl.textContent = 'Last sync: ' + fmtSync(nowMs, Number(data.lastHttpSyncMs));
    }

    setFirmwareInfo(data.firmwareVersion, data.firmwareBuildDate);
  } catch (e) {
    // Ignore transient network or parsing failures; next poll will retry.
  } finally {
    liveDataRefreshInProgress = false;
  }
}

async function reloadAfterRestart() {
  deviceRestartPending = true;
  showStatus('Data saved. Device restarting...', 'success');
  await new Promise(resolve => setTimeout(resolve, 2000));
  for (let attempt = 0; attempt < 30; attempt++) {
    try {
      const response = await fetch('/api', {
        cache: 'no-store', signal: AbortSignal.timeout(2000)
      });
      if (response.ok) {
        window.location.reload();
        return;
      }
    } catch (error) {
      // Connection loss is expected while the device restarts.
    }
    await new Promise(resolve => setTimeout(resolve, 1000));
  }
  deviceRestartPending = false;
  showStatus('Device has not reconnected. Check its connection and reload the page.', 'error');
}

async function submitSettingsForm(event) {
  event.preventDefault();

  const form = event.currentTarget;
  const body = new URLSearchParams(new FormData(form));

  try {
    const response = await fetch(form.action || '/api', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
      body: body.toString(),
      cache: 'no-store'
    });

    const responseText = await response.text();
    let data = null;

    try {
      data = JSON.parse(responseText);
    } catch (parseError) {
      showStatus('Unexpected response from device.', 'error');
      return;
    }

    if (data && data.success) {
      if (data.rebooting) {
        await reloadAfterRestart();
        return;
      }
      const cmd = body.get('cmd');
      showStatus(cmd ? 'Manual control updated.' : 'Data saved.', 'success');
      await refreshLiveData();
      return;
    }

    const errors = data && Array.isArray(data.errors) ? data.errors : [];
    if (errors.length > 0) {
      const firstError = errors[0];
      showStatus(firstError.field + ': ' + firstError.message, 'error');
    } else {
      showStatus('Save failed.', 'error');
    }
  } catch (networkError) {
    showStatus('Network error while saving.', 'error');
  }
}

document.querySelectorAll('form[action="/api"][method="POST"]').forEach(form => {
  form.addEventListener('submit', submitSettingsForm);
});

async function submitFirmwareForm(event) {
  event.preventDefault();

  const form = event.currentTarget;
  const fileInput = form.querySelector('input[type="file"]');
  if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
    showStatus('Select firmware binary file first.', 'error');
    return;
  }

  const body = new FormData(form);
  showStatus('Uploading firmware. Please wait...', 'success');

  try {
    const response = await fetch(form.action || '/update', {
      method: 'POST',
      body,
      cache: 'no-store'
    });

    const responseText = await response.text();
    let data = null;
    try {
      data = JSON.parse(responseText);
    } catch (parseError) {
      showStatus('Unexpected response during firmware update.', 'error');
      return;
    }

    if (response.ok && data && data.success) {
      showStatus(data.message || 'Firmware uploaded. Device rebooting...', 'success');
      return;
    }

    const message = data && data.message ? data.message : 'Firmware update failed.';
    showStatus(message, 'error');
  } catch (networkError) {
    showStatus('Network error during firmware upload.', 'error');
  }
}

const firmwareForm = document.getElementById('firmware-update-form');
if (firmwareForm) {
  firmwareForm.addEventListener('submit', submitFirmwareForm);
}

refreshLiveData();
setInterval(refreshLiveData, 1000);
)rawhtml";

    html += R"rawhtml(</script>
</body>
</html>)rawhtml";

    return html;
}

#endif // HTML_TEMPLATE_H
