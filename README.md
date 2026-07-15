🌐 **English** | [Русский](README.ru.md)

# FermentPressureTool

A device for monitoring and controlling the fermentation process with real-time pressure and temperature measurement.

## 📋 Project Overview

**FermentPressureTool** is an ESP32-based IoT device designed for precise fermentation condition control. It reads pressure and temperature inside a fermentation vessel, uploads data to cloud services, and controls a pressure relief valve.

<p align="center">
   <img src="device.jpg" alt="FermentPressureTool device" width="560">
</p>

### Key Features

- 📊 **Pressure monitoring** — analog pressure sensor reading via ADC
- 🌡️ **Temperature measurement** — DS18B20 sensor support over OneWire
- 📡 **Cloud integration** — data upload to:
  - **Brewfather** (homebrewing platform)
  - **ThingSpeak** (IoT platform)
  - **Custom HTTP API** (flexible integration)
- 🖥️ **Web interface** — browser-based control and monitoring
- 🔧 **Configuration portal** — WiFi setup without reflashing
- 📱 **OLED display** — alternating pressure and temperature display (SSD1306, 128×64)
- 🤖 **Multithreading** — parallel execution of sensor and network operations
- 💾 **Persistent settings** — stored in Preferences/NVS

## 🛠️ Hardware

### Microcontroller
- **ESP32-DevKit** — WiFi-enabled dual-core microcontroller

### Sensors and Modules
- **Pressure sensor** — analog, connected to `GPIO34` (ADC1_CH6)
- **Temperature sensor** — DS18B20 (OneWire)
  - DATA: `GPIO4`
  - 4.7 kΩ pull-up resistor required between DATA and VCC
- **OLED display** — SSD1306 128×64 (I2C, address 0x3C)
- **Solenoid valve** — controlled via `GPIO27`

### Connections
- Serial (UART) at 115200 baud for debugging
- CH340 USB-UART converter for programming

## 📁 Project Structure

```
FermentPressureTool/
├── src/
│   ├── main.cpp                 # Entry point, initialization
│   ├── config.h                 # Central constants (pins, parameters)
│   ├── Settings.h/cpp           # Persistent settings management
│   ├── CloudManager.h/cpp       # Cloud provider coordination
│   ├── ConfigPortal.h/cpp       # WiFi configuration portal
│   ├── web_server.h/cpp         # REST API and web interface
│   ├── RuntimeState.h           # Shared state between cores
│   ├── debug.h                  # Debug macros
│   ├── html_template.h          # Web UI HTML
│   ├── cloud/
│   │   ├── CloudProvider.h      # Base provider interface
│   │   ├── BrewfatherProvider.h/cpp
│   │   ├── ThingSpeakProvider.h/cpp
│   │   └── CustomHTTPProvider.h/cpp
│   └── device/
│       ├── DisplayManager.h/cpp      # OLED display management
│       ├── SensorManager.h/cpp       # Sensor reading (pressure, temperature)
│       └── SolenoidController.h/cpp  # Valve control
├── include/                     # User libraries
├── lib/                         # External libraries
├── test/                        # Tests
├── platformio.ini               # PlatformIO configuration
└── README.md                    # This file
```

## 🔌 Software Components

### CloudManager
Coordinates data upload to cloud platforms. Supports multiple simultaneous providers.

### ConfigPortal
WiFi configuration portal for initial setup and reconfiguration without reflashing.

### SensorManager
- ADC initialization and eFuse-based calibration
- Filtered pressure reading (average of N samples)
- DS18B20 temperature reading over OneWire

### DisplayManager
Manages the SSD1306 OLED display, alternating between pressure and temperature. The switch interval is configurable via the web interface.

### WebServer
REST API for:
- Retrieving current sensor data
- Controlling the valve
- Changing system parameters
- Triggering cloud uploads

### SolenoidController
Controls the solenoid valve for pressure relief.

## 🚀 Getting Started

### Requirements
- **PlatformIO** — embedded build system
- **VS Code** with the PlatformIO extension
- **CH340 driver** — for USB programming

### Dependencies
Declared in `platformio.ini`:
```
adafruit/Adafruit SSD1306 @ ^2.5.9
adafruit/Adafruit GFX Library @ ^1.11.9
adafruit/Adafruit BusIO @ ^1.14.5
paulstoffregen/OneWire @ ^2.3.8
milesburton/DallasTemperature @ ^4.0.4
```

### Build and Flash

1. **Open the project in VS Code**
   ```bash
   code d:\Projects\FermentPressureTool
   ```

2. **Connect the ESP32 via USB**

3. **Select the environment in PlatformIO**
   - Choose `esp32s` from the available environments

4. **Build the project**
   ```bash
   pio run -e esp32s
   ```

5. **Upload the firmware**
   ```bash
   pio run -e esp32s --target upload
   ```

6. **Open the serial monitor**
   ```bash
   pio device monitor -e esp32s
   ```

## ⚙️ Configuration

### Build Flags
Debug flags defined in `platformio.ini`:
- `APP_DEBUG_SERIAL=0` — debug level (0 = disabled)

### Hardware Pins (`src/config.h`)
All pins are defined centrally in `config.h`. Edit the values before building as needed.

### Network Parameters
- **Hostname**: `ferment01` (accessible as `ferment01.local`)
- **Web server port**: 80
- **UART baud rate**: 115200

### Cloud Integrations
Configure API keys and settings via the configuration portal or web interface:
- **Brewfather API Key**
- **ThingSpeak Channel ID and API Key**
- **Custom HTTP URL endpoint**

### OLED and Sensors
- Pressure and temperature are shown alternately on the OLED.
- The switch interval is configurable in the web interface; default is 3 seconds.
- If the temperature sensor is disconnected, only pressure is shown.
- If the pressure sensor is not connected, the display shows `Disconnected` and cloud uploads are skipped.

## 🌐 Cloud Integration

### Brewfather
Integration with the popular homebrewing platform. Sends pressure and temperature data.

### ThingSpeak
IoT platform for data storage and visualization. Supports measurement history and charts.

### Custom HTTP API
Flexible integration with any HTTP API to send data to a custom backend.

## 📊 REST API

### Get Sensor Data
```
GET /api
```

Response:
```json
{
  "pressure": 1.5,
  "temperature": 22.5,
  "voltage": 2.3,
  "pressureConnected": true,
  "tempConnected": true
}
```

### Control the Valve
```
POST /api
Body: cmd=manual_on
```

### Cloud Upload
Cloud uploads are performed automatically in the firmware's main loop and are skipped if the pressure sensor is disconnected.

## 🔄 Multithreading Architecture

The system uses two tasks running on separate ESP32 cores:
- **Core 1**: Sensor operations (reading sensors, valve control)
- **Core 0**: Network operations (WiFi, cloud requests, web server)

Synchronization is handled through the `RuntimeState` structure using semaphores.

## 🐛 Debugging

Debug output is sent to UART when enabled:

```cpp
DBG("Debug message here");
```

View the output in the serial monitor.

---

**Version**: 1.2.0  
**Last updated**: 2026-07-15
