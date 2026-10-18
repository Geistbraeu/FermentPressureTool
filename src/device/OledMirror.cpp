#include "device/OledMirror.h"
#include "config.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/gpio_struct.h>

namespace {
  constexpr uint16_t EVENT_START = 0x100;
  constexpr uint16_t EVENT_STOP = 0x101;
  constexpr size_t CAPTURE_BUFFER_MASK = OledMirrorConfig::CAPTURE_BUFFER_SIZE - 1;
  constexpr uint8_t OLED_ADDRESS = DisplayConfig::OLED_I2C_ADDRESS;

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

  portMUX_TYPE framebufferMux = portMUX_INITIALIZER_UNLOCKED;
  OledMirror::Snapshot currentSnapshot;
  uint32_t receivedBytes[OledMirror::FRAMEBUFFER_SIZE / 32]{};
  uint16_t receivedByteCount = 0;

  bool inTransaction = false;
  bool expectingAddress = false;
  bool selectedAddress = false;
  bool expectingControl = false;
  bool controlContinues = false;
  bool dataMode = false;

  uint8_t column = 0;
  uint8_t page = 0;
  uint8_t columnStart = 0;
  uint8_t columnEnd = OledMirror::RAM_WIDTH - 1;
  uint8_t pageStart = 0;
  uint8_t pageEnd = 7;
  uint8_t addressingMode = 2;

  uint8_t pendingCommand = 0;
  uint8_t pendingParameters = 0;
  uint8_t parameterIndex = 0;
  uint8_t parameters[6]{};

  inline bool IRAM_ATTR readMonitorPin(uint8_t pin) {
    return (GPIO.in & (1UL << pin)) != 0;
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

  void IRAM_ATTR onClockRising(void*) {
    if (!captureEnabled) {
      return;
    }
    if (skipAcknowledgementClock) {
      skipAcknowledgementClock = false;
      return;
    }

    capturedByte = static_cast<uint8_t>(
        (capturedByte << 1) |
        (readMonitorPin(OledMirrorConfig::I2C_SDA_PIN) ? 1 : 0));
    ++capturedBitCount;
    if (capturedBitCount == 8) {
      pushCaptureEvent(capturedByte);
      capturedByte = 0;
      capturedBitCount = 0;
      skipAcknowledgementClock = true;
    }
  }

  void IRAM_ATTR onDataLineChange(void*) {
    if (!captureEnabled || !readMonitorPin(OledMirrorConfig::I2C_SCL_PIN)) {
      return;
    }

    if (readMonitorPin(OledMirrorConfig::I2C_SDA_PIN)) {
      pushCaptureEvent(EVENT_STOP);
    } else {
      pushCaptureEvent(EVENT_START);
    }
    capturedByte = 0;
    capturedBitCount = 0;
    skipAcknowledgementClock = false;
  }

  void resetTransactionParser() {
    inTransaction = false;
    expectingAddress = false;
    selectedAddress = false;
    expectingControl = false;
    controlContinues = false;
    pendingParameters = 0;
    parameterIndex = 0;
  }

  uint8_t commandParameterCount(uint8_t command) {
    switch (command) {
      case 0x20: return 1;
      case 0x21: return 2;
      case 0x22: return 2;
      case 0x26: return 6;
      case 0x27: return 6;
      case 0x29: return 5;
      case 0x2A: return 5;
      case 0x81: return 1;
      case 0x8D: return 1;
      case 0xA3: return 2;
      case 0xA8: return 1;
      case 0xD3: return 1;
      case 0xD5: return 1;
      case 0xD9: return 1;
      case 0xDA: return 1;
      case 0xDB: return 1;
      case 0xDC: return 1;
      case 0xAD: return 1;
      case 0xFD: return 1;
      default: return 0;
    }
  }

  void applyCommand(uint8_t command, const uint8_t* args) {
    portENTER_CRITICAL(&framebufferMux);
    switch (command) {
      case 0x20:
        addressingMode = args[0] & 0x03;
        break;
      case 0x21:
        columnStart = args[0] & 0x7F;
        columnEnd = args[1] & 0x7F;
        column = columnStart;
        break;
      case 0x22:
        pageStart = args[0] & 0x07;
        pageEnd = args[1] & 0x07;
        page = pageStart;
        break;
      case 0x40:
        currentSnapshot.startLine = 0;
        ++currentSnapshot.sequence;
        break;
      case 0xA0:
        currentSnapshot.segmentRemap = false;
        ++currentSnapshot.sequence;
        break;
      case 0xA1:
        currentSnapshot.segmentRemap = true;
        ++currentSnapshot.sequence;
        break;
      case 0xA4:
        currentSnapshot.displayAllOn = false;
        ++currentSnapshot.sequence;
        break;
      case 0xA5:
        currentSnapshot.displayAllOn = true;
        ++currentSnapshot.sequence;
        break;
      case 0xA6:
        currentSnapshot.inverse = false;
        ++currentSnapshot.sequence;
        break;
      case 0xA7:
        currentSnapshot.inverse = true;
        ++currentSnapshot.sequence;
        break;
      case 0xAE:
        currentSnapshot.displayEnabled = false;
        ++currentSnapshot.sequence;
        break;
      case 0xAF:
        currentSnapshot.displayEnabled = true;
        ++currentSnapshot.sequence;
        break;
      case 0xD3:
        currentSnapshot.displayOffset = args[0] & 0x3F;
        ++currentSnapshot.sequence;
        break;
      case 0xC0:
        currentSnapshot.comScanReverse = false;
        ++currentSnapshot.sequence;
        break;
      case 0xC8:
        currentSnapshot.comScanReverse = true;
        ++currentSnapshot.sequence;
        break;
      default:
        if (command >= 0x40 && command <= 0x7F) {
          currentSnapshot.startLine = command & 0x3F;
          ++currentSnapshot.sequence;
        } else if (command >= 0xB0 && command <= 0xB7) {
          page = command & 0x07;
        } else if (command <= 0x0F) {
          column = static_cast<uint8_t>((column & 0xF0) | (command & 0x0F));
        } else if (command >= 0x10 && command <= 0x1F) {
          column = static_cast<uint8_t>((column & 0x0F) |
                                        ((command & 0x0F) << 4));
        }
        break;
    }
    portEXIT_CRITICAL(&framebufferMux);
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
    if (addressingMode == 0) {
      if (column < columnEnd) {
        ++column;
      } else {
        column = columnStart;
        page = (page < pageEnd) ? static_cast<uint8_t>(page + 1) : pageStart;
      }
    } else if (addressingMode == 1) {
      if (page < pageEnd) {
        ++page;
      } else {
        page = pageStart;
        column = (column < columnEnd) ? static_cast<uint8_t>(column + 1)
                                      : columnStart;
      }
    } else {
      column = (column + 1) % OledMirror::RAM_WIDTH;
    }
  }

  void processDisplayData(uint8_t value) {
    if (page >= 8 || column >= OledMirror::RAM_WIDTH) {
      return;
    }

    const size_t index = static_cast<size_t>(page) * OledMirror::RAM_WIDTH + column;
    portENTER_CRITICAL(&framebufferMux);
    if (currentSnapshot.framebuffer[index] != value) {
      currentSnapshot.framebuffer[index] = value;
      ++currentSnapshot.sequence;
    }
    currentSnapshot.lastUpdateMs = millis();
    if (column >= OledMirror::VISIBLE_COLUMN_OFFSET &&
        column < OledMirror::VISIBLE_COLUMN_OFFSET + 128) {
      const size_t visibleIndex =
          static_cast<size_t>(page) * 128 + column - OledMirror::VISIBLE_COLUMN_OFFSET;
      const size_t coverageWord = visibleIndex / 32;
      const uint32_t coverageBit = 1UL << (visibleIndex % 32);
      if ((receivedBytes[coverageWord] & coverageBit) == 0) {
        receivedBytes[coverageWord] |= coverageBit;
        ++receivedByteCount;
        currentSnapshot.capturedBytes = receivedByteCount;
        if (receivedByteCount == OledMirror::FRAMEBUFFER_SIZE) {
          currentSnapshot.ready = true;
        }
      }
    }
    portEXIT_CRITICAL(&framebufferMux);
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
    } else if (event == EVENT_STOP) {
      resetTransactionParser();
    } else if (inTransaction) {
      processTransactionByte(static_cast<uint8_t>(event));
    }
  }
}

namespace OledMirror {
  bool begin() {
    const uint8_t sdaPin = OledMirrorConfig::I2C_SDA_PIN;
    const uint8_t sclPin = OledMirrorConfig::I2C_SCL_PIN;
    if (sdaPin >= 32 || sclPin >= 32 || sdaPin == sclPin) {
      return false;
    }

    pinMode(sdaPin, INPUT);
    pinMode(sclPin, INPUT);
    captureEnabled = false;
    capturedByte = 0;
    capturedBitCount = 0;

    attachInterruptArg(sclPin, onClockRising, nullptr, RISING);
    attachInterruptArg(sdaPin, onDataLineChange, nullptr, CHANGE);
    captureEnabled = true;

    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        task,
        "OledMirrorTask",
        OledMirrorConfig::TASK_STACK_SIZE,
        nullptr,
        OledMirrorConfig::TASK_PRIORITY,
        nullptr,
        OledMirrorConfig::TASK_CORE);
    if (taskCreated != pdPASS) {
      captureEnabled = false;
      detachInterrupt(sclPin);
      detachInterrupt(sdaPin);
      return false;
    }
    return true;
  }

  void task(void*) {
    uint32_t observedOverflowCount = 0;
    for (;;) {
      if (observedOverflowCount != captureOverflowCount) {
        observedOverflowCount = captureOverflowCount;
        resetTransactionParser();
      }

      bool didWork = false;
      uint16_t processedEvents = 0;
      while (captureTail != captureHead) {
        const uint16_t event = captureBuffer[captureTail];
        captureTail = static_cast<uint16_t>((captureTail + 1) &
                                            CAPTURE_BUFFER_MASK);
        processCaptureEvent(event);
        didWork = true;
        if (++processedEvents == 256) {
          taskYIELD();
          processedEvents = 0;
        }
      }

      portENTER_CRITICAL(&framebufferMux);
      currentSnapshot.overflowCount = observedOverflowCount;
      portEXIT_CRITICAL(&framebufferMux);

      if (!didWork) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }

  void getStatus(Status& status) {
    portENTER_CRITICAL(&framebufferMux);
    status.sequence = currentSnapshot.sequence;
    status.lastUpdateMs = currentSnapshot.lastUpdateMs;
    status.overflowCount = currentSnapshot.overflowCount;
    status.capturedBytes = currentSnapshot.capturedBytes;
    status.startLine = currentSnapshot.startLine;
    status.displayOffset = currentSnapshot.displayOffset;
    status.ready = currentSnapshot.ready;
    status.displayEnabled = currentSnapshot.displayEnabled;
    status.displayAllOn = currentSnapshot.displayAllOn;
    status.inverse = currentSnapshot.inverse;
    status.segmentRemap = currentSnapshot.segmentRemap;
    status.comScanReverse = currentSnapshot.comScanReverse;
    portEXIT_CRITICAL(&framebufferMux);
  }

  void getSnapshot(Snapshot& snapshot) {
    portENTER_CRITICAL(&framebufferMux);
    memcpy(&snapshot, &currentSnapshot, sizeof(snapshot));
    portEXIT_CRITICAL(&framebufferMux);
  }
}
