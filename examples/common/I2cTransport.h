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

/**
 * @brief Wire-based I2C write implementation.
 *
 * Pass to Config::i2cWrite, and pass &Wire (or custom TwoWire*) to i2cUser.
 */
inline INA3221::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                 uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return INA3221::Status::Error(INA3221::Err::INVALID_CONFIG, "Wire instance is null");
  }
  if (!data || len == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C write params");
  }
  if (len > 128) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Write exceeds I2C buffer",
                                  static_cast<int32_t>(len));
  }

  (void)timeoutMs;

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
 * Pass to Config::i2cWriteRead, and pass &Wire (or custom TwoWire*) to i2cUser.
 */
inline INA3221::Status wireWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                                     uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                     void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return INA3221::Status::Error(INA3221::Err::INVALID_CONFIG, "Wire instance is null");
  }
  if ((txLen > 0 && tx == nullptr) || (rxLen > 0 && rx == nullptr)) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C read params");
  }
  if (txLen == 0 || rxLen == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C read length invalid");
  }
  if (txLen > 128 || rxLen > 128) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C read exceeds buffer");
  }

  (void)timeoutMs;

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
inline bool initWire(int sda, int scl, uint32_t freq = 400000, uint16_t timeoutMs = 50,
                     uint8_t address = 0x40) {
  (void)address;
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

  Wire.begin(sda, scl);
  Wire.setClock(freq);
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(timeoutMs);
#else
  (void)timeoutMs;
#endif
  return true;
}

inline INA3221::Status wireWriteReadAt(uint8_t addr, const uint8_t* tx, size_t txLen,
                                       uint8_t* rx, size_t rxLen, uint32_t timeoutMs) {
  return wireWriteRead(addr, tx, txLen, rx, rxLen, timeoutMs, &Wire);
}

inline INA3221::Status probeAddress(uint8_t addr, uint16_t timeoutMs) {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(timeoutMs);
#else
  (void)timeoutMs;
#endif
  Wire.beginTransmission(addr);
  switch (Wire.endTransmission(true)) {
    case 0:
      return INA3221::Status::Ok();
    case 1:
      return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "I2C data too long", 1);
    case 2:
      return INA3221::Status::Error(INA3221::Err::I2C_NACK_ADDR, "I2C address NACK", 2);
    case 3:
      return INA3221::Status::Error(INA3221::Err::I2C_NACK_DATA, "I2C data NACK", 3);
    case 4:
      return INA3221::Status::Error(INA3221::Err::I2C_BUS, "I2C bus error", 4);
    case 5:
      return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT, "I2C timeout", 5);
    default:
      return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C unknown error");
  }
}

inline uint32_t arduinoNowMs(void*) {
  return millis();
}

inline void arduinoYield(void*) {
  yield();
}

inline void* configUser() {
  return &Wire;
}

}  // namespace transport
