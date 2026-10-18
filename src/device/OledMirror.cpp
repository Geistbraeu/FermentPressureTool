#include "device/OledMirror.h"
#include "device/OledFrameCoverage.h"
#include "config.h"
#include <driver/gpio.h>
#include <esp_cpu.h>
#include <esp_intr_alloc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/gpio_struct.h>

namespace {
  constexpr uint16_t EVENT_START = 0x100;
  constexpr uint16_t EVENT_STOP = 0x101;
  constexpr uint16_t EVENT_NACK = 0x102;
  constexpr size_t CAPTURE_BUFFER_MASK = OledMirrorConfig::CAPTURE_BUFFER_SIZE - 1;
  constexpr uint8_t OLED_ADDRESS = DisplayConfig::OLED_I2C_ADDRESS;
  constexpr uint32_t SCL_MASK = 1UL << OledMirrorConfig::I2C_SCL_PIN;
  constexpr uint32_t SDA_MASK = 1UL << OledMirrorConfig::I2C_SDA_PIN;
  constexpr uint32_t BUS_MASK = SCL_MASK | SDA_MASK;
  constexpr uint32_t PUBLISH_IDLE_MS = 3;

  static_assert((OledMirrorConfig::CAPTURE_BUFFER_SIZE &
                 (OledMirrorConfig::CAPTURE_BUFFER_SIZE - 1)) == 0,
                "I2C capture buffer size must be a power of two");

  volatile uint16_t captureBuffer[OledMirrorConfig::CAPTURE_BUFFER_SIZE]{};
  volatile uint16_t captureHead = 0;
  volatile uint16_t captureTail = 0;
  volatile uint32_t captureOverflowCount = 0;
  volatile uint8_t capturedByte = 0;
  volatile uint8_t capturedBitCount = 0;
  volatile bool skipAcknowledgementClock = false;
  volatile bool captureEnabled = false;
  volatile uint32_t captureErrorCount = 0;
  volatile uint32_t decoderErrorCount = 0;
  volatile uint32_t captureTimeoutCount = 0;
  volatile uint32_t incompleteByteCount = 0;
  volatile uint8_t lastBoundaryBits = 0;
  volatile uint16_t lastBoundaryBytes = 0;
  volatile uint8_t lastBoundaryAddress = 0;
  volatile bool lastBoundaryWasStop = false;
  uint32_t nackCount = 0;
  uint32_t invalidControlCount = 0;
  volatile bool capturingTransaction = false;
  gpio_isr_handle_t captureInterrupt = nullptr;
  uint32_t transactionTimeoutCycles = 0;

  portMUX_TYPE framebufferMux = portMUX_INITIALIZER_UNLOCKED;
  OledMirror::Snapshot currentSnapshot;
  OledMirror::Snapshot publishedSnapshot;
  OledMirror::FrameTiming frameTiming;
  bool newFrameReady = false;
  OledMirror::FrameCoverage frameCoverage;
  volatile uint16_t receivedByteCount = 0;
  volatile uint32_t droppedFrameCount = 0;
  static_assert(OledMirror::FrameCoverage::SIZE == OledMirror::FRAMEBUFFER_SIZE,
                "Frame coverage must match the visible SH1106 RAM size");

  bool inTransaction = false;
  bool expectingAddress = false;
  bool selectedAddress = false;
  bool expectingControl = false;
  bool controlContinues = false;
  bool dataMode = false;

  uint8_t column = 0;
  uint8_t page = 0;
  bool pageKnown = false;
  uint8_t columnKnown = 0;

  uint8_t pendingCommand = 0;
  uint8_t pendingParameters = 0;
  uint8_t parameterIndex = 0;
  uint8_t parameters[6]{};

  uint32_t totalCaptureErrors() {
    return captureErrorCount + decoderErrorCount;
  }

  inline void IRAM_ATTR pushCaptureEvent(uint16_t event) {
    const uint16_t head = captureHead;
    const uint16_t nextHead =
        static_cast<uint16_t>((head + 1) & CAPTURE_BUFFER_MASK);
    if (nextHead == captureTail) {
      ++captureOverflowCount;
      return;
    }

    captureBuffer[head] = event;
    captureHead = nextHead;
  }

  void IRAM_ATTR onBusChange(void*) {
    GPIO.status_w1tc = SDA_MASK;
    uint32_t previous = GPIO.in & BUS_MASK;
    if (!captureEnabled || previous != SCL_MASK) {
      return;
    }

    // One bounded polling burst avoids interrupt dispatch latency on every edge.
    // The task on the other core drains bytes; neither monitored pin is driven.
    capturingTransaction = true;
    capturedByte = 0;
    capturedBitCount = 0;
    skipAcknowledgementClock = false;
    pushCaptureEvent(EVENT_START);
    const uint32_t startedAt = esp_cpu_get_ccount();
    uint16_t transactionBytes = 0;
    uint8_t transactionAddress = 0;
    for (;;) {
      const uint32_t levels = GPIO.in & BUS_MASK;
      const uint32_t changed = levels ^ previous;
      if ((changed & SDA_MASK) && (levels & SCL_MASK) && (previous & SCL_MASK)) {
        if (capturedBitCount > 1 || skipAcknowledgementClock) {
          ++captureErrorCount;
          ++incompleteByteCount;
          lastBoundaryBits = skipAcknowledgementClock ? 8 : capturedBitCount;
          lastBoundaryBytes = transactionBytes;
          lastBoundaryAddress = transactionAddress;
          lastBoundaryWasStop = (levels & SDA_MASK) != 0;
        }
        pushCaptureEvent((levels & SDA_MASK) ? EVENT_STOP : EVENT_START);
        capturedByte = 0;
        capturedBitCount = 0;
        skipAcknowledgementClock = false;
        transactionBytes = 0;
        transactionAddress = 0;
        if (levels & SDA_MASK) {
          break;
        }
      } else if ((changed & SCL_MASK) && (levels & SCL_MASK)) {
        if (skipAcknowledgementClock) {
          if (transactionBytes == 0) {
            transactionAddress = capturedByte;
          }
          ++transactionBytes;
          pushCaptureEvent((levels & SDA_MASK) ? EVENT_NACK : capturedByte);
          capturedByte = 0;
          skipAcknowledgementClock = false;
        } else {
          capturedByte = static_cast<uint8_t>(
              (capturedByte << 1) | ((levels & SDA_MASK) ? 1 : 0));
          if (++capturedBitCount == 8) {
            capturedBitCount = 0;
            skipAcknowledgementClock = true;
          }
        }
      }
      previous = levels;
      if (static_cast<uint32_t>(esp_cpu_get_ccount() - startedAt) >=
          transactionTimeoutCycles) {
        ++captureErrorCount;
        ++captureTimeoutCount;
        break;
      }
    }
    capturingTransaction = false;
    GPIO.status_w1tc = SDA_MASK;
  }

  void resetTransactionParser() {
    inTransaction = false;
    expectingAddress = false;
    selectedAddress = false;
    expectingControl = false;
    controlContinues = false;
  }

  uint8_t commandParameterCount(uint8_t command) {
    switch (command) {
      case 0x81: return 1;
      case 0xA8: return 1;
      case 0xD3: return 1;
      case 0xD5: return 1;
      case 0xD9: return 1;
      case 0xDA: return 1;
      case 0xDB: return 1;
      case 0xAD: return 1;
      default: return 0;
    }
  }

  void applyCommand(uint8_t command, const uint8_t* args) {
    switch (command) {
      case 0x40:
        currentSnapshot.knownState |= 8;
        currentSnapshot.startLine = 0;
        ++currentSnapshot.sequence;
        break;
      case 0xA0:
        currentSnapshot.knownState |= 2;
        currentSnapshot.segmentRemap = false;
        ++currentSnapshot.sequence;
        break;
      case 0xA1:
        currentSnapshot.knownState |= 2;
        currentSnapshot.segmentRemap = true;
        ++currentSnapshot.sequence;
        break;
      case 0xA4:
        currentSnapshot.knownState |= 64;
        currentSnapshot.displayAllOn = false;
        ++currentSnapshot.sequence;
        break;
      case 0xA5:
        currentSnapshot.knownState |= 64;
        currentSnapshot.displayAllOn = true;
        ++currentSnapshot.sequence;
        break;
      case 0xA6:
        currentSnapshot.knownState |= 32;
        currentSnapshot.inverse = false;
        ++currentSnapshot.sequence;
        break;
      case 0xA7:
        currentSnapshot.knownState |= 32;
        currentSnapshot.inverse = true;
        ++currentSnapshot.sequence;
        break;
      case 0xAE:
        currentSnapshot.knownState |= 1;
        currentSnapshot.displayEnabled = false;
        ++currentSnapshot.sequence;
        break;
      case 0xAF:
        currentSnapshot.knownState |= 1;
        currentSnapshot.displayEnabled = true;
        ++currentSnapshot.sequence;
        break;
      case 0xD3:
        currentSnapshot.knownState |= 16;
        currentSnapshot.displayOffset = args[0] & 0x3F;
        ++currentSnapshot.sequence;
        break;
      case 0xC0:
        currentSnapshot.knownState |= 4;
        currentSnapshot.comScanReverse = false;
        ++currentSnapshot.sequence;
        break;
      case 0xC8:
        currentSnapshot.knownState |= 4;
        currentSnapshot.comScanReverse = true;
        ++currentSnapshot.sequence;
        break;
      default:
        if (command >= 0x40 && command <= 0x7F) {
          currentSnapshot.knownState |= 8;
          currentSnapshot.startLine = command & 0x3F;
          ++currentSnapshot.sequence;
        } else if (command >= 0xB0 && command <= 0xB7) {
          page = command & 0x07;
          pageKnown = true;
          columnKnown = 0;
        } else if (command <= 0x0F) {
          column = static_cast<uint8_t>((column & 0xF0) | (command & 0x0F));
          columnKnown |= 1;
        } else if (command >= 0x10 && command <= 0x1F) {
          column = static_cast<uint8_t>((column & 0x0F) |
                                        ((command & 0x0F) << 4));
          columnKnown |= 2;
        }
        break;
    }
  }

  void processCommandByte(uint8_t value) {
    if (pendingParameters > 0) {
      parameters[parameterIndex++] = value;
      --pendingParameters;
      if (pendingParameters == 0) {
        applyCommand(pendingCommand, parameters);
        parameterIndex = 0;
      }
      return;
    }

    pendingCommand = value;
    pendingParameters = commandParameterCount(value);
    parameterIndex = 0;
    if (pendingParameters == 0) {
      applyCommand(value, parameters);
    }
  }

  void advanceAddress() {
    column = (column + 1) % OledMirror::RAM_WIDTH;
  }

  void processDisplayData(uint8_t value) {
    if (!pageKnown || columnKnown != 3) {
      return;
    }
    if (page >= 8 || column >= OledMirror::RAM_WIDTH) {
      ++decoderErrorCount;
      return;
    }

    if (column >= OledMirror::VISIBLE_COLUMN_OFFSET &&
        column < OledMirror::VISIBLE_COLUMN_OFFSET + 128) {
      const uint16_t visibleIndex =
          static_cast<size_t>(page) * 128 + column - OledMirror::VISIBLE_COLUMN_OFFSET;
      const OledMirror::FrameCoverage next = frameCoverage.accept(visibleIndex);
      if (frameCoverage.collecting &&
          (visibleIndex == 0 || next.count == 0)) {
        ++droppedFrameCount;
      }
      frameCoverage = next;
      receivedByteCount = next.count;
      currentSnapshot.capturedBytes = next.count;
      currentSnapshot.ready = next.complete;
      newFrameReady = next.complete;
      if (next.count != 0) {
        const size_t index = static_cast<size_t>(page) * OledMirror::RAM_WIDTH + column;
        currentSnapshot.framebuffer[index] = value;
        currentSnapshot.lastUpdateMs = millis();
        if (next.complete) {
          ++currentSnapshot.sequence;
        }
      }
    }
    advanceAddress();
  }

  void processTransactionByte(uint8_t value) {
    if (expectingAddress) {
      expectingAddress = false;
      const uint8_t address = value >> 1;
      selectedAddress = (address == OLED_ADDRESS ||
                         address == static_cast<uint8_t>(OLED_ADDRESS + 1)) &&
                        ((value & 1) == 0);
      expectingControl = selectedAddress;
      return;
    }

    if (!selectedAddress) {
      return;
    }

    if (expectingControl) {
      if ((value & 0x3F) != 0) {
        ++decoderErrorCount;
        ++invalidControlCount;
        selectedAddress = false;
        return;
      }
      controlContinues = (value & 0x80) != 0;
      dataMode = (value & 0x40) != 0;
      expectingControl = false;
      return;
    }

    if (dataMode) {
      processDisplayData(value);
    } else {
      processCommandByte(value);
    }

    if (controlContinues) {
      expectingControl = true;
    }
  }

  void processCaptureEvent(uint16_t event) {
    if (event == EVENT_START) {
      resetTransactionParser();
      inTransaction = true;
      expectingAddress = true;
    } else if (event == EVENT_STOP || event == EVENT_NACK) {
      if (event == EVENT_NACK && selectedAddress) {
        ++decoderErrorCount;
        ++nackCount;
      }
      resetTransactionParser();
    } else if (inTransaction) {
      processTransactionByte(static_cast<uint8_t>(event));
    }
  }

  void stopCapture() {
    captureEnabled = false;
    gpio_intr_disable(static_cast<gpio_num_t>(OledMirrorConfig::I2C_SCL_PIN));
    gpio_intr_disable(static_cast<gpio_num_t>(OledMirrorConfig::I2C_SDA_PIN));
    if (captureInterrupt != nullptr) {
      esp_intr_free(captureInterrupt);
      captureInterrupt = nullptr;
    }
  }

  bool initializeCapture() {
    // setup() runs on core 1; keep the ISR away from Wi-Fi/cloud work on core 0.
    gpio_config_t pins{};
    pins.pin_bit_mask = BUS_MASK;
    pins.mode = GPIO_MODE_INPUT;
    pins.intr_type = GPIO_INTR_DISABLE;
    esp_err_t result = gpio_config(&pins);
    if (result == ESP_OK) {
      result = gpio_isr_register(onBusChange, nullptr,
                                ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                                &captureInterrupt);
    }
    if (result == ESP_OK) {
      transactionTimeoutCycles = getCpuFrequencyMhz() * 20000UL;
      GPIO.status_w1tc = BUS_MASK;
      captureEnabled = true;
      result = gpio_set_intr_type(static_cast<gpio_num_t>(OledMirrorConfig::I2C_SDA_PIN),
                                   GPIO_INTR_ANYEDGE);
      if (result == ESP_OK) {
        result = gpio_intr_enable(static_cast<gpio_num_t>(OledMirrorConfig::I2C_SDA_PIN));
      }
    }
    if (result != ESP_OK) {
      stopCapture();
      Serial.printf("[OLED mirror] GPIO capture initialization failed: %s\n",
                    esp_err_to_name(result));
      return false;
    }
    Serial.printf("[OLED mirror] Direct IRAM GPIO capture on core %d\n", xPortGetCoreID());
    return true;
  }
}

namespace OledMirror {
  bool begin() {
    const uint8_t sdaPin = OledMirrorConfig::I2C_SDA_PIN;
    const uint8_t sclPin = OledMirrorConfig::I2C_SCL_PIN;
    if (sdaPin >= 32 || sclPin >= 32 || sdaPin == sclPin) {
      return false;
    }
    if (!initializeCapture()) {
      return false;
    }
    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        task,
        "OledMirrorTask",
        OledMirrorConfig::TASK_STACK_SIZE,
        nullptr,
        OledMirrorConfig::TASK_PRIORITY,
        nullptr,
        OledMirrorConfig::TASK_CORE);
    if (taskCreated != pdPASS) {
      stopCapture();
      return false;
    }
    return true;
  }

  void task(void*) {
    uint32_t observedOverflowCount = 0;
    uint32_t observedErrorCount = 0;
    uint32_t lastEventMs = millis();
    uint32_t lastErrorLogMs = 0;
    bool pendingPublication = false;
    for (;;) {
      if (observedOverflowCount != captureOverflowCount ||
          observedErrorCount != totalCaptureErrors()) {
        observedOverflowCount = captureOverflowCount;
        observedErrorCount = totalCaptureErrors();
        resetTransactionParser();
        pendingParameters = 0;
        parameterIndex = 0;
        pageKnown = false;
        columnKnown = 0;
        captureTail = captureHead;
        if (frameCoverage.collecting) {
          ++droppedFrameCount;
        }
        frameCoverage = FrameCoverage();
        receivedByteCount = 0;
        currentSnapshot.capturedBytes = 0;
        currentSnapshot.ready = false;
        newFrameReady = false;
        currentSnapshot.knownState = 0;
        ++currentSnapshot.sequence;
        currentSnapshot.overflowCount = observedOverflowCount;
        currentSnapshot.captureErrors = observedErrorCount;
        pendingPublication = false;
        if (lastErrorLogMs == 0 || millis() - lastErrorLogMs >= 5000) {
          Serial.printf("[OLED mirror] Capture lost synchronization: errors=%lu, overflows=%lu, timeouts=%lu, boundaries=%lu, nack=%lu, control=%lu; last boundary bits=%u bytes=%u address=0x%02X stop=%u; reacquiring\n",
                        static_cast<unsigned long>(observedErrorCount),
                        static_cast<unsigned long>(observedOverflowCount),
                        static_cast<unsigned long>(captureTimeoutCount),
                        static_cast<unsigned long>(incompleteByteCount),
                        static_cast<unsigned long>(nackCount),
                        static_cast<unsigned long>(invalidControlCount),
                        static_cast<unsigned int>(lastBoundaryBits),
                        static_cast<unsigned int>(lastBoundaryBytes),
                        static_cast<unsigned int>(lastBoundaryAddress),
                        static_cast<unsigned int>(lastBoundaryWasStop));
          lastErrorLogMs = millis();
        }
      }

      bool didWork = false;
      uint16_t processedEvents = 0;
      while (captureTail != captureHead &&
             observedOverflowCount == captureOverflowCount &&
             observedErrorCount == totalCaptureErrors()) {
        const uint16_t event = captureBuffer[captureTail];
        captureTail = static_cast<uint16_t>((captureTail + 1) &
                                            CAPTURE_BUFFER_MASK);
        processCaptureEvent(event);
        lastEventMs = millis();
        pendingPublication = true;
        didWork = true;
        if (++processedEvents == 256) {
          taskYIELD();
          processedEvents = 0;
        }
      }

      if (pendingPublication && currentSnapshot.ready && !inTransaction &&
          captureTail == captureHead && !capturingTransaction &&
          observedErrorCount == totalCaptureErrors() &&
          observedOverflowCount == captureOverflowCount &&
          millis() - lastEventMs >= PUBLISH_IDLE_MS) {
        currentSnapshot.droppedFrames = droppedFrameCount;
        portENTER_CRITICAL(&framebufferMux);
        publishedSnapshot = currentSnapshot;
        if (newFrameReady) {
          frameTiming.record(millis());
          newFrameReady = false;
        }
        portEXIT_CRITICAL(&framebufferMux);
        pendingPublication = false;
      }

      if (!didWork) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }

  void getStatus(Status& status) {
    portENTER_CRITICAL(&framebufferMux);
    status.timing = frameTiming;
    status.sequence = publishedSnapshot.sequence;
    status.lastUpdateMs = publishedSnapshot.lastUpdateMs;
    status.overflowCount = captureOverflowCount;
    status.captureErrors = totalCaptureErrors();
    status.capturedBytes = receivedByteCount;
    status.droppedFrames = droppedFrameCount;
    status.startLine = publishedSnapshot.startLine;
    status.displayOffset = publishedSnapshot.displayOffset;
    status.knownState = publishedSnapshot.knownState;
    status.ready = publishedSnapshot.ready;
    status.recovering = publishedSnapshot.ready &&
                        (publishedSnapshot.captureErrors != status.captureErrors ||
                         publishedSnapshot.overflowCount != status.overflowCount ||
                         publishedSnapshot.droppedFrames != status.droppedFrames);
    status.displayEnabled = publishedSnapshot.displayEnabled;
    status.displayAllOn = publishedSnapshot.displayAllOn;
    status.inverse = publishedSnapshot.inverse;
    status.segmentRemap = publishedSnapshot.segmentRemap;
    status.comScanReverse = publishedSnapshot.comScanReverse;
    portEXIT_CRITICAL(&framebufferMux);
  }

  void getSnapshot(Snapshot& snapshot) {
    portENTER_CRITICAL(&framebufferMux);
    memcpy(&snapshot, &publishedSnapshot, sizeof(snapshot));
    portEXIT_CRITICAL(&framebufferMux);
  }
}
