/**
 * @file I2cTransport.h
 * @brief Wire-based I2C transport adapter for INA3221 examples.
 *
 * This file provides Wire-compatible I2C callbacks that can be
 * used with the INA3221 driver. The library does not depend on Wire
 * directly; this adapter bridges them.
 *
 * NOT part of the library API. Example-only.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "INA3221/Status.h"

namespace transport {

struct WireContext {
  TwoWire* wire = nullptr;
  uint32_t configuredTimeoutMs = 0;
  uint32_t frequencyHz = 0;
};

struct TransferStats {
  uint32_t read = 0;
  uint32_t write = 0;
};

inline WireContext& wireContext() {
  static WireContext context{&Wire, 0, 0};
  return context;
}

inline TransferStats& transferStatsStorage() {
  static TransferStats stats{};
  return stats;
}

inline void resetTransferStats() {
  transferStatsStorage() = {};
}

inline TransferStats transferStats() {
  return transferStatsStorage();
}

inline void noteReadTransfer() {
  if (transferStatsStorage().read != UINT32_MAX) {
    ++transferStatsStorage().read;
  }
}

inline void noteWriteTransfer() {
  if (transferStatsStorage().write != UINT32_MAX) {
    ++transferStatsStorage().write;
  }
}

inline INA3221::Status validateTransferContext(uint32_t timeoutMs, void* user,
                                                TwoWire*& wire) {
  WireContext* context = static_cast<WireContext*>(user);
  if (context == nullptr || context->wire == nullptr) {
    return INA3221::Status::Error(INA3221::Err::INVALID_CONFIG,
                                  "Wire context is null");
  }
  if (timeoutMs == 0 || context->configuredTimeoutMs == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM,
                                  "I2C timeout must be finite");
  }
  // TwoWire exposes a bus-level timeout, not a per-call timeout. The example
  // configures it once in initWire(). Refuse a tighter callback deadline rather
  // than silently exceeding it or reconfiguring the shared bus in a callback.
  if (timeoutMs < context->configuredTimeoutMs) {
    return INA3221::Status::Error(
        INA3221::Err::INVALID_CONFIG,
        "Requested deadline is tighter than fixed Wire timeout");
  }
  wire = context->wire;
  return INA3221::Status::Ok();
}

/**
 * @brief Wire-based I2C write implementation.
 *
 * Pass to Config::i2cWrite and use transport::configUser() as i2cUser.
 * The callback context is a WireContext, not a bare TwoWire pointer.
 */
inline INA3221::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                 uint32_t timeoutMs, void* user) {
  TwoWire* wire = nullptr;
  INA3221::Status contextStatus =
      validateTransferContext(timeoutMs, user, wire);
  if (!contextStatus.ok()) return contextStatus;
  if (!data || len == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C write params");
  }
  if (len > 128) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Write exceeds I2C buffer",
                                  static_cast<int32_t>(len));
  }
  noteWriteTransfer();
  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);
  if (written != len) {
    return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C write incomplete",
                                  static_cast<int32_t>(written));
  }

  uint8_t result = wire->endTransmission(true);
  switch (result) {
    case 0:
      return INA3221::Status::Ok();
    case 1:
      return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C data too long", result);
    case 2:
      return INA3221::Status::Error(INA3221::Err::I2C_NACK_ADDR, "I2C address NACK", result);
    case 3:
      return INA3221::Status::Error(INA3221::Err::I2C_NACK_DATA, "I2C data NACK", result);
    case 4:
      return INA3221::Status::Error(INA3221::Err::I2C_BUS, "I2C bus error", result);
    case 5:
      return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT, "I2C timeout", result);
    default:
      return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C unknown error", result);
  }
}

/**
 * @brief Wire-based I2C write-read implementation.
 *
 * Pass to Config::i2cWriteRead and use transport::configUser() as i2cUser.
 * The callback context is a WireContext, not a bare TwoWire pointer.
 */
inline INA3221::Status wireWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                                     uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                     void* user) {
  TwoWire* wire = nullptr;
  INA3221::Status contextStatus =
      validateTransferContext(timeoutMs, user, wire);
  if (!contextStatus.ok()) return contextStatus;
  if ((txLen > 0 && tx == nullptr) || (rxLen > 0 && rx == nullptr)) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C read params");
  }
  if (txLen == 0 || rxLen == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C read length invalid");
  }
  if (txLen > 128 || rxLen > 128) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C read exceeds buffer");
  }
  noteReadTransfer();
  wire->beginTransmission(addr);
  size_t written = wire->write(tx, txLen);
  if (written != txLen) {
    return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C write incomplete",
                                  static_cast<int32_t>(written));
  }

  uint8_t result = wire->endTransmission(false);  // Repeated start
  switch (result) {
    case 0:
      break;
    case 1:
      return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C data too long", result);
    case 2:
      return INA3221::Status::Error(INA3221::Err::I2C_NACK_ADDR, "I2C address NACK", result);
    case 3:
      return INA3221::Status::Error(INA3221::Err::I2C_NACK_DATA, "I2C data NACK", result);
    case 4:
      return INA3221::Status::Error(INA3221::Err::I2C_BUS, "I2C bus error", result);
    case 5:
      return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT, "I2C timeout", result);
    default:
      return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C write failed", result);
  }

  size_t read = wire->requestFrom(addr, static_cast<uint8_t>(rxLen));
  if (read != rxLen) {
    return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C read length mismatch",
                                  static_cast<int32_t>(read));
  }

  for (size_t i = 0; i < rxLen; ++i) {
    if (wire->available()) {
      rx[i] = static_cast<uint8_t>(wire->read());
    } else {
      return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C data not available");
    }
  }

  return INA3221::Status::Ok();
}

/**
 * @brief Initialize Wire with default pins and frequency.
 */
inline bool initWire(int sda, int scl, uint32_t freq = 400000, uint16_t timeoutMs = 50) {
  WireContext& context = wireContext();
  context.wire = nullptr;
  context.configuredTimeoutMs = 0;
  context.frequencyHz = 0;
#if defined(ARDUINO_ARCH_ESP32)
  // Toggle SCL to release any stuck slave
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
  }
  // Generate STOP condition
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
#endif

  // Pass the frequency into begin() atomically. In pioarduino 55.03.311 the
  // new I2C backend's i2cSetClock() returns ESP_FAIL when no device handles
  // exist yet, even though it updates the bus frequency. Calling setClock()
  // immediately after begin() would therefore report a false startup failure.
  if (!Wire.begin(sda, scl, freq)) {
    return false;
  }
  if (Wire.getClock() != freq) {
    Wire.end();
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(timeoutMs);
#else
  (void)timeoutMs;
#endif
  context.wire = &Wire;
  context.configuredTimeoutMs = timeoutMs;
  context.frequencyHz = freq;
  return true;
}

inline INA3221::Status setFrequency(uint32_t frequencyHz) {
  WireContext& context = wireContext();
  if (context.wire == nullptr || context.configuredTimeoutMs == 0U) {
    return INA3221::Status::Error(INA3221::Err::I2C_BUS,
                                  "Wire bus is not initialized");
  }
  if (frequencyHz < 10000U || frequencyHz > 400000U) {
    return INA3221::Status::Error(INA3221::Err::OUT_OF_RANGE,
                                  "I2C frequency must be 10000-400000 Hz",
                                  static_cast<int32_t>(frequencyHz));
  }
  if (!context.wire->setClock(frequencyHz)) {
    return INA3221::Status::Error(INA3221::Err::I2C_BUS,
                                  "Wire rejected I2C frequency",
                                  static_cast<int32_t>(frequencyHz));
  }
  context.frequencyHz = frequencyHz;
  return INA3221::Status::Ok();
}

inline uint32_t frequencyHz() {
  return wireContext().frequencyHz;
}

inline INA3221::Status probeAddress(uint8_t addr, uint16_t timeoutMs) {
  WireContext& context = wireContext();
  TwoWire* wire = nullptr;
  INA3221::Status contextStatus =
      validateTransferContext(timeoutMs, &context, wire);
  if (!contextStatus.ok()) return contextStatus;
  wire->beginTransmission(addr);
  const uint8_t result = wire->endTransmission(true);
  switch (result) {
    case 0: return INA3221::Status::Ok();
    case 2: return INA3221::Status::Error(INA3221::Err::I2C_NACK_ADDR,
                                          "I2C address NACK", result);
    case 3: return INA3221::Status::Error(INA3221::Err::I2C_NACK_DATA,
                                          "I2C data NACK", result);
    case 4: return INA3221::Status::Error(INA3221::Err::I2C_BUS,
                                          "I2C bus error", result);
    case 5: return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT,
                                          "I2C timeout", result);
    default: return INA3221::Status::Error(INA3221::Err::I2C_ERROR,
                                           "I2C address probe failed", result);
  }
}

inline INA3221::Status wireWriteReadAt(uint8_t addr, const uint8_t* tx,
                                        size_t txLen, uint8_t* rx, size_t rxLen,
                                        uint32_t timeoutMs) {
  return wireWriteRead(addr, tx, txLen, rx, rxLen, timeoutMs, &wireContext());
}

inline uint32_t arduinoNowMs(void*) {
  return millis();
}

inline void arduinoYield(void*) {
  yield();
}

inline void* configUser() {
  return &wireContext();
}

}  // namespace transport
