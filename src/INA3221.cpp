/// @file INA3221.cpp
/// @brief Implementation of INA3221 driver

#include "INA3221/INA3221.h"

#include <Arduino.h>
#include <climits>
#include <cmath>

namespace INA3221 {

namespace {

constexpr uint8_t kMinAddress = 0x40;
constexpr uint8_t kMaxAddress = 0x43;

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

bool isTriggeredMode(Mode mode) {
  return mode == Mode::SHUNT_TRIG || mode == Mode::BUS_TRIG ||
         mode == Mode::SHUNT_BUS_TRIG;
}

bool isContinuousMode(Mode mode) {
  return mode == Mode::SHUNT_CONT || mode == Mode::BUS_CONT ||
         mode == Mode::SHUNT_BUS_CONT;
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

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status INA3221::begin(const Config& config) {
  _config = config;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

  if (_config.i2cWrite == nullptr || _config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (_config.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "Timeout must be > 0");
  }
  if (_config.i2cAddress < kMinAddress || _config.i2cAddress > kMaxAddress) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address (0x40-0x43)");
  }
  if (!isValidAveraging(_config.averaging) ||
      !isValidConvTime(_config.vBusCt) ||
      !isValidConvTime(_config.vShCt) ||
      !isValidMode(_config.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid config enum value");
  }
  for (int i = 0; i < 3; ++i) {
    if (_config.shuntResistance[i] <= 0.0f) {
      return Status::Error(Err::INVALID_CONFIG, "Shunt resistance must be > 0");
    }
  }
  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  // Verify device identity
  Status st = probe();
  if (!st.ok()) {
    return st;
  }

  // Apply configuration
  st = _applyConfig();
  if (!st.ok()) {
    return st;
  }

  _initialized = true;
  _driverState = DriverState::READY;
  return Status::Ok();
}

void INA3221::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }

  if (_isTriggeredMode() && _conversionStarted && !_conversionReady) {
    uint32_t cycleUs = getCycleTimeUs();
    uint32_t cycleMs = (cycleUs + 999) / 1000 + 1;  // round up + margin
    if ((nowMs - _conversionStartMs) >= cycleMs) {
      // Check CVRF
      uint16_t maskEn = 0;
      Status st = readRegister16(cmd::REG_MASK_ENABLE, maskEn);
      if (st.ok() && (maskEn & cmd::MASK_CVRF)) {
        _conversionStarted = false;
        _conversionReady = true;
      }
    }
  }
}

void INA3221::end() {
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _conversionReady = false;
}

// ============================================================================
// Diagnostics
// ============================================================================

Status INA3221::probe() {
  uint16_t mfgId = 0;
  Status st = _readRegister16Raw(cmd::REG_MANUFACTURER_ID, mfgId);
  if (!st.ok()) {
    if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
      return st;
    }
    return Status::Error(Err::DEVICE_NOT_FOUND, "INA3221 not responding", st.detail);
  }
  if (mfgId != cmd::MANUFACTURER_ID_VALUE) {
    return Status::Error(Err::MANUFACTURER_ID_MISMATCH, "Manufacturer ID mismatch",
                         static_cast<int32_t>(mfgId));
  }

  uint16_t dieId = 0;
  st = _readRegister16Raw(cmd::REG_DIE_ID, dieId);
  if (!st.ok()) {
    if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
      return st;
    }
    return Status::Error(Err::DEVICE_NOT_FOUND, "INA3221 not responding", st.detail);
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

  uint16_t configReg = 0;
  return readRegister16(cmd::REG_CONFIG, configReg);
}

// ============================================================================
// Measurement API
// ============================================================================

Status INA3221::readShuntRaw(Channel ch, int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
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
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
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
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
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
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
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
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
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
  // Sum register: data in bits [15:1], bit 0 reserved. LSB = 40µV
  int16_t dataValue = raw >> cmd::SUM_DATA_SHIFT;
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
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  // Writing to config register triggers single-shot conversion
  uint16_t configReg = _buildConfigRegister();
  Status st = writeRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
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
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  Mode prevMode = _config.mode;
  _config.mode = mode;

  uint16_t configReg = _buildConfigRegister();
  Status st = writeRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _config.mode = prevMode;
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = _nowMs();
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

bool INA3221::conversionReady() {
  if (!_initialized) {
    return false;
  }
  if (_isContinuousMode()) {
    return true;
  }
  if (_conversionReady) {
    return true;
  }
  if (!_conversionStarted) {
    return false;
  }

  uint32_t cycleUs = getCycleTimeUs();
  uint32_t cycleMs = (cycleUs + 999) / 1000 + 1;
  uint32_t nowMs = _nowMs();
  if ((nowMs - _conversionStartMs) < cycleMs) {
    return false;
  }

  // Check CVRF in Mask/Enable register
  // NOTE: reading Mask/Enable clears CVRF and alert flags
  uint16_t maskEn = 0;
  Status st = readRegister16(cmd::REG_MASK_ENABLE, maskEn);
  if (!st.ok()) {
    return false;
  }

  if (maskEn & cmd::MASK_CVRF) {
    _conversionStarted = false;
    _conversionReady = true;
    return true;
  }

  return false;
}

Status INA3221::readBlocking(ChannelMeasurement* ch1,
                             ChannelMeasurement* ch2,
                             ChannelMeasurement* ch3,
                             uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
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

  const uint32_t nowMs = _nowMs();
  const uint32_t deadlineMs = nowMs + timeoutMs;

  // Wait for conversion ready
  while (static_cast<int32_t>(_nowMs() - deadlineMs) < 0) {
    if (conversionReady()) {
      break;
    }
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
  _config.mode = mode;
  _conversionStarted = false;
  _conversionReady = false;
  return _applyConfig();
}

Status INA3221::setAveraging(Averaging avg) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidAveraging(avg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid averaging");
  }
  _config.averaging = avg;
  return _applyConfig();
}

Status INA3221::setVBusConvTime(ConvTime ct) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  _config.vBusCt = ct;
  return _applyConfig();
}

Status INA3221::setVShuntConvTime(ConvTime ct) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  _config.vShCt = ct;
  return _applyConfig();
}

Status INA3221::setChannelEnable(Channel ch, bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  switch (ch) {
    case Channel::CH1: _config.ch1Enable = enable; break;
    case Channel::CH2: _config.ch2Enable = enable; break;
    case Channel::CH3: _config.ch3Enable = enable; break;
  }
  return _applyConfig();
}

bool INA3221::getChannelEnable(Channel ch) const {
  switch (ch) {
    case Channel::CH1: return _config.ch1Enable;
    case Channel::CH2: return _config.ch2Enable;
    case Channel::CH3: return _config.ch3Enable;
    default: return false;
  }
}

Status INA3221::setShuntResistance(Channel ch, float ohms) {
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  if (ohms <= 0.0f) {
    return Status::Error(Err::INVALID_PARAM, "Shunt resistance must be > 0");
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
  Status st = writeRegister16(cmd::REG_CONFIG, config);
  if (!st.ok()) {
    return st;
  }

  // Sync config struct from register value
  _config.ch1Enable = (config & cmd::MASK_CH1EN) != 0;
  _config.ch2Enable = (config & cmd::MASK_CH2EN) != 0;
  _config.ch3Enable = (config & cmd::MASK_CH3EN) != 0;
  _config.averaging = static_cast<Averaging>((config & cmd::MASK_AVG) >> cmd::BIT_AVG);
  _config.vBusCt = static_cast<ConvTime>((config & cmd::MASK_VBUSCT) >> cmd::BIT_VBUSCT);
  _config.vShCt = static_cast<ConvTime>((config & cmd::MASK_VSHCT) >> cmd::BIT_VSHCT);
  _config.mode = static_cast<Mode>((config & cmd::MASK_MODE) >> cmd::BIT_MODE);

  _conversionStarted = false;
  _conversionReady = false;
  return Status::Ok();
}

Status INA3221::softReset() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = writeRegister16(cmd::REG_CONFIG, cmd::MASK_RST);
  if (!st.ok()) {
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
  return writeRegister16(critRegAddr(ch), static_cast<uint16_t>(raw));
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
  return writeRegister16(warnRegAddr(ch), static_cast<uint16_t>(raw));
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
  return writeRegister16(cmd::REG_SHUNT_SUM_LIMIT, static_cast<uint16_t>(raw));
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
  return writeRegister16(cmd::REG_PV_UPPER_LIMIT, static_cast<uint16_t>(raw));
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
  return writeRegister16(cmd::REG_PV_LOWER_LIMIT, static_cast<uint16_t>(raw));
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

  return Status::Ok();
}

Status INA3221::setSummationChannels(bool ch1, bool ch2, bool ch3) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_MASK_ENABLE, regVal);
  if (!st.ok()) {
    return st;
  }

  // Clear SCC bits and set new values
  regVal &= ~(cmd::MASK_SCC1 | cmd::MASK_SCC2 | cmd::MASK_SCC3);
  if (ch1) regVal |= cmd::MASK_SCC1;
  if (ch2) regVal |= cmd::MASK_SCC2;
  if (ch3) regVal |= cmd::MASK_SCC3;

  return writeRegister16(cmd::REG_MASK_ENABLE, regVal);
}

Status INA3221::setAlertLatchEnable(bool warningLatch, bool criticalLatch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t regVal = 0;
  Status st = readRegister16(cmd::REG_MASK_ENABLE, regVal);
  if (!st.ok()) {
    return st;
  }

  regVal &= ~(cmd::MASK_WEN | cmd::MASK_CEN);
  if (warningLatch) regVal |= cmd::MASK_WEN;
  if (criticalLatch) regVal |= cmd::MASK_CEN;

  return writeRegister16(cmd::REG_MASK_ENABLE, regVal);
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
  // Data in bits [15:3], arithmetic right shift to get 13-bit signed value
  int16_t dataValue = raw >> cmd::DATA_SHIFT;
  return dataValue * cmd::SHUNT_LSB_MV;
}

float INA3221::busRawToVolts(int16_t raw) {
  int16_t dataValue = raw >> cmd::DATA_SHIFT;
  return dataValue * cmd::BUS_LSB_V;
}

int16_t INA3221::mvToShuntRaw(float mV) {
  int16_t dataValue = static_cast<int16_t>(lrintf(mV / cmd::SHUNT_LSB_MV));
  return static_cast<int16_t>(dataValue << cmd::DATA_SHIFT);
}

int16_t INA3221::voltsToBusRaw(float volts) {
  int16_t dataValue = static_cast<int16_t>(lrintf(volts / cmd::BUS_LSB_V));
  return static_cast<int16_t>(dataValue << cmd::DATA_SHIFT);
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
  return getConversionTimeUs() * _enabledChannelCount();
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
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status INA3221::_i2cWriteTracked(const uint8_t* buf, size_t len) {
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
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadTracked(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
  return Status::Ok();
}

Status INA3221::writeRegister16(uint8_t reg, uint16_t value) {
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
  uint32_t nowMs = _nowMs();

  if (st.ok() || st.inProgress()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }

    if (_initialized) {
      _driverState = DriverState::READY;
    }
  } else {
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
  }

  return st;
}

// ============================================================================
// Internal
// ============================================================================

Status INA3221::_applyConfig() {
  return writeRegister16(cmd::REG_CONFIG, _buildConfigRegister());
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

uint32_t INA3221::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return millis();
}

void INA3221::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
    return;
  }
  yield();
}

uint8_t INA3221::_enabledChannelCount() const {
  uint8_t count = 0;
  if (_config.ch1Enable) count++;
  if (_config.ch2Enable) count++;
  if (_config.ch3Enable) count++;
  return count;
}

bool INA3221::_isTriggeredMode() const {
  return isTriggeredMode(_config.mode);
}

bool INA3221::_isContinuousMode() const {
  return isContinuousMode(_config.mode);
}

} // namespace INA3221
