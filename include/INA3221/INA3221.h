/// @file INA3221.h
/// @brief Main driver class for INA3221
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

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

/// @brief Verification state for one controlled hardware-register family.
enum class AppliedConfigState : uint8_t {
  UNKNOWN, ///< Hardware effect or retained state is not known
  APPLIED, ///< Desired state was verified by readback
  DIRTY    ///< Desired state is known but has not been fully applied/verified
};

/// @brief Datasheet typical and maximum time for one ADC conversion.
struct ConversionTiming {
  uint32_t typicalUs = 0;
  uint32_t maximumUs = 0;
};

/// @brief Decoded and retained Mask/Enable evidence.
struct AlertSnapshot {
  uint16_t raw = 0;               ///< Raw value from the consuming read
  uint16_t events = 0;            ///< OR-latched destructive event bits
  uint16_t writableBits = 0;      ///< SCC/WEN/CEN bits from the latest read
  bool powerValid = false;        ///< Latest condition-level PVF value
  bool timingControl = false;     ///< Sticky TCF observation
  bool conversionReady = false;   ///< CVRF in the consuming read
  bool evidenceUncertain = false; ///< Failed read may have cleared unknown events
};

enum class Quantity : uint8_t {
  SHUNT = 0x01,
  BUS = 0x02,
  CURRENT = 0x04,
  POWER = 0x08
};

using QuantityMask = uint8_t;

/// @brief Fixed-unit reading for one physical channel.
struct FixedChannelReading {
  int32_t shuntMicroVolts = 0;
  int32_t busMilliVolts = 0;
  int32_t currentMilliAmps = 0;
  int32_t powerMilliWatts = 0;
  QuantityMask validQuantities = 0;
};

enum class SampleCoherence : uint8_t {
  TRIGGERED_ATOMIC,
  CONTINUOUS_MIXED_AGE
};

/// @brief Atomically committed fixed-capacity sample result.
struct SampleBatch {
  FixedChannelReading channels[3];
  ChannelMask enabledChannels = 0;
  ChannelMask validChannels = 0;
  AlertSnapshot alerts{};
  bool alertSnapshotValid = false; ///< True only when this sample consumed Mask/Enable
  SampleCoherence coherence = SampleCoherence::TRIGGERED_ATOMIC;
  uint64_t captureUptimeMs = 0;
  uint32_t profileGeneration = 0;
  uint32_t requestId = 0;
};

enum class JobKind : uint8_t {
  NONE,
  INITIALIZE,
  APPLY_PROFILE,
  RECONCILE,
  TRIGGERED_SAMPLE,
  CONTINUOUS_SAMPLE,
  POWER_DOWN
};

enum class JobTerminalState : uint8_t {
  IDLE,
  ACTIVE,
  SUCCEEDED,
  FAILED,
  CANCELLED,
  TIMED_OUT,
  PARTIAL,
  INDETERMINATE
};

/// @brief Stable externally observable cooperative-operation stage.
enum class JobStage : uint8_t {
  IDLE,
  READ_IDENTITY,
  READ_PROFILE,
  WRITE_PROFILE,
  VERIFY_PROFILE,
  TRIGGER_SAMPLE,
  WAIT_CONVERSION,
  READ_ALERTS,
  READ_CHANNELS,
  READ_POWER_STATE,
  WRITE_POWER_STATE,
  VERIFY_POWER_STATE,
  TERMINAL
};

enum class HardwareEffect : uint8_t {
  NONE,
  CONFIRMED,
  PARTIAL,
  INDETERMINATE
};

/// @brief Caller-owned per-poll scheduling and transport budget.
struct PollContext {
  uint64_t nowMs = 0;
  uint64_t deadlineMs = 0;       ///< Absolute deadline; 0 uses job deadline
  uint32_t transferTimeoutMs = 0;///< Per-attempt cap, never above transport default
  uint8_t maxTransfers = 1;      ///< Remaining deadline is divided across this budget
};

/// @brief Take-once terminal result for every cooperative operation.
struct JobResult {
  JobKind kind = JobKind::NONE;
  JobTerminalState state = JobTerminalState::IDLE;
  HardwareEffect hardwareEffect = HardwareEffect::NONE;
  Status status = Status::Ok();
  uint32_t requestId = 0;
  uint16_t transfers = 0;
  uint32_t profileGeneration = 0;
  bool sampleValid = false;
  SampleBatch sample{};
};

/// @brief Cache-only progress snapshot; performs no transport work.
struct JobProgress {
  JobKind kind = JobKind::NONE;
  JobStage stage = JobStage::IDLE;
  JobTerminalState state = JobTerminalState::IDLE;
  uint32_t requestId = 0;
  uint16_t totalTransfers = 0;
  uint8_t lastPollTransfers = 0;
  uint64_t deadlineMs = 0;
  uint64_t readyAtMs = 0; ///< 0 means poll once with a fresh nowMs to arm the wait
  bool resultPending = false;
};

static_assert(std::is_trivially_copyable<DeviceProfile>::value,
              "DeviceProfile must remain fixed and trivially copyable");
static_assert(std::is_trivially_copyable<SampleBatch>::value,
              "SampleBatch must remain fixed and trivially copyable");
static_assert(std::is_trivially_copyable<JobResult>::value,
              "JobResult must remain fixed and trivially copyable");

/// @brief Managed INA3221 driver with a cooperative external-owner path.
///
/// The object is single-owner, non-copyable, non-reentrant and not ISR-safe.
/// The caller must serialize every method and keep all transport callbacks in
/// that same ownership context. The library owns no task, lock, bus, retry,
/// recovery policy, scheduling policy or deadline renewal.
class INA3221 {
public:
  INA3221() = default;
  ~INA3221() = default; ///< Bus-silent; call an explicit power-down operation if needed.
  INA3221(const INA3221&) = delete;
  INA3221& operator=(const INA3221&) = delete;
  INA3221(INA3221&&) = delete;
  INA3221& operator=(INA3221&&) = delete;

  /// @name Owner-safe cooperative production API
  /// @{
  /// Validate and store a non-owning transport and complete profile without I2C.
  Status bind(const TransportConfig& transport, const DeviceProfile& profile);
  /// Bus-silent cancellation, result discard and binding release.
  void unbind();
  bool isBound() const { return _bound; }
  const TransportConfig& transportConfig() const { return _transport; }
  const DeviceProfile& deviceProfile() const { return _profile; }
  AppliedConfigState measurementConfigState() const { return _measurementConfigState; }
  AppliedConfigState alertConfigState() const { return _alertConfigState; }
  uint32_t profileGeneration() const { return _profileGeneration; }

  /// Request IDs must be non-zero. Reuse is allowed only after the preceding
  /// terminal result has been taken; a pending result blocks every new job.
  Status startInitialize(uint32_t requestId, uint64_t deadlineMs);
  /// Apply a new profile after initialization. The physical I2C address cannot
  /// be changed by a register write; use bind() again for another address.
  Status startApplyProfile(const DeviceProfile& profile, uint32_t requestId,
                           uint64_t deadlineMs);
  Status startReconcile(uint32_t requestId, uint64_t deadlineMs);
  /// Start an atomic triggered sample. The first poll may write the trigger;
  /// a subsequent strictly later nowMs arms the maximum conversion wait.
  /// A deadline that cannot contain that wait and the success callback bounds
  /// is rejected before the trigger write.
  Status startTriggeredSample(Mode mode, uint32_t requestId,
                              uint64_t deadlineMs);
  Status startContinuousSample(uint32_t requestId, uint64_t deadlineMs,
                               bool consumeAlertSnapshot = false);
  Status startPowerDown(uint32_t requestId, uint64_t deadlineMs);
  Status pollJob(const PollContext& context);
  /// Cancel without I2C. Partial sample work is discarded.
  Status cancelJob();
  Status getJobProgress(JobProgress& out) const;
  /// Take a terminal result exactly once.
  Status takeJobResult(JobResult& out);
  /// Cache-only last-good sample access; never exposes partial work.
  Status peekLastSample(SampleBatch& out) const;
  /// Cache-only destructive-event access.
  Status peekAlertEvents(AlertSnapshot& out) const;
  Status takeAlertEvents(AlertSnapshot& out);
  /// @}

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
  /// Schedule a chunked single-shot job using the verified triggered profile.
  /// @param pollConversionReady Retained for source compatibility and ignored;
  ///        production safety always checks CVRF through Mask/Enable.
  /// @return INVALID_CONFIG unless the verified profile is already triggered.
  /// @note Compatibility-only. Prefer startTriggeredSample() with an explicit
  ///       absolute owner deadline and 64-bit PollContext time.
  Status startSingleShot(bool pollConversionReady = true);
  /// Schedule a chunked single-shot job with an explicit triggered mode.
  /// @param mode Must exactly match the verified triggered profile mode.
  /// @param pollConversionReady Retained for source compatibility and ignored;
  ///        production safety always checks CVRF through Mask/Enable, clearing
  ///        CVRF/latched alerts while retaining their evidence.
  Status startSingleShot(Mode mode, bool pollConversionReady = true);
  /// Advance a chunked single-shot job by at most maxInstructions register
  /// transfers. One 16-bit register read or write is one instruction.
  /// The 32-bit time input is extended across one wrap while this job is active.
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
  /// Compatibility placeholder; manual channel stepping is unavailable and
  /// returns JOB_BUSY without I2C. Use the typed poll method instead.
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
  /// @brief Schedule complete read/conditional-write/verify reconciliation
  /// after replacing the desired Mask/Enable writable bits.
  /// @param writableBits New SCC/WEN/CEN bits; read-only flags are masked out.
  Status startApplyMaskEnable(uint16_t writableBits);
  /// @brief Advance the scheduled complete profile reconciliation by budget.
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
  static Status conversionTiming(ConvTime ct, ConversionTiming& out);
  static Status maximumCycleTimeUs(const DeviceProfile& profile, Mode mode,
                                   uint64_t& outUs);
  static Status decodeShuntMicroVolts(uint16_t raw, int32_t& outMicroVolts);
  static Status decodeBusMilliVolts(uint16_t raw, int32_t& outMilliVolts);
  /// Convert a checked channel enum to its fixed array index.
  static Status channelIndex(Channel channel, uint8_t& outIndex);
  /// Convert a checked channel enum to its CH1-through-CH3 mask bit.
  static Status channelBit(Channel channel, ChannelMask& outBit);
  /// Check mask membership after validating both mask and channel.
  static Status contains(ChannelMask mask, Channel channel, bool& outContains);
  /// Count enabled channels after rejecting bits outside CH1 through CH3.
  static Status enabledChannelCount(ChannelMask mask, uint8_t& outCount);
  static Status encodeShuntMicroVolts(int32_t microVolts, uint16_t& outRaw);
  static Status encodeBusMilliVolts(int32_t milliVolts, uint16_t& outRaw);
  static Status encodeShuntSumMicroVolts(int32_t microVolts, uint16_t& outRaw);
  static Status validatePowerValidWindow(uint32_t lowerMilliVolts,
                                         uint32_t upperMilliVolts);
  static Status calculateCurrentMilliAmps(int32_t shuntMicroVolts,
                                           const ShuntCalibration& calibration,
                                           int32_t& outMilliAmps);
  static Status calculatePowerMilliWatts(int32_t busMilliVolts,
                                          int32_t currentMilliAmps,
                                          int32_t& outMilliWatts);
  /// Pure raw-register to fixed-unit conversion with quantity validity.
  /// @param shuntRaw Raw 16-bit shunt-voltage register value.
  /// @param busRaw Raw 16-bit bus-voltage register value.
  /// @param requestedRawQuantities SHUNT and/or BUS; derived CURRENT/POWER are
  ///        produced when their inputs are valid.
  /// @param calibration Explicit shunt calibration used when SHUNT is requested.
  /// @param out Fully replaced fixed-unit result and validity mask.
  static Status convertRawChannel(uint16_t shuntRaw, uint16_t busRaw,
                                  QuantityMask requestedRawQuantities,
                                  const ShuntCalibration& calibration,
                                  FixedChannelReading& out);
  static bool isReadableRegister(uint8_t reg);
  static bool isWritableRegister(uint8_t reg);
  /// Maximum callbacks on the success path when CVRF is set at its first
  /// eligible check. A low CVRF is rechecked no sooner than 1 ms later and is
  /// bounded by the caller's absolute deadline and poll cadence.
  static Status maximumJobTransfers(JobKind kind, const DeviceProfile& profile,
                                    uint16_t& outTransfers);
  uint32_t getConversionTimeUs() const;
  uint32_t getCycleTimeUs() const;

private:
  enum class OperationStage : uint8_t {
    IDLE,
    READ_MANUFACTURER_ID,
    READ_DIE_ID,
    PROFILE_READ,
    PROFILE_WRITE,
    PROFILE_VERIFY,
    SAMPLE_WRITE_CONFIG,
    SAMPLE_WAIT_ORIGIN,
    SAMPLE_WAIT,
    SAMPLE_READ_MASK,
    SAMPLE_READ_CHANNELS,
    POWER_READ_CONFIG,
    POWER_WRITE_CONFIG,
    POWER_VERIFY_CONFIG
  };

  static constexpr uint8_t MANAGED_PROFILE_REGISTER_COUNT = 11;
  static constexpr uint32_t CONVERSION_WAKE_MARGIN_US = 100;
  static constexpr uint32_t COMPATIBILITY_SCHEDULING_MARGIN_MS = 1000;

  Status _validateTransport(const TransportConfig& transport) const;
  static Status _validateProfile(const DeviceProfile& profile);
  Status _startJob(JobKind kind, uint32_t requestId, uint64_t deadlineMs);
  Status _startProfileJob(JobKind kind, const DeviceProfile& profile,
                          uint32_t requestId, uint64_t deadlineMs);
  Status _pollProfileJob(const PollContext& context, uint8_t& transfersLeft);
  Status _pollSampleOperation(const PollContext& context, uint8_t& transfersLeft);
  Status _pollPowerDownOperation(const PollContext& context, uint8_t& transfersLeft);
  Status _jobReadRegister(uint8_t reg, uint16_t& value,
                          const PollContext& context, uint8_t& transfersLeft);
  Status _jobWriteRegister(uint8_t reg, uint16_t value,
                           const PollContext& context, uint8_t& transfersLeft);
  Status _readRegister16WithTimeout(uint8_t reg, uint16_t& value,
                                    uint32_t timeoutMs, bool tracked);
  Status _writeRegister16WithTimeout(uint8_t reg, uint16_t value,
                                     uint32_t timeoutMs, bool tracked);
  Status _readMaskEnableWithTimeout(uint16_t& value, AlertSnapshot* consumed,
                                    uint32_t timeoutMs, bool tracked);
  void _retainMaskEnable(uint16_t raw, AlertSnapshot* consumed);
  static void _decodeAlertFlags(uint16_t raw, AlertFlags& flags);
  uint32_t _clampedTransferTimeout(const PollContext& context) const;
  bool _deadlineExpired(const PollContext& context) const;
  uint64_t _effectiveDeadline(const PollContext& context) const;
  Status _desiredRegister(uint8_t index, const DeviceProfile& profile,
                          uint8_t& reg, uint16_t& value) const;
  static bool _registerMatches(uint8_t reg, uint16_t actual, uint16_t desired);
  static bool _registerIsMeasurementConfig(uint8_t reg);
  static bool _ambiguousWriteFailure(const Status& status);
  void _markRegisterUnknown(uint8_t reg);
  void _markRegisterDirty(uint8_t reg);
  void _finishJob(JobTerminalState state, const Status& status,
                  HardwareEffect effect);
  void _finishJobSuccess();
  void _resetOperationState();
  Status _requireNoOwnerJob() const;
  void _syncLegacyConfigFromProfile();
  static Status _legacyToContracts(const Config& config,
                                   TransportConfig& transport,
                                   DeviceProfile& profile);
  Status _driveCompatibilityJob(uint32_t maximumTransfers);
  Status _compatibilityDurationMs(JobKind kind,
                                  const DeviceProfile& profile, Mode mode,
                                  uint64_t& outMs) const;
  Status _prepareCompatibilityPoll(uint32_t nowMs, uint8_t maxTransfers,
                                   PollContext& out);
  Status _buildFixedReading(uint8_t index, Mode mode,
                            FixedChannelReading& out) const;
  static int64_t _roundDivide(int64_t numerator, int64_t denominator);

  // === Transport Wrappers ===
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen, uint32_t timeoutMs);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len, uint32_t timeoutMs);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                              uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                              uint8_t* rxBuf, size_t rxLen,
                              uint32_t timeoutMs);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len,
                          uint32_t timeoutMs);

  // === Register Access ===
  Status _readRegister16Raw(uint8_t reg, uint16_t& value);
  Status _readRegister16Tracked(uint8_t reg, uint16_t& value);
  Status _writeRegister16Tracked(uint8_t reg, uint16_t value);

  // === Health Tracking ===
  Status _updateHealth(const Status& st);
  Status _recordFailure(const Status& st);
  void _markHardwareConfigDirty(const Status& reason);
  void _clearHardwareConfigDirty();

  // === Internal ===
  Status _applyConfig();
  Status _readConversionReadyAt(uint32_t nowMs, bool& ready);
  Status _ensureMeasurementReadyForRead();
  void _handleConfigWriteSideEffects();
  void _handleResetWriteEffect(bool confirmed);
  void _clearPollJob();
  void _resetPollSampleCache();
  uint16_t _buildConfigRegister() const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;

  uint8_t _enabledChannelCount() const;
  bool _isChannelEnabled(Channel ch) const;
  bool _isTriggeredMode() const;
  bool _isContinuousMode() const;

  // === State ===
  TransportConfig _transport{};
  DeviceProfile _profile{};
  DeviceProfile _pendingProfile{};
  bool _bound = false;
  AppliedConfigState _measurementConfigState = AppliedConfigState::UNKNOWN;
  AppliedConfigState _alertConfigState = AppliedConfigState::UNKNOWN;
  uint32_t _profileGeneration = 0;

  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;

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
  bool _pollTimeInitialized = false;
  uint32_t _pollLastNowMs = 0;
  uint64_t _pollExtendedNowMs = 0;
  uint64_t _pollDeadlineDurationMs = 0;
  uint8_t _pollNextChannel = 0;
  uint8_t _pollInstructionsLast = 0;
  uint16_t _pollInstructionsTotal = 0;
  ChannelRawMeasurement _pollChannels[3];

  // === Cooperative owner operation ===
  JobKind _jobKind = JobKind::NONE;
  OperationStage _jobStage = OperationStage::IDLE;
  JobTerminalState _jobState = JobTerminalState::IDLE;
  HardwareEffect _jobHardwareEffect = HardwareEffect::NONE;
  Status _jobStatus = Status::Ok();
  uint32_t _jobRequestId = 0;
  uint64_t _jobDeadlineMs = 0;
  uint16_t _jobTransfers = 0;
  uint8_t _jobTransfersLastPoll = 0;
  uint8_t _jobProfileIndex = 0;
  uint8_t _jobChannelIndex = 0;
  bool _jobReadBusNext = false;
  bool _jobConsumeAlerts = false;
  bool _jobAnyWriteConfirmed = false;
  Mode _jobSampleMode = Mode::SHUNT_BUS_TRIG;
  uint64_t _jobConversionStartMs = 0;
  uint64_t _jobReadyAtMs = 0;
  uint64_t _jobWaitDurationMs = 0;
  uint64_t _jobWaitOriginAfterMs = 0;
  uint16_t _jobDesiredRegisterValue = 0;
  uint8_t _jobDesiredRegisterAddress = 0;
  ChannelRawMeasurement _sampleRaw[3];
  SampleBatch _sampleWork{};
  SampleBatch _lastGoodSample{};
  bool _hasLastGoodSample = false;
  JobResult _pendingJobResult{};
  bool _hasPendingJobResult = false;
  AlertSnapshot _retainedAlerts{};
};

} // namespace INA3221
