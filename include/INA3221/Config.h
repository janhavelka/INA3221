/// @file Config.h
/// @brief Transport, device-profile, and legacy configuration contracts.
#pragma once

#include <cstddef>
#include <cstdint>
#include "INA3221/Status.h"

namespace INA3221 {

/// @brief I2C write callback signature.
///
/// The callback is synchronous and represents exactly one physical transfer
/// attempt. Success means all requested bytes were transferred. It must not
/// retry, recover or reconfigure the bus, interleave another bus user, or call
/// back into the INA3221 object. The application owns those policies.
/// @param addr     I2C device address (7-bit)
/// @param data     Pointer to data to write
/// @param len      Number of bytes to write
/// @param timeoutMs Hard maximum time to wait for completion; the callback
///                  must return no later than this bound
/// @param user     User context pointer passed through from TransportConfig or Config
/// @return Status indicating success or failure
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// @brief I2C write-then-read callback signature.
///
/// The callback is synchronous and represents one non-interleaved register
/// pointer write followed by the exact requested read. Repeated-START versus
/// STOP/START is a backend policy supported by the device. Success means both
/// exact lengths completed. There must be no hidden retry, recovery, bus
/// reconfiguration, or driver re-entry.
/// @param addr     I2C device address (7-bit)
/// @param txData   Pointer to data to write
/// @param txLen    Number of bytes to write
/// @param rxData   Pointer to buffer for read data
/// @param rxLen    Number of bytes to read
/// @param timeoutMs Hard maximum time to wait for completion; the callback
///                  must return no later than this bound
/// @param user     User context pointer passed through from TransportConfig or Config
/// @return Status indicating success or failure
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* txData, size_t txLen,
                                  uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// @brief Optional monotonic millisecond timestamp callback.
/// @param user User context pointer passed through from TransportConfig or Config
/// @return Current monotonic milliseconds
/// @note Framework-neutral builds do not call platform time APIs; if unset,
/// health timestamps use 0 and blocking helpers cannot advance from wall time.
using NowMsFn = uint32_t (*)(void* user);

/// @brief Optional cooperative yield callback.
/// @param user User context pointer passed through from TransportConfig or Config
/// @note If unset, no scheduler/yield API is called by the driver core.
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

/// @brief Fixed channel mask using bits 0 through 2 for CH1 through CH3.
using ChannelMask = uint8_t;

static constexpr ChannelMask CHANNEL_1 = 0x01U;    ///< Channel 1 mask bit.
static constexpr ChannelMask CHANNEL_2 = 0x02U;    ///< Channel 2 mask bit.
static constexpr ChannelMask CHANNEL_3 = 0x04U;    ///< Channel 3 mask bit.
static constexpr ChannelMask ALL_CHANNELS = 0x07U; ///< All valid channel mask bits.

/// @brief Sign convention applied after the shunt-voltage measurement.
enum class CurrentDirection : uint8_t {
  POSITIVE_SHUNT_IS_POSITIVE_CURRENT = 0, ///< Preserve the measured shunt sign.
  POSITIVE_SHUNT_IS_NEGATIVE_CURRENT = 1 ///< Invert the measured shunt sign.
};

/// @brief Explicit fixed-unit calibration for one enabled channel.
struct ShuntCalibration {
  uint32_t resistanceMicroOhms = 0; ///< Must be non-zero for enabled channels
  CurrentDirection direction =
      CurrentDirection::POSITIVE_SHUNT_IS_POSITIVE_CURRENT; ///< Host-side sign convention
};

/// @brief Complete desired state for all managed volatile alert registers.
struct AlertProfile {
  int32_t criticalLimitMicroVolts[3] = {163800, 163800, 163800}; ///< Per-channel single-conversion limits
  int32_t warningLimitMicroVolts[3] = {163800, 163800, 163800}; ///< Per-channel averaged limits
  ChannelMask summationChannels = 0; ///< Enabled channels included in shunt summation
  int32_t shuntSumLimitMicroVolts = 655320; ///< Signed shunt-sum alert limit
  uint32_t powerValidUpperMilliVolts = 10000; ///< Upper bus-voltage threshold
  uint32_t powerValidLowerMilliVolts = 9000; ///< Lower bus-voltage threshold
  bool warningLatch = false; ///< Enable latched warning-alert behavior
  bool criticalLatch = false; ///< Enable latched critical-alert behavior
};

/// @brief Complete fixed-size desired device profile.
/// @note The default shunt calibrations are zero, so the default profile is not
/// valid while any channel is enabled. Set resistanceMicroOhms for each enabled
/// channel before passing the profile to INA3221::bind().
struct DeviceProfile {
  uint8_t i2cAddress = 0x40; ///< Seven-bit address; valid range is 0x40 through 0x43
  ChannelMask enabledChannels = ALL_CHANNELS; ///< Enabled CH1-through-CH3 mask
  Averaging averaging = Averaging::AVG_1; ///< Recursive averaging setting
  ConvTime vBusCt = ConvTime::CT_1100US; ///< Bus-voltage conversion time
  ConvTime vShCt = ConvTime::CT_1100US; ///< Shunt-voltage conversion time
  Mode mode = Mode::SHUNT_BUS_CONT; ///< Desired operating mode
  ShuntCalibration shunts[3]{}; ///< Host calibration indexed by Channel
  AlertProfile alerts{}; ///< Complete desired alert configuration
};

/// @brief Non-owning transport and compatibility timing hooks.
struct TransportConfig {
  I2cWriteFn i2cWrite = nullptr; ///< Required single-attempt write callback
  I2cWriteReadFn i2cWriteRead = nullptr; ///< Required single-attempt combined callback
  void* i2cUser = nullptr; ///< Opaque context passed to I2C callbacks
  NowMsFn nowMs = nullptr; ///< Optional legacy monotonic-clock callback
  YieldFn cooperativeYield = nullptr; ///< Optional legacy blocking-helper yield callback
  void* timeUser = nullptr; ///< Opaque context passed to timing callbacks
  uint32_t defaultTransferTimeoutMs = 50; ///< Required non-zero per-attempt ceiling
  uint8_t offlineThreshold = 5; ///< Passive diagnostic threshold only
};

/// @brief Legacy synchronous compatibility configuration.
/// @note New shared-bus integrations should use TransportConfig,
/// DeviceProfile, and the cooperative owner API instead.
struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr; ///< Required single-attempt write callback
  I2cWriteReadFn i2cWriteRead = nullptr; ///< Required combined-transfer callback
  void* i2cUser = nullptr; ///< Opaque context passed to I2C callbacks

  // === Timing Hooks (optional) ===
  NowMsFn nowMs = nullptr;                 ///< Optional monotonic millisecond source
  YieldFn cooperativeYield = nullptr;      ///< Optional cooperative scheduler hint
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
