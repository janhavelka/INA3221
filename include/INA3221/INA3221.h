/// @file INA3221.h
/// @brief Main driver class for INA3221
#pragma once

#include <cstddef>
#include <cstdint>

#include "INA3221/CommandTable.h"
#include "INA3221/Config.h"
#include "INA3221/Status.h"
#include "INA3221/Version.h"

namespace INA3221 {

/// Driver state for health monitoring
enum class DriverState : uint8_t {
  UNINIT,    ///< begin() not called or end() called
  READY,     ///< Operational, consecutiveFailures == 0
  DEGRADED,  ///< 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    ///< consecutiveFailures >= offlineThreshold
};

/// Per-channel measurement result
struct ChannelMeasurement {
  float shuntVoltage_mV = 0.0f;   ///< Shunt voltage in millivolts
  float busVoltage_V = 0.0f;      ///< Bus voltage in volts
  float current_mA = 0.0f;        ///< Load current in milliamps (requires shunt resistance)
  float power_mW = 0.0f;          ///< Power in milliwatts (requires shunt resistance)
};

/// Alert flags snapshot from Mask/Enable register
/// @note Reading Mask/Enable register clears CF1-CF3, SF, WF1-WF3, and CVRF.
struct AlertFlags {
  bool criticalCh1 = false;     ///< CF1: Critical flag Ch1
  bool criticalCh2 = false;     ///< CF2: Critical flag Ch2
  bool criticalCh3 = false;     ///< CF3: Critical flag Ch3
  bool summation = false;       ///< SF: Summation alert flag
  bool warningCh1 = false;      ///< WF1: Warning flag Ch1
  bool warningCh2 = false;      ///< WF2: Warning flag Ch2
  bool warningCh3 = false;      ///< WF3: Warning flag Ch3
  bool powerValid = false;      ///< PVF: Power valid flag
  bool timingControl = false;   ///< TCF: Timing control flag
  bool conversionReady = false; ///< CVRF: Conversion ready flag
};

/// INA3221 driver class
class INA3221 {
public:
  // === Lifecycle ===
  /// Initialize the driver with configuration and verify manufacturer/die IDs.
  Status begin(const Config& config);
  /// Process pending operations (currently bounded polling only).
  void tick(uint32_t nowMs);
  /// Best-effort power the device down and clear cached conversion state.
  void end();

  /// Check if begin() completed successfully and end() has not been called.
  bool isInitialized() const { return _initialized; }

  /// Get the cached configuration snapshot currently owned by the driver.
  const Config& getConfig() const { return _config; }

  // === Diagnostics (no health tracking) ===
  /// Check device presence and identity without updating health counters.
  Status probe();

  /// Re-validate IDs, clear conversion state, and re-apply cached configuration.
  Status recover();

  // === Driver State ===
  DriverState state() const { return _driverState; }
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  // === Health Tracking ===
  uint32_t lastOkMs() const { return _lastOkMs; }
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  Status lastError() const { return _lastError; }
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  uint32_t totalFailures() const { return _totalFailures; }
  uint32_t totalSuccess() const { return _totalSuccess; }

  // === Measurement API ===
  Status readShuntRaw(Channel ch, int16_t& raw);
  Status readBusRaw(Channel ch, int16_t& raw);
  Status readShuntVoltage(Channel ch, float& mV);
  Status readBusVoltage(Channel ch, float& volts);
  Status readCurrent(Channel ch, float& mA);
  Status readPower(Channel ch, float& mW);
  Status readChannel(Channel ch, ChannelMeasurement& out);
  Status readShuntSumRaw(int16_t& raw);
  Status readShuntSumVoltage(float& mV);

  // === Single-Shot Conversion ===
  Status startConversion();
  Status startConversion(Mode mode);
  /// Read conversion-ready state with Status propagation.
  /// @note This reads Mask/Enable and therefore clears CVRF and latched alert flags
  ///       per the INA3221 register semantics.
  Status readConversionReady(bool& ready);
  /// Convenience wrapper around readConversionReady(); returns false on errors.
  bool conversionReady();
  Status readBlocking(ChannelMeasurement* ch1 = nullptr,
                      ChannelMeasurement* ch2 = nullptr,
                      ChannelMeasurement* ch3 = nullptr,
                      uint32_t timeoutMs = 200);

  // === Configuration ===
  /// Set operating mode.
  /// @return IN_PROGRESS when a triggered mode write starts a single-shot conversion.
  Status setMode(Mode mode);
  Mode getMode() const { return _config.mode; }

  Status setAveraging(Averaging avg);
  Averaging getAveraging() const { return _config.averaging; }

  /// Set bus-voltage conversion time.
  Status setVBusConvTime(ConvTime ct);
  /// Get bus-voltage conversion time.
  ConvTime getVBusConvTime() const { return _config.vBusCt; }
  /// Cross-library naming alias for setVBusConvTime().
  Status setVbusConvTime(ConvTime ct) { return setVBusConvTime(ct); }
  /// Cross-library naming alias for getVBusConvTime().
  ConvTime getVbusConvTime() const { return getVBusConvTime(); }

  /// Set shunt-voltage conversion time.
  Status setVShuntConvTime(ConvTime ct);
  /// Get shunt-voltage conversion time.
  ConvTime getVShuntConvTime() const { return _config.vShCt; }
  /// Cross-library naming alias for setVShuntConvTime().
  Status setVshuntConvTime(ConvTime ct) { return setVShuntConvTime(ct); }
  /// Cross-library naming alias for getVShuntConvTime().
  ConvTime getVshuntConvTime() const { return getVShuntConvTime(); }

  Status setChannelEnable(Channel ch, bool enable);
  bool getChannelEnable(Channel ch) const;

  /// Set host-side shunt resistance used for current/power calculations.
  /// @param ch Channel whose shunt resistance is being configured.
  /// @param ohms Must be finite and > 0.
  Status setShuntResistance(Channel ch, float ohms);
  float getShuntResistance(Channel ch) const;

  Status readConfig(uint16_t& config);
  /// Write raw Configuration register and synchronize cached Config.
  /// @note Triggered mode bits start and track a single-shot conversion.
  Status writeConfig(uint16_t config);
  Status softReset();

  // === Alert Limits ===
  Status setCriticalAlertLimit(Channel ch, int16_t raw);
  Status getCriticalAlertLimit(Channel ch, int16_t& raw);
  Status setWarningAlertLimit(Channel ch, int16_t raw);
  Status getWarningAlertLimit(Channel ch, int16_t& raw);
  Status setShuntSumLimit(int16_t raw);
  Status getShuntSumLimit(int16_t& raw);
  Status setPowerValidUpperLimit(int16_t raw);
  Status getPowerValidUpperLimit(int16_t& raw);
  Status setPowerValidLowerLimit(int16_t raw);
  Status getPowerValidLowerLimit(int16_t& raw);

  // === Mask/Enable Register ===
  Status readAlertFlags(AlertFlags& flags);
  Status setSummationChannels(bool ch1, bool ch2, bool ch3);
  Status setAlertLatchEnable(bool warningLatch, bool criticalLatch);

  // === Device Identification ===
  Status readManufacturerId(uint16_t& id);
  Status readDieId(uint16_t& id);

  // === Raw Register Access ===
  /// Read a 16-bit register using tracked I2C access.
  Status readRegister16(uint8_t reg, uint16_t& value);
  /// Write a 16-bit register using tracked I2C access.
  Status writeRegister16(uint8_t reg, uint16_t value);

  // === Utility ===
  static float shuntRawToMv(int16_t raw);
  static float busRawToVolts(int16_t raw);
  static int16_t mvToShuntRaw(float mV);
  static int16_t voltsToBusRaw(float volts);
  uint32_t getConversionTimeUs() const;
  uint32_t getCycleTimeUs() const;

private:
  class ScopedOfflineI2cAllowance;

  // === Transport Wrappers ===
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                              uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len);

  // === Register Access ===
  Status _readRegister16Raw(uint8_t reg, uint16_t& value);
  Status _readRegister16Tracked(uint8_t reg, uint16_t& value);
  Status _writeRegister16Tracked(uint8_t reg, uint16_t value);

  // === Health Tracking ===
  Status _updateHealth(const Status& st);
  Status _recordFailure(const Status& st);
  void _reassertOfflineLatch();

  // === Internal ===
  Status _applyConfig();
  Status _readConversionReadyAt(uint32_t nowMs, bool& ready);
  Status _ensureMeasurementReadyForRead();
  void _handleConfigWriteSideEffects();
  uint16_t _buildConfigRegister() const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;

  uint8_t _enabledChannelCount() const;
  bool _isTriggeredMode() const;
  bool _isContinuousMode() const;

  // === State ===
  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;
  bool _allowOfflineI2c = false;

  // === Health Counters ===
  uint32_t _lastOkMs = 0;
  uint32_t _lastErrorMs = 0;
  Status _lastError = Status::Ok();
  uint8_t _consecutiveFailures = 0;
  uint32_t _totalFailures = 0;
  uint32_t _totalSuccess = 0;

  // === Conversion State ===
  bool _conversionStarted = false;
  bool _conversionReady = false;
  uint32_t _conversionStartMs = 0;
  uint16_t _maskEnableWritableCache = 0;
};

} // namespace INA3221
