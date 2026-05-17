/// @file Config.h
/// @brief Configuration structure for INA3221 driver
#pragma once

#include <cstddef>
#include <cstdint>
#include "INA3221/Status.h"

namespace INA3221 {

/// @brief I2C write callback signature.
/// @param addr     I2C device address (7-bit)
/// @param data     Pointer to data to write
/// @param len      Number of bytes to write
/// @param timeoutMs Maximum time to wait for completion
/// @param user     User context pointer passed through from Config
/// @return Status indicating success or failure
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// @brief I2C write-then-read callback signature.
/// @param addr     I2C device address (7-bit)
/// @param txData   Pointer to data to write
/// @param txLen    Number of bytes to write
/// @param rxData   Pointer to buffer for read data
/// @param rxLen    Number of bytes to read
/// @param timeoutMs Maximum time to wait for completion
/// @param user     User context pointer passed through from Config
/// @return Status indicating success or failure
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* txData, size_t txLen,
                                  uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// @brief Millisecond timestamp callback.
/// @param user User context pointer passed through from Config
/// @return Current monotonic milliseconds
using NowMsFn = uint32_t (*)(void* user);

/// @brief Cooperative yield callback.
/// @param user User context pointer passed through from Config
using YieldFn = void (*)(void* user);

/// @brief Averaging mode (number of samples for recursive averaging).
enum class Averaging : uint8_t {
  AVG_1    = 0,  ///< 1 sample (no averaging, default)
  AVG_4    = 1,  ///< 4 samples
  AVG_16   = 2,  ///< 16 samples
  AVG_64   = 3,  ///< 64 samples
  AVG_128  = 4,  ///< 128 samples
  AVG_256  = 5,  ///< 256 samples
  AVG_512  = 6,  ///< 512 samples
  AVG_1024 = 7   ///< 1024 samples
};

/// @brief Conversion time for bus or shunt voltage.
enum class ConvTime : uint8_t {
  CT_140US  = 0,  ///< 140 µs
  CT_204US  = 1,  ///< 204 µs
  CT_332US  = 2,  ///< 332 µs
  CT_588US  = 3,  ///< 588 µs
  CT_1100US = 4,  ///< 1.1 ms (default)
  CT_2116US = 5,  ///< 2.116 ms
  CT_4156US = 6,  ///< 4.156 ms
  CT_8244US = 7   ///< 8.244 ms
};

/// @brief Operating mode.
enum class Mode : uint8_t {
  POWER_DOWN      = 0,  ///< Power-down
  SHUNT_TRIG      = 1,  ///< Shunt voltage, single-shot
  BUS_TRIG        = 2,  ///< Bus voltage, single-shot
  SHUNT_BUS_TRIG  = 3,  ///< Shunt + bus, single-shot (default for triggered)
  POWER_DOWN_ALT  = 4,  ///< Power-down (alternate)
  SHUNT_CONT      = 5,  ///< Shunt voltage, continuous
  BUS_CONT        = 6,  ///< Bus voltage, continuous
  SHUNT_BUS_CONT  = 7   ///< Shunt + bus, continuous (default)
};

/// @brief Channel index.
enum class Channel : uint8_t {
  CH1 = 0,  ///< Channel 1
  CH2 = 1,  ///< Channel 2
  CH3 = 2   ///< Channel 3
};

/// @brief Configuration for INA3221 driver.
struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;

  // === Timing Hooks (optional) ===
  NowMsFn nowMs = nullptr;                 ///< Monotonic millisecond source
  YieldFn cooperativeYield = nullptr;      ///< Cooperative scheduler hint
  void* timeUser = nullptr;                ///< User context for timing hooks

  // === Device Settings ===
  uint8_t i2cAddress = 0x40;       ///< 0x40-0x43 based on A0 pin
  uint32_t i2cTimeoutMs = 50;      ///< I2C transaction timeout in ms

  // === Channel Enable (default: all enabled) ===
  bool ch1Enable = true;           ///< Enable channel 1
  bool ch2Enable = true;           ///< Enable channel 2
  bool ch3Enable = true;           ///< Enable channel 3

  // === Conversion Settings ===
  Averaging averaging = Averaging::AVG_1;        ///< Averaging mode
  ConvTime vBusCt = ConvTime::CT_1100US;         ///< Bus voltage conversion time
  ConvTime vShCt = ConvTime::CT_1100US;          ///< Shunt voltage conversion time
  Mode mode = Mode::SHUNT_BUS_CONT;             ///< Operating mode

  // === Shunt Resistor Values (ohms) ===
  float shuntResistance[3] = {0.1f, 0.1f, 0.1f}; ///< Shunt resistor per channel (ohms)

  // === Health Tracking ===
  uint8_t offlineThreshold = 5;    ///< Consecutive failures before OFFLINE
};

} // namespace INA3221
