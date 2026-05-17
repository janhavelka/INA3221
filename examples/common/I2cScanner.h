/**
 * @file I2cScanner.h
 * @brief Simple I2C bus scanner utility for examples.
 *
 * NOT part of the library API. This is a diagnostic tool for examples.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "INA3221/CommandTable.h"
#include "examples/common/Log.h"

namespace i2c_scanner {

static constexpr uint8_t INA3221_ADDR_MIN = 0x40;
static constexpr uint8_t INA3221_ADDR_MAX = 0x43;

struct Ina3221Identity {
  bool readOk = false;
  uint16_t manufacturerId = 0;
  uint16_t dieId = 0;
};

inline bool isIna3221Address(uint8_t addr) {
  return addr >= INA3221_ADDR_MIN && addr <= INA3221_ADDR_MAX;
}

inline const char* ina3221AddressStrap(uint8_t addr) {
  switch (addr) {
    case 0x40: return "A0=GND";
    case 0x41: return "A0=VS";
    case 0x42: return "A0=SDA";
    case 0x43: return "A0=SCL";
    default:   return "A0=?";
  }
}

inline bool readRegister16(TwoWire& wire, uint8_t addr, uint8_t reg, uint16_t& value) {
  wire.beginTransmission(addr);
  if (wire.write(reg) != 1U) {
    (void)wire.endTransmission(true);
    return false;
  }

  if (wire.endTransmission(false) != 0U) {
    return false;
  }

  if (wire.requestFrom(addr, static_cast<uint8_t>(2)) != 2U) {
    return false;
  }

  if (wire.available() < 2) {
    return false;
  }

  const uint8_t msb = static_cast<uint8_t>(wire.read());
  const uint8_t lsb = static_cast<uint8_t>(wire.read());
  value = (static_cast<uint16_t>(msb) << 8) | lsb;
  return true;
}

inline Ina3221Identity readIna3221Identity(TwoWire& wire, uint8_t addr) {
  Ina3221Identity identity;
  if (!isIna3221Address(addr)) {
    return identity;
  }

  identity.readOk =
      readRegister16(wire, addr, INA3221::cmd::REG_MANUFACTURER_ID, identity.manufacturerId) &&
      readRegister16(wire, addr, INA3221::cmd::REG_DIE_ID, identity.dieId);
  return identity;
}

inline bool isRecognizedIna3221(const Ina3221Identity& identity) {
  return identity.readOk &&
         identity.manufacturerId == INA3221::cmd::MANUFACTURER_ID_VALUE &&
         identity.dieId == INA3221::cmd::DIE_ID_VALUE;
}

/**
 * @brief Attempt to recover a stuck I2C bus by toggling SCL.
 */
inline void recoverBus(int sda, int scl) {
  Wire.end();

  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);

  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    if (digitalRead(sda)) {
      break;
    }
  }

  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);

  Wire.begin(sda, scl);
}

/**
 * @brief Scan I2C bus and print found devices.
 */
inline void scan(TwoWire& wire, uint16_t timeoutMs = 50) {
  LOGI("Scanning I2C bus (timeout=%dms)...", timeoutMs);
  LOG_SERIAL.flush();

#if defined(ARDUINO_ARCH_ESP32)
  wire.setTimeOut(timeoutMs);
#endif

  LOGI("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
  LOG_SERIAL.flush();

  uint8_t count = 0;
  uint8_t ina3221Count = 0;
  for (uint8_t row = 0; row < 8; row++) {
    LOG_SERIAL.printf("%02X: ", row * 16);
    LOG_SERIAL.flush();

    for (uint8_t col = 0; col < 16; col++) {
      uint8_t addr = row * 16 + col;
      if (addr < 0x08 || addr > 0x77) {
        LOG_SERIAL.print("   ");
        continue;
      }

      wire.beginTransmission(addr);
      uint8_t error = wire.endTransmission(true);

      if (error == 0) {
        LOG_SERIAL.printf("%02X ", addr);
        count++;
      } else if (error == 5) {
        LOG_SERIAL.print("TO ");
      } else {
        LOG_SERIAL.print("-- ");
      }

      yield();
      delay(1);
    }
    LOG_SERIAL.println();
    LOG_SERIAL.flush();
  }

  LOGI("Scan complete. Found %d device(s).", count);
  LOG_SERIAL.flush();

  if (count > 0) {
    LOGI("Checking INA3221 identity registers on 0x40-0x43...");
    for (uint8_t addr = INA3221_ADDR_MIN; addr <= INA3221_ADDR_MAX; ++addr) {
      wire.beginTransmission(addr);
      if (wire.endTransmission(true) != 0U) {
        continue;
      }

      const Ina3221Identity identity = readIna3221Identity(wire, addr);
      if (isRecognizedIna3221(identity)) {
        LOGI("  0x%02X: INA3221 recognized (%s, mfg=0x%04X, die=0x%04X)",
             addr,
             ina3221AddressStrap(addr),
             identity.manufacturerId,
             identity.dieId);
        ina3221Count++;
      } else if (identity.readOk) {
        LOGW("  0x%02X: responded, but not INA3221 (%s, mfg=0x%04X, die=0x%04X)",
             addr,
             ina3221AddressStrap(addr),
             identity.manufacturerId,
             identity.dieId);
      } else {
        LOGW("  0x%02X: responded, but identity registers could not be read (%s)",
             addr,
             ina3221AddressStrap(addr));
      }
    }
    LOGI("INA3221 recognized: %d device(s).", ina3221Count);
  }

  if (count > 0) {
    LOGI("Common addresses: 0x40-0x43=INA3221, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280");
  }
}

}  // namespace i2c_scanner
