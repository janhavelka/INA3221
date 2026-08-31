/// @file INA3221.h
/// @brief Public INA3221 driver types and API.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "INA3221/CommandTable.h"
#include "INA3221/Config.h"
#include "INA3221/Status.h"
#include "INA3221/Version.h"

namespace INA3221 {

/// @brief Passive driver-health state derived from consecutive transport results.
enum class DriverState : uint8_t {
  UNINIT,    ///< No successful initialization, or the object was unbound
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

/// @brief Legacy compatibility polling-job type.
enum class PollJobKind : uint8_t {
  NONE,             ///< No active chunked job
  SINGLE_SHOT,      ///< Triggered conversion and enabled-channel reads
  CONTINUOUS_READ,  ///< Enabled-channel reads from an active continuous mode
  APPLY_MASK_ENABLE ///< Mask/Enable writable-bit apply
};

/// @brief Current phase of a legacy compatibility polling job.
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
  PollJobKind kind = PollJobKind::NONE; ///< Scheduled compatibility job type
  PollJobStage stage = PollJobStage::IDLE; ///< Current compatibility stage
  bool active = false; ///< True while the shared owner engine is active
  bool complete = false; ///< True when cached compatibility results are complete
  bool pollConversionReady = false; ///< True when readiness polling was requested
  bool conversionReady = false; ///< True after CVRF was observed for this job
  Mode sampleMode = Mode::SHUNT_BUS_TRIG; ///< Measurement mode used by the job
  uint32_t conversionStartMs = 0; ///< Extended job start reduced to 32-bit milliseconds
  uint8_t nextChannel = 0; ///< Zero-based index of the next channel to service
  uint8_t lastInstructions = 0; ///< Register transfers used by the previous poll
  uint16_t totalInstructions = 0; ///< Register transfers used by the whole job
  ChannelRawMeasurement channels[3]; ///< Fixed CH1-through-CH3 raw result slots
};

/// @brief Snapshot of cached settings and runtime state without I2C access.
struct SettingsSnapshot {
  bool initialized = false; ///< True after successful initialization
  DriverState state = DriverState::UNINIT; ///< Cached passive health state
  uint8_t i2cAddress = 0x40; ///< Cached seven-bit device address
  uint32_t i2cTimeoutMs = 0; ///< Cached compatibility transfer timeout
  uint8_t offlineThreshold = 0; ///< Cached passive offline threshold
  bool hasNowMsHook = false; ///< True when a legacy monotonic-time hook is configured
  bool hasCooperativeYieldHook = false; ///< True when a legacy yield hook is configured
  bool ch1Enable = true; ///< Cached channel 1 enable state
  bool ch2Enable = true; ///< Cached channel 2 enable state
  bool ch3Enable = true; ///< Cached channel 3 enable state
  Averaging averaging = Averaging::AVG_1; ///< Cached averaging setting
  ConvTime vBusCt = ConvTime::CT_1100US; ///< Cached bus conversion time
  ConvTime vShCt = ConvTime::CT_1100US; ///< Cached shunt conversion time
  Mode mode = Mode::SHUNT_BUS_CONT; ///< Cached operating mode
  float shuntResistance[3] = {0.1f, 0.1f, 0.1f}; ///< Legacy shunt values in ohms
  bool conversionStarted = false; ///< True while a legacy trigger is outstanding
  bool conversionReady = false; ///< Cached legacy conversion-ready observation
  uint32_t conversionStartMs = 0; ///< Legacy conversion start timestamp
  uint16_t maskEnableWritableCache = 0; ///< Cached SCC/WEN/CEN bits
  bool hardwareConfigDirty = false; ///< Compatibility view of configuration uncertainty
  Status hardwareConfigDirtyStatus = Status::Ok(); ///< Reason the compatibility cache is dirty
};

/// @brief Verification state for one controlled hardware-register family.
enum class AppliedConfigState : uint8_t {
  UNKNOWN, ///< Hardware effect or retained state is not known
  APPLIED, ///< Desired state was verified by readback
  DIRTY    ///< Desired state is known but has not been fully applied/verified
};

/// @brief Datasheet typical and maximum time for one ADC conversion.
struct ConversionTiming {
  uint32_t typicalUs = 0; ///< Datasheet typical time in microseconds
  uint32_t maximumUs = 0; ///< Scheduling maximum used by the driver in microseconds
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

/// @brief Bit values describing valid quantities in a fixed-unit reading.
enum class Quantity : uint8_t {
  SHUNT = 0x01,   ///< Shunt voltage is valid
  BUS = 0x02,     ///< Bus voltage is valid
  CURRENT = 0x04, ///< Derived current is valid
  POWER = 0x08    ///< Derived power is valid
};

/// @brief Bitwise combination of Quantity values.
using QuantityMask = uint8_t;

/// @brief Fixed-unit reading for one physical channel.
struct FixedChannelReading {
  int32_t shuntMicroVolts = 0; ///< Signed shunt voltage in microvolts
  int32_t busMilliVolts = 0; ///< Bus voltage in millivolts
  int32_t currentMilliAmps = 0; ///< Signed calibrated current in milliamps
  int32_t powerMilliWatts = 0; ///< Signed host-calculated power in milliwatts
  QuantityMask validQuantities = 0; ///< Bitwise Quantity validity mask
};

/// @brief Temporal relationship between channel values in a SampleBatch.
enum class SampleCoherence : uint8_t {
  TRIGGERED_ATOMIC,     ///< One triggered conversion cycle produced the batch
  CONTINUOUS_MIXED_AGE ///< Sequential reads may observe different conversion ages
};

/// @brief All-or-nothing committed fixed-capacity sample result.
struct SampleBatch {
  FixedChannelReading channels[3]; ///< Fixed CH1-through-CH3 result slots
  ChannelMask enabledChannels = 0; ///< Profile channel mask used for acquisition
  ChannelMask validChannels = 0; ///< Channels whose mode-requested raw and derived quantities are all valid
  AlertSnapshot alerts{}; ///< Alert evidence consumed during this acquisition
  bool alertSnapshotValid = false; ///< True only when this sample consumed Mask/Enable
  SampleCoherence coherence = SampleCoherence::TRIGGERED_ATOMIC; ///< Batch coherence class
  uint64_t captureUptimeMs = 0; ///< Caller monotonic time at completed capture
  uint32_t profileGeneration = 0; ///< Verified profile generation used for capture
  uint32_t requestId = 0; ///< Non-zero request ID supplied at admission
};

/// @brief Cooperative production operation type.
enum class JobKind : uint8_t {
  NONE,              ///< No cooperative operation
  INITIALIZE,        ///< Verify identity and apply the complete profile
  APPLY_PROFILE,     ///< Apply and verify a replacement profile
  RECONCILE,         ///< Reapply and verify the retained desired profile
  TRIGGERED_SAMPLE,  ///< Trigger, wait for, and capture one conversion cycle
  CONTINUOUS_SAMPLE, ///< Capture sequential values from a continuous profile
  POWER_DOWN         ///< Enter and verify power-down mode
};

/// @brief Stable terminal or active state of a cooperative operation.
enum class JobTerminalState : uint8_t {
  IDLE,          ///< No active operation or pending result
  ACTIVE,        ///< Operation admitted and not terminal
  SUCCEEDED,     ///< Operation completed and all required effects were verified
  FAILED,        ///< Operation failed before a confirmed hardware side effect
  CANCELLED,     ///< Caller cancelled without another transport callback
  TIMED_OUT,     ///< Effective absolute deadline expired
  PARTIAL,       ///< Some hardware effect was confirmed before failure
  INDETERMINATE  ///< Hardware acceptance of a failed write is unknown
};

/// @brief Stable externally observable cooperative-operation stage.
enum class JobStage : uint8_t {
  IDLE,               ///< No staged work
  READ_IDENTITY,      ///< Manufacturer or die identity read
  READ_PROFILE,       ///< Managed-register comparison read
  WRITE_PROFILE,      ///< Managed-register update write
  VERIFY_PROFILE,     ///< Managed-register readback verification
  TRIGGER_SAMPLE,     ///< Triggering Configuration-register write
  WAIT_CONVERSION,    ///< Bus-silent conversion wait
  READ_ALERTS,        ///< Destructive Mask/Enable snapshot read
  READ_CHANNELS,      ///< Shunt and/or bus register reads
  READ_POWER_STATE,   ///< Configuration read before power-down
  WRITE_POWER_STATE,  ///< Configuration write for power-down
  VERIFY_POWER_STATE, ///< Configuration readback after power-down
  TERMINAL            ///< A take-once result is pending
};

/// @brief Certainty about register side effects caused by an operation.
enum class HardwareEffect : uint8_t {
  NONE,          ///< No write was confirmed
  CONFIRMED,     ///< All requested writes and verification completed
  PARTIAL,       ///< At least one write completed before a later failure
  INDETERMINATE  ///< A failed write may or may not have reached the device
};

/// @brief Caller-owned per-poll scheduling and transport budget.
struct PollContext {
  uint64_t nowMs = 0;              ///< Current monotonic uptime in milliseconds
  uint64_t deadlineMs = 0;       ///< Absolute deadline; 0 uses job deadline
  uint32_t transferTimeoutMs = 0;///< Per-attempt cap, never above transport default
  uint8_t maxTransfers = 1;      ///< Remaining deadline is divided across this budget
};

/// @brief Take-once terminal result for every cooperative operation.
struct JobResult {
  JobKind kind = JobKind::NONE; ///< Operation that produced this result
  JobTerminalState state = JobTerminalState::IDLE; ///< Terminal outcome
  HardwareEffect hardwareEffect = HardwareEffect::NONE; ///< Write-effect certainty
  Status status = Status::Ok(); ///< Final success or failure status
  uint32_t requestId = 0; ///< Request identity supplied at admission
  uint16_t transfers = 0; ///< Physical callback attempts made by the operation
  uint32_t profileGeneration = 0; ///< Profile generation after terminalization
  bool mismatchValid = false; ///< True when register verification captured a mismatch
  uint8_t mismatchRegister = 0; ///< Register address whose verification failed
  uint16_t mismatchExpected = 0; ///< Desired register value before compare masking
  uint16_t mismatchActual = 0; ///< Read-back register value before compare masking
  uint16_t mismatchMask = 0; ///< Bits used when comparing expected and actual values
  bool sampleValid = false; ///< True only when sample contains a committed batch
  SampleBatch sample{}; ///< Committed batch for successful sample jobs
};

/// @brief Cache-only progress snapshot; performs no transport work.
struct JobProgress {
  JobKind kind = JobKind::NONE; ///< Active or terminal operation type
  JobStage stage = JobStage::IDLE; ///< Stable externally visible stage
  JobTerminalState state = JobTerminalState::IDLE; ///< Current operation state
  uint32_t requestId = 0; ///< Active or terminal request identity
  uint16_t totalTransfers = 0; ///< Callback attempts made by this operation
  uint8_t lastPollTransfers = 0; ///< Callback attempts made by the previous poll
  uint64_t deadlineMs = 0; ///< Absolute job deadline supplied at admission
  uint64_t readyAtMs = 0; ///< 0 means poll once with a fresh nowMs to arm the wait
  bool resultPending = false; ///< True when takeJobResult() can consume a result
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

  /// @brief Validate and store a non-owning transport and complete profile.
  /// @param transport Callback and timeout contract owned by the application.
  /// @param profile Complete desired volatile device state and calibration.
  /// @return OK after a bus-silent bind, or a validation/admission error.
  /// @note Performs no I2C. A successful call clears prior job results, samples,
  /// retained alerts, and profile certainty by first performing unbind().
  Status bind(const TransportConfig& transport, const DeviceProfile& profile);

  /// @brief Release the binding and discard active, pending, and cached state.
  /// @note Performs no I2C and does not power down the physical device.
  void unbind();

  /// @return `true` after bind() and before unbind().
  bool isBound() const { return _bound; }

  /// @return Non-owning reference to the currently bound transport contract.
  const TransportConfig& transportConfig() const { return _transport; }

  /// @return Non-owning reference to the current desired device profile.
  const DeviceProfile& deviceProfile() const { return _profile; }

  /// @return Verification state of measurement-related managed registers.
  AppliedConfigState measurementConfigState() const { return _measurementConfigState; }

  /// @return Verification state of alert-related managed registers.
  AppliedConfigState alertConfigState() const { return _alertConfigState; }

  /// @return Generation counter incremented after a profile is fully verified
  /// and after setShuntResistance() changes host-only calibration, so a cached
  /// SampleBatch can always be matched against the calibration that produced it.
  uint32_t profileGeneration() const { return _profileGeneration; }

  /// @brief Start identity verification and complete profile reconciliation.
  /// @param requestId Non-zero caller identity for progress, result, and samples.
  /// @param deadlineMs Required finite absolute monotonic deadline.
  /// @return IN_PROGRESS on admission, otherwise a validation or admission error.
  /// @note Reuse an ID only after its preceding result has been taken. A pending
  /// result blocks every new job.
  Status startInitialize(uint32_t requestId, uint64_t deadlineMs);

  /// @brief Start application and readback verification of a replacement profile.
  /// @param profile Complete desired profile; its address must match the binding.
  /// @param requestId Non-zero caller identity.
  /// @param deadlineMs Required finite absolute monotonic deadline.
  /// @return IN_PROGRESS on admission, otherwise a validation or admission error.
  /// @note The physical I2C address cannot be changed by a register write; call
  /// unbind() and bind() for another address.
  Status startApplyProfile(const DeviceProfile& profile, uint32_t requestId,
                           uint64_t deadlineMs);

  /// @brief Reapply and verify the retained desired profile.
  /// @param requestId Non-zero caller identity.
  /// @param deadlineMs Required finite absolute monotonic deadline.
  /// @return IN_PROGRESS on admission, otherwise a precondition/admission error.
  Status startReconcile(uint32_t requestId, uint64_t deadlineMs);

  /// @brief Start an all-or-nothing triggered sample acquisition.
  /// @param mode Triggered mode that exactly matches the verified profile.
  /// @param requestId Non-zero caller identity copied into the sample.
  /// @param deadlineMs Required finite absolute monotonic deadline.
  /// @return IN_PROGRESS on admission, otherwise a validation/admission error.
  /// @note The first poll may write the trigger; a subsequent strictly later
  /// nowMs arms the maximum conversion wait. A deadline that cannot contain
  /// that wait and successful-path callback bounds is rejected before writing.
  Status startTriggeredSample(Mode mode, uint32_t requestId,
                              uint64_t deadlineMs);

  /// @brief Start a sequential sample from the verified continuous-mode profile.
  /// @param requestId Non-zero caller identity copied into the sample.
  /// @param deadlineMs Required finite absolute monotonic deadline.
  /// @param consumeAlertSnapshot When true, read and retain Mask/Enable before
  /// channel registers; this destructive read is included in the sample.
  /// @return IN_PROGRESS on admission, otherwise a precondition/admission error.
  Status startContinuousSample(uint32_t requestId, uint64_t deadlineMs,
                               bool consumeAlertSnapshot = false);

  /// @brief Start a read/conditional-write/readback-verify power-down job.
  /// @param requestId Non-zero caller identity.
  /// @param deadlineMs Required finite absolute monotonic deadline.
  /// @return IN_PROGRESS on admission, otherwise a precondition/admission error.
  Status startPowerDown(uint32_t requestId, uint64_t deadlineMs);

  /// @brief Advance the active cooperative job within a strict callback budget.
  /// @param context Caller-owned monotonic time, effective deadline, timeout,
  /// and maximum callback budget for this service call.
  /// @return IN_PROGRESS while work remains, OK on successful terminalization,
  /// or an observable error. Every terminal outcome also publishes JobResult.
  Status pollJob(const PollContext& context);

  /// @brief Cancel the active job without another I2C operation.
  /// @return CANCELLED after publishing a terminal result, or NO_ACTIVE_JOB.
  /// @note Partial sample work is discarded; confirmed earlier writes remain
  /// represented by the result's hardware effect and configuration certainty.
  Status cancelJob();

  /// @brief Copy a cache-only progress snapshot.
  /// @param out Replaced with current progress and pending-result state.
  /// @return Always OK.
  Status getJobProgress(JobProgress& out) const;

  /// @brief Consume a terminal result exactly once.
  /// @param out Replaced with the pending terminal result on success.
  /// @return OK when a result was consumed, otherwise NO_RESULT.
  Status takeJobResult(JobResult& out);

  /// @brief Copy the most recent successfully committed sample without I2C.
  /// @param out Replaced with the last-good sample on success.
  /// @return OK when a completed sample exists, otherwise NO_RESULT.
  /// @note Never exposes partial work.
  Status peekLastSample(SampleBatch& out) const;

  /// @brief Copy retained destructive alert evidence without consuming it.
  /// @param out Replaced with the retained alert snapshot.
  /// @return Always OK; an all-zero snapshot means no retained event evidence.
  Status peekAlertEvents(AlertSnapshot& out) const;

  /// @brief Copy and acknowledge retained destructive alert evidence.
  /// @param out Replaced with the retained snapshot before acknowledgement.
  /// @return Always OK.
  /// @note Clears only retained event bits and evidenceUncertain; condition
  /// levels and the latest raw/writable-bit snapshot remain cached.
  Status takeAlertEvents(AlertSnapshot& out);
  /// @}

  // === Lifecycle ===
  /// @brief Synchronously initialize through the legacy compatibility path.
  /// @param config Complete legacy transport and device configuration.
  /// @return OK after identity/profile verification, otherwise the first error.
  /// @note May perform up to 35 synchronous transport callbacks in one call.
  Status begin(const Config& config);
  /// Process pending operations (currently bounded single-shot polling only).
  /// @note In triggered modes, once the local conversion delay has elapsed,
  ///       this may read Mask/Enable to inspect CVRF. Per INA3221 register
  ///       semantics, that read clears CVRF and latched alert flags.
  /// @param nowMs Current monotonic time in milliseconds.
  void tick(uint32_t nowMs);
  /// Process pending operations and return any I2C/readiness failure.
  /// @param nowMs Current monotonic time in milliseconds.
  /// @return OK when idle/ready, or the observed precondition/transport status.
  /// @note Same Mask/Enable read-clear side effect as tick().
  Status tickStatus(uint32_t nowMs);
  /// @brief Synchronously enter and verify power-down mode.
  /// @return OK after verification, otherwise a precondition/transport error.
  /// @note The driver remains initialized on success.
  Status powerDown();
  /// Bus-silent release of the binding and all cached state; equivalent to
  /// unbind(). The physical device keeps its current configuration and
  /// continues converting.
  /// @note Call powerDown() first when the device must actually be powered
  ///       down and shutdown I2C failures must be observed.
  void end();

  /// @return `true` after successful initialization and before unbind()/end().
  bool isInitialized() const { return _initialized; }

  /// @return Non-owning reference to the cached legacy configuration.
  const Config& getConfig() const { return _config; }

  // === Diagnostics (no health tracking) ===
  /// @brief Check device identity without changing health counters.
  /// @return OK when both identity registers match, otherwise the exact
  /// transport or identity-mismatch status.
  /// @note Raw transport failures are returned unchanged; DEVICE_NOT_FOUND is
  ///       returned only when the transport callback reports it.
  Status probe();

  /// @brief Synchronously reinitialize and verify the retained profile.
  /// @return OK after full verification, otherwise the terminal error.
  Status recover();

  /// @brief Populate a legacy settings snapshot without I2C.
  /// @param out Replaced with cached configuration and runtime state.
  /// @return Always OK.
  Status getSettings(SettingsSnapshot& out) const;

  // === Driver State ===
  /// @return Current passive health state.
  DriverState state() const { return _driverState; }
  /// @return Current passive health state; source-compatible alias for state().
  DriverState driverState() const { return state(); }
  /// @return `true` for READY or DEGRADED; this never controls I2C admission.
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  // === Health Tracking ===
  /// @return Timestamp of the latest successful tracked transfer, or zero.
  uint32_t lastOkMs() const { return _lastOkMs; }
  /// @return Timestamp of the latest failed tracked transfer, or zero.
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  /// @return Most recent tracked transport error, or Status::Ok().
  Status lastError() const { return _lastError; }
  /// @return Failed tracked transfers since the latest tracked success.
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  /// @return Saturating object-lifetime tracked failure count.
  uint32_t totalFailures() const { return _totalFailures; }
  /// @return Saturating object-lifetime tracked success count.
  uint32_t totalSuccess() const { return _totalSuccess; }
  /// @return Compatibility view indicating unverified managed hardware state.
  bool hardwareConfigDirty() const { return _hardwareConfigDirty; }
  /// @return Reason associated with hardwareConfigDirty().
  Status hardwareConfigDirtyStatus() const { return _hardwareConfigDirtyStatus; }

  // === Measurement API ===
  /// Measurement reads require the channel to be enabled and the current mode
  /// to include the requested quantity; otherwise INVALID_CONFIG is returned
  /// before touching I2C.
  /// @param ch Channel to read.
  /// @param raw Receives the signed datasheet-format 16-bit shunt register.
  /// @return Status from precondition, readiness, and register-read checks.
  Status readShuntRaw(Channel ch, int16_t& raw);

  /// @brief Read one channel's raw bus-voltage register.
  /// @param ch Channel to read.
  /// @param raw Receives the signed datasheet-format 16-bit bus register.
  /// @return Status from precondition, readiness, and register-read checks.
  Status readBusRaw(Channel ch, int16_t& raw);

  /// @brief Read and convert one channel's shunt voltage.
  /// @param ch Channel to read.
  /// @param mV Receives signed shunt voltage in millivolts.
  /// @return Status from readShuntRaw().
  Status readShuntVoltage(Channel ch, float& mV);

  /// @brief Read and convert one channel's bus voltage.
  /// @param ch Channel to read.
  /// @param volts Receives bus voltage in volts.
  /// @return Status from readBusRaw().
  Status readBusVoltage(Channel ch, float& volts);

  /// @brief Read shunt voltage and calculate current using legacy calibration.
  /// @param ch Channel to read.
  /// @param mA Receives signed current in milliamps.
  /// @return Status from validation and the shunt-voltage read.
  Status readCurrent(Channel ch, float& mA);

  /// @brief Read bus/shunt voltage and calculate host-side power.
  /// @param ch Channel to read.
  /// @param mW Receives signed power in milliwatts.
  /// @return Status from validation and the two measurement reads.
  Status readPower(Channel ch, float& mW);

  /// @brief Read both raw quantities and calculate all legacy channel values.
  /// @param ch Channel to read.
  /// @param out Replaced with converted voltage, current, and power.
  /// @return Status from validation, readiness, and the two register reads.
  Status readChannel(Channel ch, ChannelMeasurement& out);

  /// @brief Read the raw shunt-voltage summation register.
  /// @param raw Receives the signed datasheet-format sum register.
  /// @return Status from precondition, readiness, and register-read checks.
  Status readShuntSumRaw(int16_t& raw);

  /// @brief Read and convert the shunt-voltage summation register.
  /// @param mV Receives the signed sum in millivolts.
  /// @return Status from readShuntSumRaw().
  Status readShuntSumVoltage(float& mV);

  // === Single-Shot Conversion ===
  /// @brief Trigger the currently configured legacy single-shot mode.
  /// @return IN_PROGRESS after the trigger write, or a validation/transport error.
  Status startConversion();

  /// @brief Select and trigger a legacy single-shot mode.
  /// @param mode SHUNT_TRIG, BUS_TRIG, or SHUNT_BUS_TRIG.
  /// @return IN_PROGRESS after the trigger write, or a validation/transport error.
  Status startConversion(Mode mode);
  /// Read conversion-ready state with Status propagation.
  /// @note This reads Mask/Enable and therefore clears CVRF and latched alert flags
  ///       per the INA3221 register semantics.
  /// @param ready Receives the observed conversion-ready state.
  /// @return OK after a valid observation, or a precondition/transport error.
  /// Before the configured conversion delay has elapsed this returns OK with
  /// `ready` false and performs no I2C.
  Status readConversionReady(bool& ready);
  /// Convenience wrapper around readConversionReady(); returns false on errors.
  /// @note Convenience-only: this hides Status detail and can still perform a
  ///       tracked Mask/Enable read with the same read-clear side effects.
  /// @return `true` only when the readiness read succeeds and CVRF is set.
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
  /// @return IN_PROGRESS on admission, otherwise a validation/admission error.
  Status startSingleShot(Mode mode, bool pollConversionReady = true);
  /// Advance a chunked single-shot job by at most maxInstructions register
  /// transfers. One 16-bit register read or write is one instruction.
  /// The 32-bit time input is extended across one wrap while this job is active.
  /// @param nowMs Current monotonic 32-bit millisecond timestamp.
  /// @param maxInstructions Maximum register transfers for this call; must be non-zero.
  /// @return IN_PROGRESS while active, final job status on completion, or an error.
  Status pollSingleShot(uint32_t nowMs, uint8_t maxInstructions);
  /// Schedule a chunked read from the current continuous mode.
  /// @param pollConversionReady If true, pollContinuousRead() first reads
  ///        Mask/Enable; that read clears CVRF and latched alerts.
  /// @return IN_PROGRESS on admission, otherwise a precondition/admission error.
  Status startContinuousRead(bool pollConversionReady = false);
  /// Advance a chunked continuous-read job by at most maxInstructions register
  /// transfers. One 16-bit register read is one instruction.
  /// @param nowMs Current monotonic 32-bit millisecond timestamp.
  /// @param maxInstructions Maximum register transfers for this call; must be non-zero.
  /// @return IN_PROGRESS while active, final job status on completion, or an error.
  Status pollContinuousRead(uint32_t nowMs, uint8_t maxInstructions);

  /// @brief Advance whichever legacy compatibility job is active.
  /// @param nowMs Current monotonic 32-bit millisecond timestamp.
  /// @param maxInstructions Maximum register transfers for this call.
  /// @return Status from the matching typed compatibility poll method.
  Status pollJob(uint32_t nowMs, uint8_t maxInstructions);
  /// Compatibility placeholder; manual channel stepping is unavailable and
  /// returns JOB_BUSY without I2C. Use the typed poll method instead.
  /// @param ch Ignored legacy channel argument.
  /// @return Always JOB_BUSY.
  Status readChannelRawStep(Channel ch);

  /// @brief Get cached raw results from the last or active compatibility job.
  /// @param out Replaced with compatibility job state and raw channel slots.
  /// @return Always OK.
  Status getPollJobSnapshot(PollJobSnapshot& out) const;
  /// Sentinel selecting a bounded timeout derived from the active profile and
  /// configured per-transfer ceiling.
  static constexpr uint32_t AUTO_BLOCKING_TIMEOUT_MS = UINT32_MAX;

  /// Convenience-only helper that may start a conversion, poll repeatedly,
  /// yield cooperatively, clear Mask/Enable flags through readiness polling,
  /// and read multiple channel registers in one call. Keep explicit staged
  /// operations in deadline-owned steady polling paths.
  /// @return INVALID_PARAM if no channel output pointer is provided or the
  ///         explicit timeout is zero or too large for wrap-safe local
  ///         deadline math; timeout derivation failures are returned directly.
  /// @param ch1 Optional channel 1 output.
  /// @param ch2 Optional channel 2 output.
  /// @param ch3 Optional channel 3 output.
  /// @param timeoutMs Non-zero explicit overall timeout at most INT32_MAX, or
  ///                  AUTO_BLOCKING_TIMEOUT_MS to derive a safe bound.
  Status readBlocking(
      ChannelMeasurement* ch1 = nullptr,
      ChannelMeasurement* ch2 = nullptr,
      ChannelMeasurement* ch3 = nullptr,
      uint32_t timeoutMs = AUTO_BLOCKING_TIMEOUT_MS);

  // === Configuration ===
  /// Set operating mode.
  /// @param mode Desired valid operating mode.
  /// @return IN_PROGRESS when a triggered mode write starts a single-shot conversion.
  Status setMode(Mode mode);

  /// @return Cached legacy operating mode.
  Mode getMode() const { return _config.mode; }

  /// @brief Write a new averaging setting through the compatibility API.
  /// @param avg Desired averaging setting.
  /// @return Status from validation and Configuration-register write.
  Status setAveraging(Averaging avg);

  /// @return Cached legacy averaging setting.
  Averaging getAveraging() const { return _config.averaging; }

  /// Set bus-voltage conversion time.
  /// @param ct Desired conversion-time setting.
  /// @return Status from validation and Configuration-register write.
  Status setVBusConvTime(ConvTime ct);
  /// Get bus-voltage conversion time.
  /// @return Cached bus-voltage conversion-time setting.
  ConvTime getVBusConvTime() const { return _config.vBusCt; }
  /// Cross-library naming alias for setVBusConvTime().
  /// @param ct Desired conversion-time setting.
  /// @return Status from setVBusConvTime().
  Status setVbusConvTime(ConvTime ct) { return setVBusConvTime(ct); }
  /// Cross-library naming alias for getVBusConvTime().
  /// @return Cached bus-voltage conversion-time setting.
  ConvTime getVbusConvTime() const { return getVBusConvTime(); }

  /// Set shunt-voltage conversion time.
  /// @param ct Desired conversion-time setting.
  /// @return Status from validation and Configuration-register write.
  Status setVShuntConvTime(ConvTime ct);
  /// Get shunt-voltage conversion time.
  /// @return Cached shunt-voltage conversion-time setting.
  ConvTime getVShuntConvTime() const { return _config.vShCt; }
  /// Cross-library naming alias for setVShuntConvTime().
  /// @param ct Desired conversion-time setting.
  /// @return Status from setVShuntConvTime().
  Status setVshuntConvTime(ConvTime ct) { return setVShuntConvTime(ct); }
  /// Cross-library naming alias for getVShuntConvTime().
  /// @return Cached shunt-voltage conversion-time setting.
  ConvTime getVshuntConvTime() const { return getVShuntConvTime(); }

  /// @brief Enable or disable one channel through the compatibility API.
  /// @param ch Channel to update.
  /// @param enable Desired enable state.
  /// @return Status from profile validation and Configuration-register write.
  Status setChannelEnable(Channel ch, bool enable);

  /// @brief Read one channel's cached enable state.
  /// @param ch Channel to inspect.
  /// @return Cached enable state, or false for an invalid enum value.
  bool getChannelEnable(Channel ch) const;

  /// Set host-side shunt resistance used for current/power calculations.
  /// @param ch Channel whose shunt resistance is being configured.
  /// @param ohms Must be finite and > 0.
  /// @note Runtime setter; pre-begin shunt values belong in Config.
  /// @return OK after updating host-only calibration, or a validation error.
  Status setShuntResistance(Channel ch, float ohms);

  /// @brief Read one channel's cached legacy shunt resistance.
  /// @param ch Channel to inspect.
  /// @return Resistance in ohms, or 0 for an invalid enum value.
  float getShuntResistance(Channel ch) const;

  /// @brief Read the Configuration register without changing the cache.
  /// @param config Receives the raw 16-bit register value.
  /// @return Status from precondition and register-read checks.
  Status readConfig(uint16_t& config);
  /// Write raw Configuration register and synchronize cached Config.
  /// @param config Raw 16-bit Configuration-register value.
  /// @return Status from validation and register write.
  /// @note Triggered mode bits start and track a single-shot conversion.
  Status writeConfig(uint16_t config);

  /// @brief Issue a software reset and invalidate configuration certainty.
  /// @return Status from the reset write.
  /// @note A confirmed reset leaves both certainty families DIRTY (the device
  ///       is known to hold power-on defaults, not the desired profile); an
  ///       ambiguous write leaves them UNKNOWN. Reconcile before measuring.
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
  /// @param flags Receives decoded flags from the consuming read.
  /// @return Status from precondition and register-read checks.
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
  /// @return IN_PROGRESS on compatibility-job admission, otherwise an error.
  Status startApplyMaskEnable(uint16_t writableBits);
  /// @brief Advance the scheduled complete profile reconciliation by budget.
  /// @param nowMs Current monotonic 32-bit millisecond timestamp.
  /// @param maxInstructions Maximum register transfers for this call.
  /// @return IN_PROGRESS while active, final job status on completion, or an error.
  Status pollApplyMaskEnable(uint32_t nowMs, uint8_t maxInstructions);

  // === Device Identification ===
  /// @brief Read the Manufacturer ID register through tracked transport.
  /// @param id Receives the raw 16-bit ID; expected value is 0x5449.
  /// @return Status from precondition and register-read checks.
  Status readManufacturerId(uint16_t& id);

  /// @brief Read the Die ID register through tracked transport.
  /// @param id Receives the raw 16-bit ID; expected value is 0x3220.
  /// @return Status from precondition and register-read checks.
  Status readDieId(uint16_t& id);

  // === Raw Register Access ===
  /// Read a 16-bit register using tracked I2C access.
  /// @param reg Valid readable register address.
  /// @param value Receives the big-endian register value converted to host order.
  /// @return Status from address validation, precondition, and transport checks.
  /// @note Reading REG_MASK_ENABLE clears latched alert and conversion-ready
  ///       flags per INA3221 register semantics.
  Status readRegister16(uint8_t reg, uint16_t& value);
  /// Write a 16-bit register using tracked I2C access.
  /// @param reg Valid writable register address.
  /// @param value Raw host-order register value to send big-endian.
  /// @return Status from address validation, precondition, and transport checks.
  /// @note Diagnostic raw writes bypass typed cache helpers. Every accepted or
  ///       ambiguous write marks hardwareConfigDirty() and drops the affected
  ///       certainty family (measurement for Configuration, alert for the
  ///       others) to UNKNOWN; reconcile before returning to the job engine.
  Status writeRegister16(uint8_t reg, uint16_t value);

  // === Utility ===
  /// @brief Decode a datasheet-format shunt register to millivolts.
  /// @param raw Raw 16-bit register value with data in bits 15:3.
  /// @return Signed shunt voltage in millivolts.
  static float shuntRawToMv(int16_t raw);

  /// @brief Decode a datasheet-format bus register to volts.
  /// @param raw Raw 16-bit register value with data in bits 15:3.
  /// @return Signed decoded bus voltage in volts.
  static float busRawToVolts(int16_t raw);

  /// @brief Encode millivolts as a clamped datasheet-format shunt register.
  /// @param mV Signed shunt voltage in millivolts; non-finite input becomes zero.
  /// @return Encoded value clamped to the signed 13-bit data range.
  static int16_t mvToShuntRaw(float mV);

  /// @brief Encode volts as a clamped datasheet-format bus register.
  /// @param volts Signed bus voltage in volts; non-finite input becomes zero.
  /// @return Encoded value clamped to the signed 13-bit data range.
  static int16_t voltsToBusRaw(float volts);

  /// @brief Get typical and scheduling-maximum time for one conversion.
  /// @param ct Valid conversion-time setting.
  /// @param out Replaced with typical and maximum microsecond values.
  /// @return OK, or INVALID_PARAM for an invalid enum value.
  static Status conversionTiming(ConvTime ct, ConversionTiming& out);

  /// @brief Calculate the scheduling-maximum full conversion-cycle duration.
  /// @param profile Supplies channel mask, averaging, and conversion times.
  /// @param mode Valid mode whose active raw quantities are included.
  /// @param outUs Receives maximum cycle time in microseconds; zero in power-down.
  /// @return OK, or a validation/overflow error.
  static Status maximumCycleTimeUs(const DeviceProfile& profile, Mode mode,
                                   uint64_t& outUs);

  /// @brief Decode a signed 13-bit shunt register using the 40 µV LSB.
  /// @param raw Raw 16-bit register value.
  /// @param outMicroVolts Receives signed shunt voltage in microvolts.
  /// @return Always OK.
  static Status decodeShuntMicroVolts(uint16_t raw, int32_t& outMicroVolts);

  /// @brief Decode a signed 13-bit bus register using the 8 mV LSB.
  /// @param raw Raw 16-bit register value.
  /// @param outMilliVolts Receives decoded bus voltage in millivolts.
  /// @return Always OK; use quantity validity to reject physical out-of-range values.
  static Status decodeBusMilliVolts(uint16_t raw, int32_t& outMilliVolts);
  /// Convert a checked channel enum to its fixed array index.
  /// @param channel Channel to convert.
  /// @param outIndex Receives 0 for CH1 through 2 for CH3.
  /// @return OK, or INVALID_PARAM for an invalid enum value.
  static Status channelIndex(Channel channel, uint8_t& outIndex);
  /// Convert a checked channel enum to its CH1-through-CH3 mask bit.
  /// @param channel Channel to convert.
  /// @param outBit Receives CHANNEL_1, CHANNEL_2, or CHANNEL_3.
  /// @return OK, or INVALID_PARAM for an invalid enum value.
  static Status channelBit(Channel channel, ChannelMask& outBit);
  /// Check mask membership after validating both mask and channel.
  /// @param mask Channel mask containing no bits outside ALL_CHANNELS.
  /// @param channel Channel to test.
  /// @param outContains Receives whether the channel bit is present.
  /// @return OK, or INVALID_PARAM for an invalid mask or enum value.
  static Status contains(ChannelMask mask, Channel channel, bool& outContains);
  /// Count enabled channels after rejecting bits outside CH1 through CH3.
  /// @param mask Channel mask to count.
  /// @param outCount Receives a value from zero through three.
  /// @return OK, or INVALID_PARAM for an invalid mask.
  static Status enabledChannelCount(ChannelMask mask, uint8_t& outCount);

  /// @brief Encode shunt microvolts using round-to-nearest 40 µV steps.
  /// @param microVolts Signed engineering-unit value.
  /// @param outRaw Receives datasheet-format bits 15:3.
  /// @return OK, or OUT_OF_RANGE outside the signed 13-bit field.
  static Status encodeShuntMicroVolts(int32_t microVolts, uint16_t& outRaw);

  /// @brief Encode bus millivolts using round-to-nearest 8 mV steps.
  /// @param milliVolts Signed engineering-unit value.
  /// @param outRaw Receives datasheet-format bits 15:3.
  /// @return OK, or OUT_OF_RANGE outside the signed 13-bit field.
  static Status encodeBusMilliVolts(int32_t milliVolts, uint16_t& outRaw);

  /// @brief Encode shunt-sum microvolts using round-to-nearest 40 µV steps.
  /// @param microVolts Signed engineering-unit value.
  /// @param outRaw Receives datasheet-format bits 15:1.
  /// @return OK, or OUT_OF_RANGE outside the signed 15-bit field.
  static Status encodeShuntSumMicroVolts(int32_t microVolts, uint16_t& outRaw);

  /// @brief Validate an ordered, encodable INA3221 power-valid window.
  /// @param lowerMilliVolts Lower bus-voltage threshold in millivolts.
  /// @param upperMilliVolts Upper bus-voltage threshold in millivolts.
  /// @return OK when lower is less than upper and both fit at or below 26 V;
  /// otherwise OUT_OF_RANGE.
  static Status validatePowerValidWindow(uint32_t lowerMilliVolts,
                                         uint32_t upperMilliVolts);

  /// @brief Calculate signed current from shunt voltage and explicit calibration.
  /// @param shuntMicroVolts Signed measured shunt voltage in microvolts.
  /// @param calibration Non-zero resistance and valid direction convention.
  /// @param outMilliAmps Receives rounded signed current in milliamps.
  /// @return OK, INVALID_CONFIG, or ARITHMETIC_OVERFLOW.
  static Status calculateCurrentMilliAmps(int32_t shuntMicroVolts,
                                           const ShuntCalibration& calibration,
                                           int32_t& outMilliAmps);

  /// @brief Calculate signed power from fixed-unit voltage and current.
  /// @param busMilliVolts Bus voltage in millivolts.
  /// @param currentMilliAmps Signed current in milliamps.
  /// @param outMilliWatts Receives rounded signed power in milliwatts.
  /// @return OK, or ARITHMETIC_OVERFLOW.
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
  /// @return OK after conversion, or a request/calibration/arithmetic error.
  static Status convertRawChannel(uint16_t shuntRaw, uint16_t busRaw,
                                  QuantityMask requestedRawQuantities,
                                  const ShuntCalibration& calibration,
                                  FixedChannelReading& out);
  /// @param reg Candidate register address.
  /// @return `true` when the address is in the documented readable register map.
  static bool isReadableRegister(uint8_t reg);

  /// @param reg Candidate register address.
  /// @return `true` when the address is a documented writable register.
  static bool isWritableRegister(uint8_t reg);
  /// Maximum callbacks on the success path when CVRF is set at its first
  /// eligible check. A low CVRF is rechecked no sooner than 1 ms later and is
  /// bounded by the caller's absolute deadline and poll cadence.
  /// @param kind Cooperative operation to bound; NONE is invalid.
  /// @param profile Valid complete profile used to count enabled channels.
  /// @param outTransfers Receives the successful-path callback ceiling.
  /// @return OK, or a profile/job-kind validation error.
  static Status maximumJobTransfers(JobKind kind, const DeviceProfile& profile,
                                    uint16_t& outTransfers);

  /// @return Datasheet-typical time for one configured compatibility conversion.
  uint32_t getConversionTimeUs() const;

  /// @return Datasheet-typical full enabled-channel cycle time, saturated to uint32_t.
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
