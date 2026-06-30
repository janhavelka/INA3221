/// @file INA3221.cpp
/// @brief Implementation of INA3221 driver

#include "INA3221/INA3221.h"

#include <climits>
#include <cmath>

namespace INA3221 {

namespace {

constexpr uint8_t kMinAddress = 0x40;
constexpr uint8_t kMaxAddress = 0x43;
constexpr uint16_t kMaskEnableWritable =
    cmd::MASK_SCC1 | cmd::MASK_SCC2 | cmd::MASK_SCC3 |
    cmd::MASK_WEN | cmd::MASK_CEN;
constexpr uint16_t kShuntLimitWritable = 0xFFF8;
constexpr uint16_t kShuntSumLimitWritable = 0xFFFE;
constexpr uint16_t kPowerValidLimitWritable = 0x7FF8;

bool isValidChannel(Channel ch) {
  return static_cast<uint8_t>(ch) <= static_cast<uint8_t>(Channel::CH3);
}

bool isValidAveraging(Averaging avg) {
  return static_cast<uint8_t>(avg) <= static_cast<uint8_t>(Averaging::AVG_1024);
}

bool isValidConvTime(ConvTime ct) {
  return static_cast<uint8_t>(ct) <= static_cast<uint8_t>(ConvTime::CT_8244US);
}

bool isValidMode(Mode mode) {
  return static_cast<uint8_t>(mode) <= static_cast<uint8_t>(Mode::SHUNT_BUS_CONT);
}

bool isPowerDownMode(Mode mode) {
  return mode == Mode::POWER_DOWN || mode == Mode::POWER_DOWN_ALT;
}

bool isTriggeredMode(Mode mode) {
  return mode == Mode::SHUNT_TRIG || mode == Mode::BUS_TRIG ||
         mode == Mode::SHUNT_BUS_TRIG;
}

bool isContinuousMode(Mode mode) {
  return mode == Mode::SHUNT_CONT || mode == Mode::BUS_CONT ||
         mode == Mode::SHUNT_BUS_CONT;
}

bool modeReadsShunt(Mode mode) {
  return mode == Mode::SHUNT_TRIG || mode == Mode::SHUNT_BUS_TRIG ||
         mode == Mode::SHUNT_CONT || mode == Mode::SHUNT_BUS_CONT;
}

bool modeReadsBus(Mode mode) {
  return mode == Mode::BUS_TRIG || mode == Mode::SHUNT_BUS_TRIG ||
         mode == Mode::BUS_CONT || mode == Mode::SHUNT_BUS_CONT;
}

bool isValidRegister(uint8_t reg) {
  return reg <= cmd::REG_PV_LOWER_LIMIT ||
         reg == cmd::REG_MANUFACTURER_ID ||
         reg == cmd::REG_DIE_ID;
}

bool registerAffectsCachedHardwareConfig(uint8_t reg) {
  return reg == cmd::REG_CONFIG || reg == cmd::REG_MASK_ENABLE;
}

bool isPositiveFinite(float value) {
  return std::isfinite(value) && value > 0.0f;
}

uint8_t enabledChannelCount(const Config& config) {
  uint8_t count = 0;
  if (config.ch1Enable) count++;
  if (config.ch2Enable) count++;
  if (config.ch3Enable) count++;
  return count;
}

bool modeAllowsNoChannels(Mode mode) {
  return isPowerDownMode(mode);
}

bool configHasRequiredChannels(const Config& config) {
  return modeAllowsNoChannels(config.mode) || enabledChannelCount(config) > 0;
}

bool isChannelEnabled(const Config& config, Channel ch) {
  switch (ch) {
    case Channel::CH1: return config.ch1Enable;
    case Channel::CH2: return config.ch2Enable;
    case Channel::CH3: return config.ch3Enable;
    default: return false;
  }
}

Status validateMeasurementRead(const Config& config, bool initialized,
                               Channel ch, bool requireShunt,
                               bool requireBus) {
  if (!initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  if (!isChannelEnabled(config, ch)) {
    return Status::Error(Err::INVALID_CONFIG, "Channel disabled");
  }
  if (requireShunt && !modeReadsShunt(config.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Mode does not measure shunt");
  }
  if (requireBus && !modeReadsBus(config.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Mode does not measure bus");
  }
  return Status::Ok();
}

int16_t signExtendField(uint16_t raw, uint8_t shift, uint8_t width) {
  const uint16_t fieldMask = static_cast<uint16_t>((1u << width) - 1u);
  uint16_t value = static_cast<uint16_t>((raw >> shift) & fieldMask);
  const uint16_t signBit = static_cast<uint16_t>(1u << (width - 1u));
  if ((value & signBit) != 0U) {
    value = static_cast<uint16_t>(value | static_cast<uint16_t>(~fieldMask));
  }
  return static_cast<int16_t>(value);
}

int16_t encodeSignedField(float value, float lsb, int32_t minValue,
                          int32_t maxValue, uint8_t shift) {
  if (!std::isfinite(value)) {
    value = 0.0f;
  }
  long scaled = lrintf(value / lsb);
  if (scaled < minValue) {
    scaled = minValue;
  } else if (scaled > maxValue) {
    scaled = maxValue;
  }
  const uint16_t encoded = static_cast<uint16_t>(static_cast<int16_t>(scaled));
  return static_cast<int16_t>(encoded << shift);
}

/// Conversion time in microseconds per CT setting
uint32_t convTimeUs(ConvTime ct) {
  static constexpr uint32_t table[] = {
    140,    // CT_140US
    204,    // CT_204US
    332,    // CT_332US
    588,    // CT_588US
    1100,   // CT_1100US
    2116,   // CT_2116US
    4156,   // CT_4156US
    8244    // CT_8244US
  };
  uint8_t idx = static_cast<uint8_t>(ct);
  if (idx >= sizeof(table) / sizeof(table[0])) {
    idx = static_cast<uint8_t>(ConvTime::CT_1100US);
  }
  return table[idx];
}

uint32_t averagingSampleCount(Averaging avg) {
  static constexpr uint32_t table[] = {
    1,     // AVG_1
    4,     // AVG_4
    16,    // AVG_16
    64,    // AVG_64
    128,   // AVG_128
    256,   // AVG_256
    512,   // AVG_512
    1024   // AVG_1024
  };
  uint8_t idx = static_cast<uint8_t>(avg);
  if (idx >= sizeof(table) / sizeof(table[0])) {
    idx = static_cast<uint8_t>(Averaging::AVG_1);
  }
  return table[idx];
}

/// Get shunt register address for channel
uint8_t shuntRegAddr(Channel ch) {
  return cmd::SHUNT_REG_BASE + static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE;
}

/// Get bus register address for channel
uint8_t busRegAddr(Channel ch) {
  return cmd::BUS_REG_BASE + static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE;
}

/// Get critical limit register address for channel
uint8_t critRegAddr(Channel ch) {
  return cmd::CRIT_REG_BASE + static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE;
}

/// Get warning limit register address for channel
uint8_t warnRegAddr(Channel ch) {
  return cmd::WARN_REG_BASE + static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE;
}

class ScopedOfflineI2cAllowance {
public:
  explicit ScopedOfflineI2cAllowance(bool& flag, bool allow) : _flag(flag), _old(flag) {
    _flag = allow;
  }

  ~ScopedOfflineI2cAllowance() {
    _flag = _old;
  }

  ScopedOfflineI2cAllowance(const ScopedOfflineI2cAllowance&) = delete;
  ScopedOfflineI2cAllowance& operator=(const ScopedOfflineI2cAllowance&) = delete;

private:
  bool& _flag;
  bool _old;
};

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status INA3221::begin(const Config& config) {
  const Config requestedConfig = config;

  _config = Config{};
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _allowOfflineI2c = false;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _maskEnableWritableCache = 0;
  _clearPollJob();

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  _clearHardwareConfigDirty();

  if (requestedConfig.i2cWrite == nullptr || requestedConfig.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (requestedConfig.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "Timeout must be > 0");
  }
  if (requestedConfig.i2cAddress < kMinAddress || requestedConfig.i2cAddress > kMaxAddress) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address (0x40-0x43)");
  }
  if (!isValidAveraging(requestedConfig.averaging) ||
      !isValidConvTime(requestedConfig.vBusCt) ||
      !isValidConvTime(requestedConfig.vShCt) ||
      !isValidMode(requestedConfig.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid config enum value");
  }
  if (!configHasRequiredChannels(requestedConfig)) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  for (int i = 0; i < 3; ++i) {
    if (!isPositiveFinite(requestedConfig.shuntResistance[i])) {
      return Status::Error(Err::INVALID_CONFIG, "Shunt resistance must be finite and > 0");
    }
  }

  _config = requestedConfig;
  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  auto failBeginAfterConfig = [this](const Status& failure) {
    _config = Config{};
    _allowOfflineI2c = false;
    _conversionStarted = false;
    _conversionReady = false;
    _conversionStartMs = 0;
    _maskEnableWritableCache = 0;
    _clearPollJob();
    _clearHardwareConfigDirty();
    return failure;
  };

  // Verify device identity
  Status st = probe();
  if (!st.ok()) {
    return failBeginAfterConfig(st);
  }

  // Apply configuration
  st = _applyConfig();
  if (!st.ok()) {
    return failBeginAfterConfig(st);
  }

  _initialized = true;
  _driverState = DriverState::READY;
  _handleConfigWriteSideEffects();
  _clearHardwareConfigDirty();
  return Status::Ok();
}

void INA3221::tick(uint32_t nowMs) {
  (void)tickStatus(nowMs);
}

Status INA3221::tickStatus(uint32_t nowMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if (_isTriggeredMode() && _conversionStarted && !_conversionReady) {
    bool ready = false;
    return _readConversionReadyAt(nowMs, ready);
  }
  return Status::Ok();
}

Status INA3221::powerDown() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return setMode(Mode::POWER_DOWN);
}

void INA3221::end() {
  if (_initialized) {
    (void)powerDown();
  }

  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _maskEnableWritableCache = 0;
  _clearPollJob();
  _clearHardwareConfigDirty();
}

// ============================================================================
// Diagnostics
// ============================================================================

Status INA3221::probe() {
  uint16_t mfgId = 0;
  Status st = _readRegister16Raw(cmd::REG_MANUFACTURER_ID, mfgId);
  if (!st.ok()) {
    return st;
  }
  if (mfgId != cmd::MANUFACTURER_ID_VALUE) {
    return Status::Error(Err::MANUFACTURER_ID_MISMATCH, "Manufacturer ID mismatch",
                         static_cast<int32_t>(mfgId));
  }

  uint16_t dieId = 0;
  st = _readRegister16Raw(cmd::REG_DIE_ID, dieId);
  if (!st.ok()) {
    return st;
  }
  if (dieId != cmd::DIE_ID_VALUE) {
    return Status::Error(Err::DIE_ID_MISMATCH, "Die ID mismatch",
                         static_cast<int32_t>(dieId));
  }

  return Status::Ok();
}

Status INA3221::recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const bool startedOffline = (_driverState == DriverState::OFFLINE);
  ScopedOfflineI2cAllowance allowOfflineI2c(_allowOfflineI2c, true);
  Status result = [&]() -> Status {
    uint16_t mfgId = 0;
    Status st = readRegister16(cmd::REG_MANUFACTURER_ID, mfgId);
    if (!st.ok()) {
      return st;
    }
    if (mfgId != cmd::MANUFACTURER_ID_VALUE) {
      return _recordFailure(Status::Error(Err::MANUFACTURER_ID_MISMATCH,
                                          "Manufacturer ID mismatch",
                                          static_cast<int32_t>(mfgId)));
    }

    uint16_t dieId = 0;
    st = readRegister16(cmd::REG_DIE_ID, dieId);
    if (!st.ok()) {
      return st;
    }
    if (dieId != cmd::DIE_ID_VALUE) {
      return _recordFailure(Status::Error(Err::DIE_ID_MISMATCH,
                                          "Die ID mismatch",
                                          static_cast<int32_t>(dieId)));
    }

    _conversionStarted = false;
    _conversionReady = false;
    _conversionStartMs = 0;
    _clearPollJob();

    st = _applyConfig();
    if (!st.ok()) {
      _markHardwareConfigDirty(st);
      return st;
    }
    _handleConfigWriteSideEffects();

    st = _writeRegister16Tracked(cmd::REG_MASK_ENABLE,
                                 static_cast<uint16_t>(_maskEnableWritableCache &
                                                       kMaskEnableWritable));
    if (!st.ok()) {
      _markHardwareConfigDirty(st);
      return st;
    }

    _clearHardwareConfigDirty();
    return Status::Ok();
  }();
  if (startedOffline && !result.ok() && !result.inProgress()) {
    _reassertOfflineLatch();
  }
  return result;
}

Status INA3221::getSettings(SettingsSnapshot& out) const {
  out.initialized = _initialized;
  out.state = _driverState;
  out.i2cAddress = _config.i2cAddress;
  out.i2cTimeoutMs = _config.i2cTimeoutMs;
  out.offlineThreshold = _config.offlineThreshold;
  out.hasNowMsHook = _config.nowMs != nullptr;
  out.hasCooperativeYieldHook = _config.cooperativeYield != nullptr;
  out.ch1Enable = _config.ch1Enable;
  out.ch2Enable = _config.ch2Enable;
  out.ch3Enable = _config.ch3Enable;
  out.averaging = _config.averaging;
  out.vBusCt = _config.vBusCt;
  out.vShCt = _config.vShCt;
  out.mode = _config.mode;
  out.shuntResistance[0] = _config.shuntResistance[0];
  out.shuntResistance[1] = _config.shuntResistance[1];
  out.shuntResistance[2] = _config.shuntResistance[2];
  out.conversionStarted = _conversionStarted;
  out.conversionReady = _conversionReady;
  out.conversionStartMs = _conversionStartMs;
  out.maskEnableWritableCache = _maskEnableWritableCache;
  out.hardwareConfigDirty = _hardwareConfigDirty;
  out.hardwareConfigDirtyStatus = _hardwareConfigDirtyStatus;
  return Status::Ok();
}

// ============================================================================
// Measurement API
// ============================================================================

Status INA3221::readShuntRaw(Channel ch, int16_t& raw) {
  Status valid = validateMeasurementRead(_config, _initialized, ch, true, false);
  if (!valid.ok()) {
    return valid;
  }
  Status readyStatus = _ensureMeasurementReadyForRead();
  if (!readyStatus.ok()) {
    return readyStatus;
  }

  uint16_t regVal = 0;
  Status st = readRegister16(shuntRegAddr(ch), regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::readBusRaw(Channel ch, int16_t& raw) {
  Status valid = validateMeasurementRead(_config, _initialized, ch, false, true);
  if (!valid.ok()) {
    return valid;
  }
  Status readyStatus = _ensureMeasurementReadyForRead();
  if (!readyStatus.ok()) {
    return readyStatus;
  }

  uint16_t regVal = 0;
  Status st = readRegister16(busRegAddr(ch), regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::readShuntVoltage(Channel ch, float& mV) {
  int16_t raw = 0;
  Status st = readShuntRaw(ch, raw);
  if (!st.ok()) {
    return st;
  }
  mV = shuntRawToMv(raw);
  return Status::Ok();
}

Status INA3221::readBusVoltage(Channel ch, float& volts) {
  int16_t raw = 0;
  Status st = readBusRaw(ch, raw);
  if (!st.ok()) {
    return st;
  }
  volts = busRawToVolts(raw);
  return Status::Ok();
}

Status INA3221::readCurrent(Channel ch, float& mA) {
  Status valid = validateMeasurementRead(_config, _initialized, ch, true, false);
  if (!valid.ok()) {
    return valid;
  }
  float shuntMv = 0.0f;
  Status st = readShuntVoltage(ch, shuntMv);
  if (!st.ok()) {
    return st;
  }
  float rShunt = _config.shuntResistance[static_cast<uint8_t>(ch)];
  // I = Vshunt / Rshunt, Vshunt in mV, Rshunt in ohms -> I in mA
  mA = shuntMv / rShunt;
  return Status::Ok();
}

Status INA3221::readPower(Channel ch, float& mW) {
  Status valid = validateMeasurementRead(_config, _initialized, ch, true, true);
  if (!valid.ok()) {
    return valid;
  }
  float busV = 0.0f;
  float currentMa = 0.0f;
  Status st = readBusVoltage(ch, busV);
  if (!st.ok()) {
    return st;
  }
  st = readCurrent(ch, currentMa);
  if (!st.ok()) {
    return st;
  }
  // P = V * I, V in volts, I in mA -> P in mW
  mW = busV * currentMa;
  return Status::Ok();
}

Status INA3221::readChannel(Channel ch, ChannelMeasurement& out) {
  Status valid = validateMeasurementRead(_config, _initialized, ch, true, true);
  if (!valid.ok()) {
    return valid;
  }
  Status readyStatus = _ensureMeasurementReadyForRead();
  if (!readyStatus.ok()) {
    return readyStatus;
  }

  int16_t shuntRaw = 0;
  Status st = readShuntRaw(ch, shuntRaw);
  if (!st.ok()) {
    return st;
  }

  int16_t busRaw = 0;
  st = readBusRaw(ch, busRaw);
  if (!st.ok()) {
    return st;
  }

  out.shuntVoltage_mV = shuntRawToMv(shuntRaw);
  out.busVoltage_V = busRawToVolts(busRaw);

  float rShunt = _config.shuntResistance[static_cast<uint8_t>(ch)];
  out.current_mA = out.shuntVoltage_mV / rShunt;
  out.power_mW = out.busVoltage_V * out.current_mA;

  return Status::Ok();
}

Status INA3221::readShuntSumRaw(int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status readyStatus = _ensureMeasurementReadyForRead();
  if (!readyStatus.ok()) {
    return readyStatus;
  }
  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_SHUNT_SUM, regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::readShuntSumVoltage(float& mV) {
  int16_t raw = 0;
  Status st = readShuntSumRaw(raw);
  if (!st.ok()) {
    return st;
  }
  // Sum register: data in bits [15:1], bit 0 reserved. LSB = 40 uV.
  int16_t dataValue = signExtendField(static_cast<uint16_t>(raw),
                                      cmd::SUM_DATA_SHIFT,
                                      15);
  mV = dataValue * cmd::SHUNT_LSB_MV;
  return Status::Ok();
}

// ============================================================================
// Single-Shot Conversion API
// ============================================================================

Status INA3221::startConversion() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_isContinuousMode()) {
    return Status::Error(Err::BUSY, "Continuous mode active");
  }
  if (!_isTriggeredMode()) {
    return Status::Error(Err::INVALID_CONFIG, "Triggered mode required");
  }
  if (_enabledChannelCount() == 0) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  // Writing to config register triggers single-shot conversion
  uint16_t configReg = _buildConfigRegister();
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = _nowMs();
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status INA3221::startConversion(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isTriggeredMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Must be a triggered mode");
  }
  if (_enabledChannelCount() == 0) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  Mode prevMode = _config.mode;
  _config.mode = mode;

  uint16_t configReg = _buildConfigRegister();
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _config.mode = prevMode;
    _markHardwareConfigDirty(st);
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = _nowMs();
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status INA3221::readConversionReady(bool& ready) {
  return _readConversionReadyAt(_nowMs(), ready);
}

bool INA3221::conversionReady() {
  bool ready = false;
  Status st = readConversionReady(ready);
  return st.ok() && ready;
}

Status INA3221::startSingleShot(bool pollConversionReady) {
  Mode mode = _isTriggeredMode() ? _config.mode : Mode::SHUNT_BUS_TRIG;
  return startSingleShot(mode, pollConversionReady);
}

Status INA3221::startSingleShot(Mode mode, bool pollConversionReady) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isTriggeredMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Must be a triggered mode");
  }
  return _startSampleJob(PollJobKind::SINGLE_SHOT, mode, pollConversionReady);
}

Status INA3221::pollSingleShot(uint32_t nowMs, uint8_t maxInstructions) {
  if (_pollJobKind == PollJobKind::NONE &&
      _pollJobStage != PollJobStage::COMPLETE) {
    return Status::Ok();
  }
  if (_pollJobKind != PollJobKind::SINGLE_SHOT) {
    return Status::Error(Err::BUSY, "Single-shot job is not active");
  }
  return _pollSampleJob(nowMs, maxInstructions);
}

Status INA3221::startContinuousRead(bool pollConversionReady) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!_isContinuousMode()) {
    return Status::Error(Err::INVALID_CONFIG, "Continuous mode required");
  }
  return _startSampleJob(PollJobKind::CONTINUOUS_READ, _config.mode,
                         pollConversionReady);
}

Status INA3221::pollContinuousRead(uint32_t nowMs, uint8_t maxInstructions) {
  if (_pollJobKind == PollJobKind::NONE &&
      _pollJobStage != PollJobStage::COMPLETE) {
    return Status::Ok();
  }
  if (_pollJobKind != PollJobKind::CONTINUOUS_READ) {
    return Status::Error(Err::BUSY, "Continuous-read job is not active");
  }
  return _pollSampleJob(nowMs, maxInstructions);
}

Status INA3221::pollJob(uint32_t nowMs, uint8_t maxInstructions) {
  switch (_pollJobKind) {
    case PollJobKind::SINGLE_SHOT:
      return pollSingleShot(nowMs, maxInstructions);
    case PollJobKind::CONTINUOUS_READ:
      return pollContinuousRead(nowMs, maxInstructions);
    case PollJobKind::APPLY_MASK_ENABLE:
      return pollApplyMaskEnable(nowMs, maxInstructions);
    case PollJobKind::NONE:
    default:
      return Status::Ok();
  }
}

Status INA3221::readChannelRawStep(Channel ch) {
  _pollInstructionsLast = 0;
  const bool willTransfer =
      isValidChannel(ch) &&
      (_pollJobKind == PollJobKind::SINGLE_SHOT ||
       _pollJobKind == PollJobKind::CONTINUOUS_READ) &&
      _pollJobStage == PollJobStage::READ_CHANNELS &&
      _pollChannels[static_cast<uint8_t>(ch)].channelEnabled &&
      !_sampleChannelComplete(ch);
  Status st = _readChannelRawStep(ch);
  if (willTransfer && (st.ok() || st.inProgress())) {
    _pollInstructionsLast = 1;
    if (_pollInstructionsTotal < UINT16_MAX) {
      _pollInstructionsTotal++;
    }
  }
  return st;
}

Status INA3221::getPollJobSnapshot(PollJobSnapshot& out) const {
  out.kind = _pollJobKind;
  out.stage = _pollJobStage;
  out.active = _pollJobKind != PollJobKind::NONE &&
               _pollJobStage != PollJobStage::COMPLETE;
  out.complete = _pollJobStage == PollJobStage::COMPLETE;
  out.pollConversionReady = _pollConversionReady;
  out.conversionReady = _pollObservedReady;
  out.sampleMode = _pollSampleMode;
  out.conversionStartMs = _pollConversionStartMs;
  out.nextChannel = _pollNextChannel;
  out.lastInstructions = _pollInstructionsLast;
  out.totalInstructions = _pollInstructionsTotal;
  for (uint8_t i = 0; i < 3; ++i) {
    out.channels[i] = _pollChannels[i];
  }
  return Status::Ok();
}

Status INA3221::readBlocking(ChannelMeasurement* ch1,
                             ChannelMeasurement* ch2,
                             ChannelMeasurement* ch3,
                             uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (ch1 == nullptr && ch2 == nullptr && ch3 == nullptr) {
    return Status::Error(Err::INVALID_PARAM, "At least one output channel is required");
  }
  if (timeoutMs > static_cast<uint32_t>(INT32_MAX)) {
    return Status::Error(Err::INVALID_PARAM, "Timeout too large");
  }

  // If continuous mode, just read directly
  if (_isContinuousMode()) {
    if (ch1) { Status st = readChannel(Channel::CH1, *ch1); if (!st.ok()) return st; }
    if (ch2) { Status st = readChannel(Channel::CH2, *ch2); if (!st.ok()) return st; }
    if (ch3) { Status st = readChannel(Channel::CH3, *ch3); if (!st.ok()) return st; }
    return Status::Ok();
  }

  // Triggered mode: start conversion if not already started
  Status st = startConversion();
  if (st.code != Err::IN_PROGRESS && st.code != Err::BUSY) {
    return st;
  }

  const uint32_t startMs = _nowMs();
  const uint32_t deadlineMs = startMs + timeoutMs;
  const uint32_t minCycleMs = (getCycleTimeUs() + 999) / 1000 + 1;
  const uint32_t maxPolls = timeoutMs + minCycleMs + 2U;
  uint32_t polls = 0;

  // Wait for conversion ready
  while (static_cast<int32_t>(_nowMs() - deadlineMs) < 0 && polls < maxPolls) {
    bool ready = false;
    st = readConversionReady(ready);
    if (!st.ok()) {
      return st;
    }
    if (ready) {
      break;
    }
    polls++;
    _cooperativeYield();
  }

  if (!_conversionReady) {
    _conversionStarted = false;
    _conversionReady = false;
    return Status::Error(Err::TIMEOUT, "Conversion timeout");
  }

  // Read requested channels
  if (ch1) { st = readChannel(Channel::CH1, *ch1); if (!st.ok()) return st; }
  if (ch2) { st = readChannel(Channel::CH2, *ch2); if (!st.ok()) return st; }
  if (ch3) { st = readChannel(Channel::CH3, *ch3); if (!st.ok()) return st; }

  _conversionReady = false;
  return Status::Ok();
}

// ============================================================================
// Configuration
// ============================================================================

Status INA3221::setMode(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mode");
  }
  if (!modeAllowsNoChannels(mode) && _enabledChannelCount() == 0) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  const Config prevConfig = _config;
  const bool prevStarted = _conversionStarted;
  const bool prevReady = _conversionReady;
  const uint32_t prevStartMs = _conversionStartMs;
  _config.mode = mode;
  _conversionStarted = false;
  _conversionReady = false;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = prevConfig;
    _conversionStarted = prevStarted;
    _conversionReady = prevReady;
    _conversionStartMs = prevStartMs;
    _markHardwareConfigDirty(st);
    return st;
  }
  _handleConfigWriteSideEffects();
  if (_isTriggeredMode()) {
    return Status{Err::IN_PROGRESS, 0, "Conversion started"};
  }
  return Status::Ok();
}

Status INA3221::setAveraging(Averaging avg) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidAveraging(avg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid averaging");
  }
  const Config prevConfig = _config;
  const bool prevStarted = _conversionStarted;
  const bool prevReady = _conversionReady;
  const uint32_t prevStartMs = _conversionStartMs;
  _config.averaging = avg;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = prevConfig;
    _conversionStarted = prevStarted;
    _conversionReady = prevReady;
    _conversionStartMs = prevStartMs;
    _markHardwareConfigDirty(st);
    return st;
  }
  _handleConfigWriteSideEffects();
  return Status::Ok();
}

Status INA3221::setVBusConvTime(ConvTime ct) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  const Config prevConfig = _config;
  const bool prevStarted = _conversionStarted;
  const bool prevReady = _conversionReady;
  const uint32_t prevStartMs = _conversionStartMs;
  _config.vBusCt = ct;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = prevConfig;
    _conversionStarted = prevStarted;
    _conversionReady = prevReady;
    _conversionStartMs = prevStartMs;
    _markHardwareConfigDirty(st);
    return st;
  }
  _handleConfigWriteSideEffects();
  return Status::Ok();
}

Status INA3221::setVShuntConvTime(ConvTime ct) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  const Config prevConfig = _config;
  const bool prevStarted = _conversionStarted;
  const bool prevReady = _conversionReady;
  const uint32_t prevStartMs = _conversionStartMs;
  _config.vShCt = ct;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = prevConfig;
    _conversionStarted = prevStarted;
    _conversionReady = prevReady;
    _conversionStartMs = prevStartMs;
    _markHardwareConfigDirty(st);
    return st;
  }
  _handleConfigWriteSideEffects();
  return Status::Ok();
}

Status INA3221::setChannelEnable(Channel ch, bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  Config nextConfig = _config;
  switch (ch) {
    case Channel::CH1: nextConfig.ch1Enable = enable; break;
    case Channel::CH2: nextConfig.ch2Enable = enable; break;
    case Channel::CH3: nextConfig.ch3Enable = enable; break;
  }
  if (!configHasRequiredChannels(nextConfig)) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  const Config prevConfig = _config;
  const bool prevStarted = _conversionStarted;
  const bool prevReady = _conversionReady;
  const uint32_t prevStartMs = _conversionStartMs;
  switch (ch) {
    case Channel::CH1: _config.ch1Enable = enable; break;
    case Channel::CH2: _config.ch2Enable = enable; break;
    case Channel::CH3: _config.ch3Enable = enable; break;
  }
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = prevConfig;
    _conversionStarted = prevStarted;
    _conversionReady = prevReady;
    _conversionStartMs = prevStartMs;
    _markHardwareConfigDirty(st);
    return st;
  }
  _handleConfigWriteSideEffects();
  return Status::Ok();
}

bool INA3221::getChannelEnable(Channel ch) const {
  return isChannelEnabled(_config, ch);
}

Status INA3221::setShuntResistance(Channel ch, float ohms) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  if (!isPositiveFinite(ohms)) {
    return Status::Error(Err::INVALID_PARAM, "Shunt resistance must be finite and > 0");
  }
  _config.shuntResistance[static_cast<uint8_t>(ch)] = ohms;
  return Status::Ok();
}

float INA3221::getShuntResistance(Channel ch) const {
  if (!isValidChannel(ch)) {
    return 0.0f;
  }
  return _config.shuntResistance[static_cast<uint8_t>(ch)];
}

Status INA3221::readConfig(uint16_t& config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return readRegister16(cmd::REG_CONFIG, config);
}

Status INA3221::writeConfig(uint16_t config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if ((config & cmd::MASK_RST) != 0U) {
    Status st = _writeRegister16Tracked(cmd::REG_CONFIG, config);
    if (!st.ok()) {
      _markHardwareConfigDirty(st);
      return st;
    }

    _config.ch1Enable = true;
    _config.ch2Enable = true;
    _config.ch3Enable = true;
    _config.averaging = Averaging::AVG_1;
    _config.vBusCt = ConvTime::CT_1100US;
    _config.vShCt = ConvTime::CT_1100US;
    _config.mode = Mode::SHUNT_BUS_CONT;
    _conversionStarted = false;
    _conversionReady = false;
    _conversionStartMs = 0;
    _maskEnableWritableCache = 0;
    _clearPollJob();
    _clearHardwareConfigDirty();
    return Status::Ok();
  }

  Config nextConfig = _config;
  nextConfig.ch1Enable = (config & cmd::MASK_CH1EN) != 0;
  nextConfig.ch2Enable = (config & cmd::MASK_CH2EN) != 0;
  nextConfig.ch3Enable = (config & cmd::MASK_CH3EN) != 0;
  nextConfig.averaging = static_cast<Averaging>((config & cmd::MASK_AVG) >> cmd::BIT_AVG);
  nextConfig.vBusCt = static_cast<ConvTime>((config & cmd::MASK_VBUSCT) >> cmd::BIT_VBUSCT);
  nextConfig.vShCt = static_cast<ConvTime>((config & cmd::MASK_VSHCT) >> cmd::BIT_VSHCT);
  nextConfig.mode = static_cast<Mode>((config & cmd::MASK_MODE) >> cmd::BIT_MODE);
  if (!configHasRequiredChannels(nextConfig)) {
    return Status::Error(Err::INVALID_PARAM, "At least one channel must be enabled");
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, config);
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  // Sync config struct from register value
  _config = nextConfig;

  _handleConfigWriteSideEffects();
  return Status::Ok();
}

Status INA3221::softReset() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, cmd::MASK_RST);
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  // RST bit is self-clearing; sync config to defaults
  _config.ch1Enable = true;
  _config.ch2Enable = true;
  _config.ch3Enable = true;
  _config.averaging = Averaging::AVG_1;
  _config.vBusCt = ConvTime::CT_1100US;
  _config.vShCt = ConvTime::CT_1100US;
  _config.mode = Mode::SHUNT_BUS_CONT;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _maskEnableWritableCache = 0;
  _clearPollJob();
  _clearHardwareConfigDirty();

  return Status::Ok();
}

// ============================================================================
// Alert Limits
// ============================================================================

Status INA3221::setCriticalAlertLimit(Channel ch, int16_t raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  return writeRegister16(critRegAddr(ch),
                         static_cast<uint16_t>(raw) & kShuntLimitWritable);
}

Status INA3221::getCriticalAlertLimit(Channel ch, int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  uint16_t regVal = 0;
  Status st = readRegister16(critRegAddr(ch), regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::setWarningAlertLimit(Channel ch, int16_t raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  return writeRegister16(warnRegAddr(ch),
                         static_cast<uint16_t>(raw) & kShuntLimitWritable);
}

Status INA3221::getWarningAlertLimit(Channel ch, int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  uint16_t regVal = 0;
  Status st = readRegister16(warnRegAddr(ch), regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::setShuntSumLimit(int16_t raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return writeRegister16(cmd::REG_SHUNT_SUM_LIMIT,
                         static_cast<uint16_t>(raw) & kShuntSumLimitWritable);
}

Status INA3221::getShuntSumLimit(int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_SHUNT_SUM_LIMIT, regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::setPowerValidUpperLimit(int16_t raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return writeRegister16(cmd::REG_PV_UPPER_LIMIT,
                         static_cast<uint16_t>(raw) & kPowerValidLimitWritable);
}

Status INA3221::getPowerValidUpperLimit(int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_PV_UPPER_LIMIT, regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

Status INA3221::setPowerValidLowerLimit(int16_t raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return writeRegister16(cmd::REG_PV_LOWER_LIMIT,
                         static_cast<uint16_t>(raw) & kPowerValidLimitWritable);
}

Status INA3221::getPowerValidLowerLimit(int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_PV_LOWER_LIMIT, regVal);
  if (!st.ok()) {
    return st;
  }
  raw = static_cast<int16_t>(regVal);
  return Status::Ok();
}

// ============================================================================
// Mask/Enable Register
// ============================================================================

Status INA3221::readAlertFlags(AlertFlags& flags) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_MASK_ENABLE, regVal);
  if (!st.ok()) {
    return st;
  }

  flags.criticalCh1     = (regVal & cmd::MASK_CF1)  != 0;
  flags.criticalCh2     = (regVal & cmd::MASK_CF2)  != 0;
  flags.criticalCh3     = (regVal & cmd::MASK_CF3)  != 0;
  flags.summation       = (regVal & cmd::MASK_SF)   != 0;
  flags.warningCh1      = (regVal & cmd::MASK_WF1)  != 0;
  flags.warningCh2      = (regVal & cmd::MASK_WF2)  != 0;
  flags.warningCh3      = (regVal & cmd::MASK_WF3)  != 0;
  flags.powerValid      = (regVal & cmd::MASK_PVF)  != 0;
  flags.timingControl   = (regVal & cmd::MASK_TCF)  != 0;
  flags.conversionReady = (regVal & cmd::MASK_CVRF) != 0;

  if (flags.conversionReady && _isTriggeredMode()) {
    _conversionStarted = false;
    _conversionReady = true;
  }

  return Status::Ok();
}

Status INA3221::readAndClearAlertFlags(AlertFlags& flags) {
  return readAlertFlags(flags);
}

Status INA3221::setSummationChannels(bool ch1, bool ch2, bool ch3) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t regVal = static_cast<uint16_t>(_maskEnableWritableCache & kMaskEnableWritable);
  regVal &= static_cast<uint16_t>(~(cmd::MASK_SCC1 | cmd::MASK_SCC2 | cmd::MASK_SCC3));
  if (ch1) regVal |= cmd::MASK_SCC1;
  if (ch2) regVal |= cmd::MASK_SCC2;
  if (ch3) regVal |= cmd::MASK_SCC3;

  Status st = _writeRegister16Tracked(cmd::REG_MASK_ENABLE, regVal);
  if (st.ok()) {
    _maskEnableWritableCache = regVal;
  } else {
    _markHardwareConfigDirty(st);
  }
  return st;
}

Status INA3221::setAlertLatchEnable(bool warningLatch, bool criticalLatch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t regVal = static_cast<uint16_t>(_maskEnableWritableCache & kMaskEnableWritable);
  regVal &= ~(cmd::MASK_WEN | cmd::MASK_CEN);
  if (warningLatch) regVal |= cmd::MASK_WEN;
  if (criticalLatch) regVal |= cmd::MASK_CEN;

  Status st = _writeRegister16Tracked(cmd::REG_MASK_ENABLE, regVal);
  if (st.ok()) {
    _maskEnableWritableCache = regVal;
  } else {
    _markHardwareConfigDirty(st);
  }
  return st;
}

Status INA3221::startApplyMaskEnable(uint16_t writableBits) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_pollJobKind != PollJobKind::NONE &&
      _pollJobStage != PollJobStage::COMPLETE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }

  _clearPollJob();
  _pollJobKind = PollJobKind::APPLY_MASK_ENABLE;
  _pollJobStage = PollJobStage::WRITE_MASK_ENABLE;
  _pendingMaskEnableWritable = static_cast<uint16_t>(writableBits & kMaskEnableWritable);
  return Status{Err::IN_PROGRESS, 0, "Mask/Enable apply scheduled"};
}

Status INA3221::pollApplyMaskEnable(uint32_t nowMs, uint8_t maxInstructions) {
  (void)nowMs;
  if (_pollJobKind == PollJobKind::NONE &&
      _pollJobStage != PollJobStage::COMPLETE) {
    return Status::Ok();
  }
  if (_pollJobKind != PollJobKind::APPLY_MASK_ENABLE) {
    return Status::Error(Err::BUSY, "Mask/Enable apply job is not active");
  }
  return _pollApplyMaskEnableJob(maxInstructions);
}

// ============================================================================
// Device Identification
// ============================================================================

Status INA3221::readManufacturerId(uint16_t& id) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return readRegister16(cmd::REG_MANUFACTURER_ID, id);
}

Status INA3221::readDieId(uint16_t& id) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return readRegister16(cmd::REG_DIE_ID, id);
}

// ============================================================================
// Utility
// ============================================================================

float INA3221::shuntRawToMv(int16_t raw) {
  // Data in bits [15:3]; explicitly sign extend the 13-bit value.
  int16_t dataValue = signExtendField(static_cast<uint16_t>(raw),
                                      cmd::DATA_SHIFT,
                                      13);
  return dataValue * cmd::SHUNT_LSB_MV;
}

float INA3221::busRawToVolts(int16_t raw) {
  int16_t dataValue = signExtendField(static_cast<uint16_t>(raw),
                                      cmd::DATA_SHIFT,
                                      13);
  return dataValue * cmd::BUS_LSB_V;
}

int16_t INA3221::mvToShuntRaw(float mV) {
  return encodeSignedField(mV, cmd::SHUNT_LSB_MV, -4096, 4095, cmd::DATA_SHIFT);
}

int16_t INA3221::voltsToBusRaw(float volts) {
  return encodeSignedField(volts, cmd::BUS_LSB_V, -4096, 4095, cmd::DATA_SHIFT);
}

uint32_t INA3221::getConversionTimeUs() const {
  uint32_t shuntUs = convTimeUs(_config.vShCt);
  uint32_t busUs = convTimeUs(_config.vBusCt);

  Mode mode = _config.mode;
  switch (mode) {
    case Mode::SHUNT_TRIG:
    case Mode::SHUNT_CONT:
      return shuntUs;
    case Mode::BUS_TRIG:
    case Mode::BUS_CONT:
      return busUs;
    case Mode::SHUNT_BUS_TRIG:
    case Mode::SHUNT_BUS_CONT:
      return shuntUs + busUs;
    default:
      return 0;
  }
}

uint32_t INA3221::getCycleTimeUs() const {
  return getConversionTimeUs() * _enabledChannelCount() *
         averagingSampleCount(_config.averaging);
}

// ============================================================================
// Transport Wrappers
// ============================================================================

Status INA3221::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                 uint8_t* rxBuf, size_t rxLen) {
  if (_config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback missing");
  }
  if (txBuf == nullptr || txLen == 0 || rxBuf == nullptr || rxLen == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen,
                              rxBuf, rxLen, _config.i2cTimeoutMs,
                              _config.i2cUser);
}

Status INA3221::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  if (_config.i2cWrite == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback missing");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
}

Status INA3221::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return Status::Error(Err::BUSY, "Driver is offline; call recover()");
  }

  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status INA3221::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return Status::Error(Err::BUSY, "Driver is offline; call recover()");
  }

  Status st = _i2cWriteRaw(buf, len);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

// ============================================================================
// Register Access
// ============================================================================

Status INA3221::readRegister16(uint8_t reg, uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register address");
  }
  return _readRegister16Tracked(reg, value);
}

Status INA3221::writeRegister16(uint8_t reg, uint16_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register address");
  }
  Status st = _writeRegister16Tracked(reg, value);
  if (registerAffectsCachedHardwareConfig(reg)) {
    if (st.ok()) {
      _markHardwareConfigDirty(
          Status{Err::OK, static_cast<int32_t>(reg),
                 "Raw register write bypassed cached settings"});
    } else {
      _markHardwareConfigDirty(st);
    }
  }
  return st;
}

Status INA3221::_readRegister16Tracked(uint8_t reg, uint16_t& value) {
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadTracked(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
  return Status::Ok();
}

Status INA3221::_writeRegister16Tracked(uint8_t reg, uint16_t value) {
  uint8_t tx[3] = {
    reg,
    static_cast<uint8_t>((value >> 8) & 0xFF),
    static_cast<uint8_t>(value & 0xFF)
  };
  return _i2cWriteTracked(tx, sizeof(tx));
}

Status INA3221::_readRegister16Raw(uint8_t reg, uint16_t& value) {
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadRaw(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
  return Status::Ok();
}

// ============================================================================
// Health Tracking
// ============================================================================

Status INA3221::_updateHealth(const Status& st) {
  if (!_initialized || st.inProgress()) {
    return st;
  }

  uint32_t nowMs = _nowMs();

  if (st.ok()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }

    _driverState = DriverState::READY;
  } else {
    _lastErrorMs = nowMs;
    _lastError = st;

    if (_consecutiveFailures < UINT8_MAX) {
      _consecutiveFailures++;
    }
    if (_totalFailures < UINT32_MAX) {
      _totalFailures++;
    }

    if (_consecutiveFailures >= _config.offlineThreshold) {
      _driverState = DriverState::OFFLINE;
    } else {
      _driverState = DriverState::DEGRADED;
    }
  }

  return st;
}

Status INA3221::_recordFailure(const Status& st) {
  if (st.ok() || st.inProgress()) {
    return st;
  }

  uint32_t nowMs = _nowMs();
  _lastErrorMs = nowMs;
  _lastError = st;

  if (_consecutiveFailures < UINT8_MAX) {
    _consecutiveFailures++;
  }
  if (_totalFailures < UINT32_MAX) {
    _totalFailures++;
  }

  if (_initialized) {
    if (_consecutiveFailures >= _config.offlineThreshold) {
      _driverState = DriverState::OFFLINE;
    } else {
      _driverState = DriverState::DEGRADED;
    }
  }

  return st;
}

void INA3221::_reassertOfflineLatch() {
  _driverState = DriverState::OFFLINE;
  const uint8_t threshold = _config.offlineThreshold == 0 ? 1 : _config.offlineThreshold;
  if (_consecutiveFailures < threshold) {
    _consecutiveFailures = threshold;
  }
}

void INA3221::_markHardwareConfigDirty(const Status& reason) {
  if (!_hardwareConfigDirty) {
    _hardwareConfigDirty = true;
    _hardwareConfigDirtyStatus = reason;
  }
}

void INA3221::_clearHardwareConfigDirty() {
  _hardwareConfigDirty = false;
  _hardwareConfigDirtyStatus = Status::Ok();
}

// ============================================================================
// Internal
// ============================================================================

void INA3221::_clearPollJob() {
  _pollJobKind = PollJobKind::NONE;
  _pollJobStage = PollJobStage::IDLE;
  _pollSampleMode = Mode::SHUNT_BUS_TRIG;
  _pollConversionReady = false;
  _pollObservedReady = false;
  _pollConversionStartMs = 0;
  _pollNextChannel = 0;
  _pollInstructionsLast = 0;
  _pollInstructionsTotal = 0;
  _pendingMaskEnableWritable = 0;
  _resetPollSampleCache();
}

void INA3221::_resetPollSampleCache() {
  for (uint8_t i = 0; i < 3; ++i) {
    _pollChannels[i] = ChannelRawMeasurement{};
  }
}

Status INA3221::_startSampleJob(PollJobKind kind, Mode mode,
                                bool pollConversionReady) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (kind != PollJobKind::SINGLE_SHOT &&
      kind != PollJobKind::CONTINUOUS_READ) {
    return Status::Error(Err::INVALID_PARAM, "Invalid sample job kind");
  }
  if (_pollJobKind != PollJobKind::NONE &&
      _pollJobStage != PollJobStage::COMPLETE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  if (_enabledChannelCount() == 0) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  if (kind == PollJobKind::SINGLE_SHOT && !isTriggeredMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Must be a triggered mode");
  }
  if (kind == PollJobKind::CONTINUOUS_READ && !isContinuousMode(mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Continuous mode required");
  }
  if (!modeReadsShunt(mode) && !modeReadsBus(mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Sample mode required");
  }

  _clearPollJob();
  _pollJobKind = kind;
  _pollJobStage = kind == PollJobKind::SINGLE_SHOT
                      ? PollJobStage::WRITE_CONFIG
                      : (pollConversionReady ? PollJobStage::READ_READY
                                             : PollJobStage::READ_CHANNELS);
  _pollSampleMode = mode;
  _pollConversionReady = pollConversionReady;
  _pollObservedReady = !pollConversionReady &&
                       kind == PollJobKind::CONTINUOUS_READ;
  for (uint8_t i = 0; i < 3; ++i) {
    const Channel ch = static_cast<Channel>(i);
    _pollChannels[i].channelEnabled = _isChannelEnabled(ch);
  }
  return Status{Err::IN_PROGRESS, 0, "Poll job scheduled"};
}

Status INA3221::_pollSampleJob(uint32_t nowMs, uint8_t maxInstructions) {
  _pollInstructionsLast = 0;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (maxInstructions == 0) {
    return Status::Error(Err::INVALID_PARAM, "maxInstructions must be > 0");
  }
  if (_pollJobStage == PollJobStage::COMPLETE) {
    return Status::Ok();
  }
  if (_pollJobKind != PollJobKind::SINGLE_SHOT &&
      _pollJobKind != PollJobKind::CONTINUOUS_READ) {
    return Status::Error(Err::BUSY, "Sample job is not active");
  }

  uint8_t executed = 0;
  auto recordInstruction = [&]() {
    executed++;
    _pollInstructionsLast = executed;
    if (_pollInstructionsTotal < UINT16_MAX) {
      _pollInstructionsTotal++;
    }
  };

  while (executed < maxInstructions) {
    if (_pollJobStage == PollJobStage::WRITE_CONFIG) {
      Status st = _writeRegister16Tracked(cmd::REG_CONFIG,
                                          _buildConfigRegisterForMode(_pollSampleMode));
      recordInstruction();
      if (!st.ok()) {
        _markHardwareConfigDirty(st);
        return st;
      }
      _config.mode = _pollSampleMode;
      _conversionStarted = true;
      _conversionReady = false;
      _conversionStartMs = nowMs;
      _pollConversionStartMs = nowMs;
      _pollObservedReady = false;
      _pollJobStage = PollJobStage::WAIT_CONVERSION;
      continue;
    }

    if (_pollJobStage == PollJobStage::WAIT_CONVERSION) {
      const uint32_t cycleUs = getCycleTimeUs();
      const uint32_t cycleMs = (cycleUs + 999) / 1000 + 1;
      if ((nowMs - _pollConversionStartMs) < cycleMs) {
        return Status{Err::IN_PROGRESS, 0, "Waiting for conversion"};
      }
      if (_pollConversionReady) {
        _pollJobStage = PollJobStage::READ_READY;
      } else {
        _pollObservedReady = true;
        _conversionStarted = false;
        _conversionReady = true;
        _pollJobStage = PollJobStage::READ_CHANNELS;
      }
      continue;
    }

    if (_pollJobStage == PollJobStage::READ_READY) {
      uint16_t maskEn = 0;
      Status st = readRegister16(cmd::REG_MASK_ENABLE, maskEn);
      recordInstruction();
      if (!st.ok()) {
        return st;
      }
      if ((maskEn & cmd::MASK_CVRF) == 0U) {
        return Status{Err::IN_PROGRESS, 0, "Conversion not ready"};
      }
      _pollObservedReady = true;
      _conversionStarted = false;
      _conversionReady = true;
      _pollJobStage = PollJobStage::READ_CHANNELS;
      continue;
    }

    if (_pollJobStage == PollJobStage::READ_CHANNELS) {
      while (_pollNextChannel < 3U) {
        const Channel ch = static_cast<Channel>(_pollNextChannel);
        if (!_pollChannels[_pollNextChannel].channelEnabled ||
            _sampleChannelComplete(ch)) {
          _pollNextChannel++;
          continue;
        }
        Status st = _readChannelRawStep(ch);
        recordInstruction();
        if (!st.ok() && !st.inProgress()) {
          return st;
        }
        if (_sampleChannelComplete(ch)) {
          _pollNextChannel++;
        }
        break;
      }

      if (_pollNextChannel >= 3U) {
        _pollJobStage = PollJobStage::COMPLETE;
        _conversionStarted = false;
        _conversionReady = true;
        return Status::Ok();
      }
      continue;
    }

    if (_pollJobStage == PollJobStage::COMPLETE) {
      return Status::Ok();
    }

    return Status::Error(Err::INVALID_CONFIG, "Invalid poll job stage");
  }

  return _pollJobStage == PollJobStage::COMPLETE
             ? Status::Ok()
             : Status{Err::IN_PROGRESS, 0, "Poll job in progress"};
}

Status INA3221::_pollApplyMaskEnableJob(uint8_t maxInstructions) {
  _pollInstructionsLast = 0;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (maxInstructions == 0) {
    return Status::Error(Err::INVALID_PARAM, "maxInstructions must be > 0");
  }
  if (_pollJobStage == PollJobStage::COMPLETE) {
    return Status::Ok();
  }
  if (_pollJobKind != PollJobKind::APPLY_MASK_ENABLE ||
      _pollJobStage != PollJobStage::WRITE_MASK_ENABLE) {
    return Status::Error(Err::BUSY, "Mask/Enable apply job is not active");
  }

  Status st = _writeRegister16Tracked(cmd::REG_MASK_ENABLE, _pendingMaskEnableWritable);
  _pollInstructionsLast = 1;
  if (_pollInstructionsTotal < UINT16_MAX) {
    _pollInstructionsTotal++;
  }
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  _maskEnableWritableCache = _pendingMaskEnableWritable;
  _pollJobStage = PollJobStage::COMPLETE;
  return Status::Ok();
}

Status INA3221::_readChannelRawStep(Channel ch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  if (_pollJobKind != PollJobKind::SINGLE_SHOT &&
      _pollJobKind != PollJobKind::CONTINUOUS_READ) {
    return Status::Error(Err::INVALID_CONFIG, "Sample job is not active");
  }
  if (_pollJobStage != PollJobStage::READ_CHANNELS) {
    return Status::Error(Err::BUSY, "Sample job is not ready for channel reads");
  }

  const uint8_t idx = static_cast<uint8_t>(ch);
  ChannelRawMeasurement& sample = _pollChannels[idx];
  if (!sample.channelEnabled) {
    return Status::Error(Err::INVALID_CONFIG, "Channel disabled for sample job");
  }
  if (_sampleChannelComplete(ch)) {
    return Status::Ok();
  }

  uint16_t regVal = 0;
  if (_sampleModeReadsShunt() && !sample.shuntValid) {
    Status st = readRegister16(shuntRegAddr(ch), regVal);
    if (!st.ok()) {
      return st;
    }
    sample.shuntRaw = static_cast<int16_t>(regVal);
    sample.shuntValid = true;
    return _sampleChannelComplete(ch)
               ? Status::Ok()
               : Status{Err::IN_PROGRESS, 0, "Channel raw read in progress"};
  }

  if (_sampleModeReadsBus() && !sample.busValid) {
    Status st = readRegister16(busRegAddr(ch), regVal);
    if (!st.ok()) {
      return st;
    }
    sample.busRaw = static_cast<int16_t>(regVal);
    sample.busValid = true;
    return _sampleChannelComplete(ch)
               ? Status::Ok()
               : Status{Err::IN_PROGRESS, 0, "Channel raw read in progress"};
  }

  return Status::Ok();
}

Status INA3221::_applyConfig() {
  return _writeRegister16Tracked(cmd::REG_CONFIG, _buildConfigRegister());
}

Status INA3221::_readConversionReadyAt(uint32_t nowMs, bool& ready) {
  ready = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if (_conversionReady) {
    ready = true;
    return Status::Ok();
  }

  if (_isTriggeredMode()) {
    if (!_conversionStarted) {
      return Status::Ok();
    }

    const uint32_t cycleUs = getCycleTimeUs();
    const uint32_t cycleMs = (cycleUs + 999) / 1000 + 1;
    if ((nowMs - _conversionStartMs) < cycleMs) {
      return Status::Ok();
    }
  }

  if (!_isTriggeredMode() && !_isContinuousMode()) {
    return Status::Ok();
  }

  uint16_t maskEn = 0;
  Status st = readRegister16(cmd::REG_MASK_ENABLE, maskEn);
  if (!st.ok()) {
    return st;
  }

  if ((maskEn & cmd::MASK_CVRF) != 0U) {
    ready = true;
    if (_isTriggeredMode()) {
      _conversionStarted = false;
      _conversionReady = true;
    }
  }

  return Status::Ok();
}

Status INA3221::_ensureMeasurementReadyForRead() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!_isTriggeredMode()) {
    return Status::Ok();
  }

  bool ready = false;
  Status st = _readConversionReadyAt(_nowMs(), ready);
  if (!st.ok()) {
    return st;
  }
  if (!ready) {
    return Status::Error(Err::CONVERSION_NOT_READY, "Conversion not ready");
  }
  return Status::Ok();
}

void INA3221::_handleConfigWriteSideEffects() {
  _clearPollJob();
  if (_isTriggeredMode()) {
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = _nowMs();
    return;
  }

  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
}

uint16_t INA3221::_buildConfigRegister() const {
  uint16_t config = 0;
  if (_config.ch1Enable) config |= cmd::MASK_CH1EN;
  if (_config.ch2Enable) config |= cmd::MASK_CH2EN;
  if (_config.ch3Enable) config |= cmd::MASK_CH3EN;
  config |= (static_cast<uint16_t>(_config.averaging) << cmd::BIT_AVG) & cmd::MASK_AVG;
  config |= (static_cast<uint16_t>(_config.vBusCt) << cmd::BIT_VBUSCT) & cmd::MASK_VBUSCT;
  config |= (static_cast<uint16_t>(_config.vShCt) << cmd::BIT_VSHCT) & cmd::MASK_VSHCT;
  config |= (static_cast<uint16_t>(_config.mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  return config;
}

uint16_t INA3221::_buildConfigRegisterForMode(Mode mode) const {
  uint16_t config = _buildConfigRegister();
  config &= static_cast<uint16_t>(~cmd::MASK_MODE);
  config |= (static_cast<uint16_t>(mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  return config;
}

uint32_t INA3221::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return 0;
}

void INA3221::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
    return;
  }
}

uint8_t INA3221::_enabledChannelCount() const {
  uint8_t count = 0;
  if (_config.ch1Enable) count++;
  if (_config.ch2Enable) count++;
  if (_config.ch3Enable) count++;
  return count;
}

bool INA3221::_isChannelEnabled(Channel ch) const {
  switch (ch) {
    case Channel::CH1: return _config.ch1Enable;
    case Channel::CH2: return _config.ch2Enable;
    case Channel::CH3: return _config.ch3Enable;
    default: return false;
  }
}

bool INA3221::_isTriggeredMode() const {
  return isTriggeredMode(_config.mode);
}

bool INA3221::_isContinuousMode() const {
  return isContinuousMode(_config.mode);
}

bool INA3221::_sampleModeReadsShunt() const {
  return modeReadsShunt(_pollSampleMode);
}

bool INA3221::_sampleModeReadsBus() const {
  return modeReadsBus(_pollSampleMode);
}

bool INA3221::_sampleChannelComplete(Channel ch) const {
  if (!isValidChannel(ch)) {
    return true;
  }
  const uint8_t idx = static_cast<uint8_t>(ch);
  const ChannelRawMeasurement& sample = _pollChannels[idx];
  if (!sample.channelEnabled) {
    return true;
  }
  const bool shuntDone = !_sampleModeReadsShunt() || sample.shuntValid;
  const bool busDone = !_sampleModeReadsBus() || sample.busValid;
  return shuntDone && busDone;
}

} // namespace INA3221
