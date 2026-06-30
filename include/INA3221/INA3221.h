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

/// @brief Driver state for health monitoring.
enum class DriverState : uint8_t {
  UNINIT,    ///< begin() not called or end() called
  READY,     ///< Operational, consecutiveFailures == 0
  DEGRADED,  ///< 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    ///< consecutiveFailures >= offlineThreshold
};

/// @brief Per-channel converted measurement result.
struct ChannelMeasurement {
  float shuntVoltage_mV = 0.0f;   ///< Shunt voltage in millivolts
  float busVoltage_V = 0.0f;      ///< Bus voltage in volts
  float current_mA = 0.0f;        ///< Load current in milliamps (requires shunt resistance)
  float power_mW = 0.0f;          ///< Power in milliwatts (requires shunt resistance)
};

/// @brief Alert flags snapshot from Mask/Enable register.
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

/// @brief Chunked driver job type.
enum class PollJobKind : uint8_t {
  NONE,             ///< No active chunked job
  SINGLE_SHOT,      ///< Triggered conversion and enabled-channel reads
  CONTINUOUS_READ,  ///< Enabled-channel reads from an active continuous mode
  APPLY_MASK_ENABLE ///< Mask/Enable writable-bit apply
};

/// @brief Current phase of a chunked driver job.
enum class PollJobStage : uint8_t {
  IDLE,              ///< No work scheduled
  WRITE_CONFIG,      ///< One Configuration-register write is pending
  WAIT_CONVERSION,   ///< Waiting for the configured conversion cycle time
  READ_READY,        ///< One Mask/Enable read is pending; clears CVRF/alerts
  READ_CHANNELS,     ///< Per-channel raw register reads are pending
  WRITE_MASK_ENABLE, ///< One Mask/Enable write is pending
  COMPLETE           ///< Job completed and cached results are available
};

/// @brief Raw per-channel sample captured by chunked polling.
struct ChannelRawMeasurement {
  int16_t shuntRaw = 0;       ///< Raw shunt register value in datasheet format
  int16_t busRaw = 0;         ///< Raw bus register value in datasheet format
  bool shuntValid = false;    ///< True if shuntRaw was read for this job
  bool busValid = false;      ///< True if busRaw was read for this job
  bool channelEnabled = false;///< True if the channel was enabled for this job
};

/// @brief Cache-only snapshot of the active chunked job.
struct PollJobSnapshot {
  PollJobKind kind = PollJobKind::NONE;
  PollJobStage stage = PollJobStage::IDLE;
  bool active = false;
  bool complete = false;
  bool pollConversionReady = false;
  bool conversionReady = false;
  Mode sampleMode = Mode::SHUNT_BUS_TRIG;
  uint32_t conversionStartMs = 0;
  uint8_t nextChannel = 0;
  uint8_t lastInstructions = 0;
  uint16_t totalInstructions = 0;
  ChannelRawMeasurement channels[3];
};

/// @brief Snapshot of cached settings and runtime state without I2C access.
struct SettingsSnapshot {
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  uint8_t i2cAddress = 0x40;
  uint32_t i2cTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  bool hasNowMsHook = false;
  bool hasCooperativeYieldHook = false;
  bool ch1Enable = true;
  bool ch2Enable = true;
  bool ch3Enable = true;
  Averaging averaging = Averaging::AVG_1;
  ConvTime vBusCt = ConvTime::CT_1100US;
  ConvTime vShCt = ConvTime::CT_1100US;
  Mode mode = Mode::SHUNT_BUS_CONT;
  float shuntResistance[3] = {0.1f, 0.1f, 0.1f};
  bool conversionStarted = false;
  bool conversionReady = false;
  uint32_t conversionStartMs = 0;
  uint16_t maskEnableWritableCache = 0;
  bool hardwareConfigDirty = false;
  Status hardwareConfigDirtyStatus = Status::Ok();
};

/// @brief Managed synchronous INA3221 driver.
class INA3221 {
public:
  // === Lifecycle ===
  /// Initialize the driver with configuration and verify manufacturer/die IDs.
  Status begin(const Config& config);
  /// Process pending operations (currently bounded single-shot polling only).
  /// @note In triggered modes, once the local conversion delay has elapsed,
  ///       this may read Mask/Enable to inspect CVRF. Per INA3221 register
  ///       semantics, that read clears CVRF and latched alert flags.
  void tick(uint32_t nowMs);
  /// Process pending operations and return any I2C/readiness failure.
  /// @note Same Mask/Enable read-clear side effect as tick().
  Status tickStatus(uint32_t nowMs);
  /// Verified power-down write; the driver remains initialized on success.
  Status powerDown();
  /// Best-effort power the device down and clear cached conversion state.
  /// @note Use powerDown() first when shutdown I2C failures must be observed.
  void end();

  /// Check if begin() completed successfully and end() has not been called.
  bool isInitialized() const { return _initialized; }

  /// Get the cached configuration snapshot currently owned by the driver.
  const Config& getConfig() const { return _config; }

  // === Diagnostics (no health tracking) ===
  /// Check device presence and identity without updating health counters.
  /// @note Raw transport failures are returned unchanged; DEVICE_NOT_FOUND is
  ///       returned only when the transport callback reports it.
  Status probe();

  /// Re-validate IDs, clear conversion state, and re-apply cached configuration.
  Status recover();

  /// Populate a cache-only settings snapshot without touching I2C.
  Status getSettings(SettingsSnapshot& out) const;

  // === Driver State ===
  DriverState state() const { return _driverState; }
  DriverState driverState() const { return state(); }
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
  bool hardwareConfigDirty() const { return _hardwareConfigDirty; }
  Status hardwareConfigDirtyStatus() const { return _hardwareConfigDirtyStatus; }

  // === Measurement API ===
  /// Measurement reads require the channel to be enabled and the current mode
  /// to include the requested quantity; otherwise INVALID_CONFIG is returned
  /// before touching I2C.
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
  /// @note Convenience-only: this hides Status detail and can still perform a
  ///       tracked Mask/Enable read with the same read-clear side effects.
  bool conversionReady();
  /// Schedule a chunked single-shot job using the current triggered mode, or
  /// SHUNT_BUS_TRIG if the current mode is not triggered.
  /// @param pollConversionReady If true, pollSingleShot() reads Mask/Enable
  ///        after the delay gate; that read clears CVRF and latched alerts.
  Status startSingleShot(bool pollConversionReady = true);
  /// Schedule a chunked single-shot job with an explicit triggered mode.
  /// @param mode SHUNT_TRIG, BUS_TRIG, or SHUNT_BUS_TRIG.
  /// @param pollConversionReady If true, pollSingleShot() reads Mask/Enable
  ///        after the delay gate; that read clears CVRF and latched alerts.
  Status startSingleShot(Mode mode, bool pollConversionReady = true);
  /// Advance a chunked single-shot job by at most maxInstructions register
  /// transfers. One 16-bit register read or write is one instruction.
  Status pollSingleShot(uint32_t nowMs, uint8_t maxInstructions);
  /// Schedule a chunked read from the current continuous mode.
  /// @param pollConversionReady If true, pollContinuousRead() first reads
  ///        Mask/Enable; that read clears CVRF and latched alerts.
  Status startContinuousRead(bool pollConversionReady = false);
  /// Advance a chunked continuous-read job by at most maxInstructions register
  /// transfers. One 16-bit register read is one instruction.
  Status pollContinuousRead(uint32_t nowMs, uint8_t maxInstructions);
  /// Advance whichever chunked job is active.
  Status pollJob(uint32_t nowMs, uint8_t maxInstructions);
  /// Read one pending raw register for a channel in the active sample job.
  Status readChannelRawStep(Channel ch);
  /// Get cached raw results from the last or active chunked sample job.
  Status getPollJobSnapshot(PollJobSnapshot& out) const;
  /// Convenience-only helper that may start a conversion, poll repeatedly,
  /// yield cooperatively, clear Mask/Enable flags through readiness polling,
  /// and read multiple channel registers in one call. Keep explicit staged
  /// operations in deadline-owned steady polling paths.
  /// @return INVALID_PARAM if no channel output pointer is provided or the
  ///         timeout is too large for wrap-safe local deadline math.
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
  /// @note Runtime setter; pre-begin shunt values belong in Config.
  Status setShuntResistance(Channel ch, float ohms);
  float getShuntResistance(Channel ch) const;

  Status readConfig(uint16_t& config);
  /// Write raw Configuration register and synchronize cached Config.
  /// @note Triggered mode bits start and track a single-shot conversion.
  Status writeConfig(uint16_t config);
  Status softReset();

  // === Alert Limits ===
  /// @brief Set a critical shunt-voltage alert limit.
  /// @param ch Channel.
  /// @param raw Datasheet-format raw threshold; reserved bits are cleared before write.
  /// @return Status from validation and register write.
  Status setCriticalAlertLimit(Channel ch, int16_t raw);

  /// @brief Read a critical shunt-voltage alert limit.
  /// @param ch Channel.
  /// @param raw Raw threshold register value.
  /// @return Status from validation and register read.
  Status getCriticalAlertLimit(Channel ch, int16_t& raw);

  /// @brief Set a warning shunt-voltage alert limit.
  /// @param ch Channel.
  /// @param raw Datasheet-format raw threshold; reserved bits are cleared before write.
  /// @return Status from validation and register write.
  Status setWarningAlertLimit(Channel ch, int16_t raw);

  /// @brief Read a warning shunt-voltage alert limit.
  /// @param ch Channel.
  /// @param raw Raw threshold register value.
  /// @return Status from validation and register read.
  Status getWarningAlertLimit(Channel ch, int16_t& raw);

  /// @brief Set shunt-voltage summation alert limit.
  /// @param raw Datasheet-format raw threshold; reserved bit 0 is cleared before write.
  /// @return Status from register write.
  Status setShuntSumLimit(int16_t raw);

  /// @brief Read shunt-voltage summation alert limit.
  /// @param raw Raw threshold register value.
  /// @return Status from register read.
  Status getShuntSumLimit(int16_t& raw);

  /// @brief Set power-valid upper bus-voltage limit.
  /// @param raw Datasheet-format raw threshold; reserved bits are cleared before write.
  /// @return Status from register write.
  Status setPowerValidUpperLimit(int16_t raw);

  /// @brief Read power-valid upper bus-voltage limit.
  /// @param raw Raw threshold register value.
  /// @return Status from register read.
  Status getPowerValidUpperLimit(int16_t& raw);

  /// @brief Set power-valid lower bus-voltage limit.
  /// @param raw Datasheet-format raw threshold; reserved bits are cleared before write.
  /// @return Status from register write.
  Status setPowerValidLowerLimit(int16_t raw);

  /// @brief Read power-valid lower bus-voltage limit.
  /// @param raw Raw threshold register value.
  /// @return Status from register read.
  Status getPowerValidLowerLimit(int16_t& raw);

  // === Mask/Enable Register ===
  /// @brief Read and decode Mask/Enable alert flags.
  /// @param flags Decoded alert flags.
  /// @return Status from register read.
  /// @note Reading Mask/Enable clears latched alert and conversion-ready flags.
  Status readAlertFlags(AlertFlags& flags);
  /// @brief Explicitly read Mask/Enable and clear latched flags/CVRF.
  Status readAndClearAlertFlags(AlertFlags& flags);

  /// @brief Configure channels included in shunt-voltage summation.
  /// @param ch1 Include channel 1.
  /// @param ch2 Include channel 2.
  /// @param ch3 Include channel 3.
  /// @return Status from register write.
  Status setSummationChannels(bool ch1, bool ch2, bool ch3);

  /// @brief Configure warning and critical alert latch behavior.
  /// @param warningLatch true latches warning alerts.
  /// @param criticalLatch true latches critical alerts.
  /// @return Status from register write.
  Status setAlertLatchEnable(bool warningLatch, bool criticalLatch);
  /// @brief Schedule one Mask/Enable writable-bit register write.
  /// @param writableBits New SCC/WEN/CEN bits; read-only flags are masked out.
  Status startApplyMaskEnable(uint16_t writableBits);
  /// @brief Advance the scheduled Mask/Enable write by instruction budget.
  Status pollApplyMaskEnable(uint32_t nowMs, uint8_t maxInstructions);

  // === Device Identification ===
  Status readManufacturerId(uint16_t& id);
  Status readDieId(uint16_t& id);

  // === Raw Register Access ===
  /// Read a 16-bit register using tracked I2C access.
  /// @note Reading REG_MASK_ENABLE clears latched alert and conversion-ready
  ///       flags per INA3221 register semantics.
  Status readRegister16(uint8_t reg, uint16_t& value);
  /// Write a 16-bit register using tracked I2C access.
  /// @note Diagnostic raw writes bypass typed cache helpers. Writes to
  ///       Configuration or Mask/Enable mark hardwareConfigDirty().
  Status writeRegister16(uint8_t reg, uint16_t value);

  // === Utility ===
  static float shuntRawToMv(int16_t raw);
  static float busRawToVolts(int16_t raw);
  static int16_t mvToShuntRaw(float mV);
  static int16_t voltsToBusRaw(float volts);
  uint32_t getConversionTimeUs() const;
  uint32_t getCycleTimeUs() const;

private:
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
  void _markHardwareConfigDirty(const Status& reason);
  void _clearHardwareConfigDirty();

  // === Internal ===
  Status _applyConfig();
  Status _readConversionReadyAt(uint32_t nowMs, bool& ready);
  Status _ensureMeasurementReadyForRead();
  void _handleConfigWriteSideEffects();
  void _clearPollJob();
  void _resetPollSampleCache();
  Status _startSampleJob(PollJobKind kind, Mode mode, bool pollConversionReady);
  Status _pollSampleJob(uint32_t nowMs, uint8_t maxInstructions);
  Status _pollApplyMaskEnableJob(uint8_t maxInstructions);
  Status _readChannelRawStep(Channel ch);
  uint16_t _buildConfigRegister() const;
  uint16_t _buildConfigRegisterForMode(Mode mode) const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;

  uint8_t _enabledChannelCount() const;
  bool _isChannelEnabled(Channel ch) const;
  bool _isTriggeredMode() const;
  bool _isContinuousMode() const;
  bool _sampleModeReadsShunt() const;
  bool _sampleModeReadsBus() const;
  bool _sampleChannelComplete(Channel ch) const;

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
  bool _hardwareConfigDirty = false;
  Status _hardwareConfigDirtyStatus = Status::Ok();

  // === Conversion State ===
  bool _conversionStarted = false;
  bool _conversionReady = false;
  uint32_t _conversionStartMs = 0;
  uint16_t _maskEnableWritableCache = 0;

  // === Chunked Poll Job State ===
  PollJobKind _pollJobKind = PollJobKind::NONE;
  PollJobStage _pollJobStage = PollJobStage::IDLE;
  Mode _pollSampleMode = Mode::SHUNT_BUS_TRIG;
  bool _pollConversionReady = false;
  bool _pollObservedReady = false;
  uint32_t _pollConversionStartMs = 0;
  uint8_t _pollNextChannel = 0;
  uint8_t _pollInstructionsLast = 0;
  uint16_t _pollInstructionsTotal = 0;
  uint16_t _pendingMaskEnableWritable = 0;
  ChannelRawMeasurement _pollChannels[3];
};

} // namespace INA3221
