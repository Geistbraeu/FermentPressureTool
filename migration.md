# Migration Plan: ESP32 DevKit V1 to ESP32-S3 Super Mini

## Scope

Move the current ferment-pressure controller firmware from ESP32 DevKit V1 to the pictured ESP32-S3 Super Mini while preserving all existing behavior:

- pressure sensing and filtering;
- DS18B20 temperature sensing;
- SSD1306 OLED output;
- solenoid control;
- configuration portal;
- Wi-Fi, mDNS, and web server;
- cloud uploads;
- settings persistence;
- two-core FreeRTOS scheduling.

The selected board photo has GPIO48 and is consistent with ESP32-S3 Super Mini. The seller description contains inconsistent C3-like specifications, so confirm that the physical module and boot log identify `ESP32-S3` before flashing.

## Target Pin Map

| Function | ESP32 DevKit V1 | ESP32-S3 Super Mini |
| --- | ---: | ---: |
| Pressure sensor ADC | GPIO34 | GPIO1 |
| DS18B20 OneWire | GPIO4 | GPIO2 |
| OLED SDA | implicit GPIO21 | GPIO4 |
| OLED SCL | implicit GPIO22 | GPIO5 |
| Solenoid driver control | GPIO27 | GPIO6 |

The following pins remain unused during this migration:

- GPIO0 and GPIO3: avoid due to ESP32-S3 boot/strapping sensitivity.
- GPIO19 and GPIO20: reserve for native USB.
- GPIO48: normally connected to the onboard RGB LED.

External-device controls, four optocoupler outputs, the passive external I2C sniffer, SSD1306 capture/decoding, and web rendering of the foreign display are out of scope for this migration. They will be planned separately.

## Implementation Steps

1. Add a dedicated ESP32-S3 PlatformIO environment in [platformio.ini](platformio.ini) using `esp32-s3-devkitc-1`.
   - Keep the existing Arduino framework, libraries, pre-build script, build flags, monitor speed, and initial upload-speed setting.
   - Do not remove the existing `esp32dev` environment until the S3 build and hardware validation pass.
   - Configure flash or PSRAM options only after the received board's chip marking or boot log verifies its hardware configuration.

2. Update the hardware constants in [src/config.h](src/config.h).
   - Change pressure ADC from GPIO34 to GPIO1.
   - Change DS18B20 OneWire from GPIO4 to GPIO2.
   - Change solenoid driver control from GPIO27 to GPIO6.
   - Add named OLED I2C constants: GPIO4 for SDA and GPIO5 for SCL.
   - Retain `SensorConfig::ADC_VOLTAGE_DIVIDER` and all pressure conversion/filter constants.

3. Update [src/device/DisplayManager.cpp](src/device/DisplayManager.cpp).
   - Call `Wire.begin(DisplayConfig::OLED_I2C_SDA_PIN, DisplayConfig::OLED_I2C_SCL_PIN)` before `display.begin(...)`.
   - This removes the dependency on the ESP32 DevKit default I2C pins GPIO21 and GPIO22.

4. Preserve the present FreeRTOS topology.
   - Keep `SensorTask` on Core 1 and `NetworkTask` on Core 0 through `TaskConfig::SENSOR_TASK_CORE` and `TaskConfig::NETWORK_TASK_CORE` in [src/config.h](src/config.h).
   - ESP32-S3 is dual-core, so no scheduling or mutex redesign is required.
   - Update only stale board-specific comments in [src/main.cpp](src/main.cpp), if needed.

5. Build the S3 environment.

   ```powershell
   & "C:\Users\GhostDemon\.platformio\penv\Scripts\platformio.exe" run -e <s3-environment>
   ```

   Fix only S3-specific compiler or framework incompatibilities found by this build. Do not change unrelated application behavior.

6. Validate hardware before connecting the valve.
   - Measure the pressure-divider output at maximum expected pressure.
   - The existing divider maps the sensor's stated maximum $4.5\text{ V}$ output to approximately $3.09\text{ V}$, below the ESP32-S3 limit of $3.3\text{ V}$, but this must be physically confirmed.
   - Confirm the relay or MOSFET module accepts 3.3 V control logic and cannot feed a 5 V pull-up back into GPIO6.

7. Flash via USB.
   - If automatic bootloader entry does not work, hold BOOT while attaching USB, then release it after the board is detected.
   - Confirm the serial startup output identifies ESP32-S3.
   - Confirm the actual flash and PSRAM configuration before adding any board-specific memory settings.

8. Run a safe functional smoke test.
   - OLED on GPIO4/GPIO5 at address `0x3C`.
   - DS18B20 discovery and valid readings on GPIO2 with a 3.3 V pull-up.
   - Stable pressure readings on GPIO1.
   - Default-closed solenoid state plus automatic and manual valve control through GPIO6.
   - Wi-Fi reconnection, mDNS, web UI, configuration persistence, and all enabled cloud sends.

## Relevant Source Files

- [platformio.ini](platformio.ini): add/select the ESP32-S3 build environment while retaining the existing target during transition.
- [src/config.h](src/config.h): hardware pin constants, sensor divider calibration, and FreeRTOS core assignments.
- [src/device/DisplayManager.cpp](src/device/DisplayManager.cpp): explicit I2C initialization on the new OLED pins.
- [src/main.cpp](src/main.cpp): task creation and board-specific comments; scheduling behavior remains unchanged.
- [src/device/SensorManager.cpp](src/device/SensorManager.cpp): reuse the centralized pin lookup, ADC attenuation/calibration, and OneWire implementation without behavior changes.
- [src/device/SolenoidController.cpp](src/device/SolenoidController.cpp): reuse the centralized pin lookup and safe LOW/closed initialization without behavior changes.

## Acceptance Checks

1. The received board/chip and serial boot log identify `ESP32-S3`. Use a C3-specific migration only if this check fails.
2. Diagnostics contain no new errors in changed sources or PlatformIO configuration.
3. The ESP32-S3 PlatformIO environment builds successfully.
4. Pressure ADC voltage is never higher than 3.3 V, and GPIO6 receives no 5 V feedback from the relay/MOSFET module.
5. All existing device, web, Wi-Fi, mDNS, settings, and cloud functions work on the new board.

## Migration Decisions

- Approved pin map: GPIO1 pressure ADC, GPIO2 DS18B20, GPIO4 OLED SDA, GPIO5 OLED SCL, GPIO6 solenoid control.
- Existing two-task/two-core architecture remains unchanged: SensorTask on Core 1, NetworkTask on Core 0.
- NVS settings from the existing physical ESP32 will not transfer automatically. The new S3 starts with defaults and is configured through the existing configuration portal unless settings are explicitly migrated later.
