/// @file INA3221.cpp
/// @brief Implementation of INA3221 driver

#include "INA3221/INA3221.h"

#include <climits>
#include <cmath>
#include <limits>

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
constexpr uint16_t kDestructiveAlertEvents =
    cmd::MASK_CF1 | cmd::MASK_CF2 | cmd::MASK_CF3 | cmd::MASK_SF |
    cmd::MASK_WF1 | cmd::MASK_WF2 | cmd::MASK_WF3 | cmd::MASK_CVRF;
constexpr uint32_t kMaximumBusMilliVolts = 26000;

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

bool isWritableRegisterAddress(uint8_t reg) {
  return reg == cmd::REG_CONFIG ||
         (reg >= cmd::REG_CH1_CRIT_LIMIT && reg <= cmd::REG_CH3_WARN_LIMIT) ||
         reg == cmd::REG_SHUNT_SUM_LIMIT || reg == cmd::REG_MASK_ENABLE ||
         reg == cmd::REG_PV_UPPER_LIMIT || reg == cmd::REG_PV_LOWER_LIMIT;
}

bool isPositiveFinite(float value) {
  return std::isfinite(value) && value > 0.0f;
}

uint8_t countEnabledChannels(const DeviceProfile& profile) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < 3U; ++i) {
    if ((profile.enabledChannels & static_cast<ChannelMask>(1U << i)) != 0U)
      ++count;
  }
  return count;
}

bool modeAllowsNoChannels(Mode mode) {
  return isPowerDownMode(mode);
}

bool profileHasRequiredChannels(const DeviceProfile& profile) {
  return modeAllowsNoChannels(profile.mode) || countEnabledChannels(profile) > 0;
}

bool isChannelEnabled(const DeviceProfile& profile, Channel ch) {
  return isValidChannel(ch) &&
         (profile.enabledChannels &
          static_cast<ChannelMask>(1U << static_cast<uint8_t>(ch))) != 0U;
}

Status validateMeasurementRead(const DeviceProfile& profile, bool initialized,
                               Channel ch, bool requireShunt,
                               bool requireBus) {
  if (!initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  if (!isChannelEnabled(profile, ch)) {
    return Status::Error(Err::INVALID_CONFIG, "Channel disabled");
  }
  if (requireShunt && !modeReadsShunt(profile.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Mode does not measure shunt");
  }
  if (requireBus && !modeReadsBus(profile.mode)) {
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
  const float minimum = static_cast<float>(minValue) * lsb;
  const float maximum = static_cast<float>(maxValue) * lsb;
  long scaled = minValue;
  if (value >= maximum) scaled = maxValue;
  else if (value > minimum) scaled = lrintf(value / lsb);
  const uint16_t encoded = static_cast<uint16_t>(static_cast<int16_t>(scaled));
  return static_cast<int16_t>(encoded << shift);
}

float applyCurrentDirection(float milliAmps,
                            const ShuntCalibration& calibration) {
  return calibration.direction ==
                 CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT
             ? -milliAmps
             : milliAmps;
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

uint32_t convTimeMaximumUs(ConvTime ct) {
  static constexpr uint32_t table[] = {
    154, 224, 365, 646, 1210, 2328, 4572, 9068
  };
  const uint8_t idx = static_cast<uint8_t>(ct);
  return idx < sizeof(table) / sizeof(table[0]) ? table[idx] : 0U;
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
  return static_cast<uint8_t>(cmd::SHUNT_REG_BASE +
                              static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE);
}

/// Get bus register address for channel
uint8_t busRegAddr(Channel ch) {
  return static_cast<uint8_t>(cmd::BUS_REG_BASE +
                              static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE);
}

/// Get critical limit register address for channel
uint8_t critRegAddr(Channel ch) {
  return static_cast<uint8_t>(cmd::CRIT_REG_BASE +
                              static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE);
}

/// Get warning limit register address for channel
uint8_t warnRegAddr(Channel ch) {
  return static_cast<uint8_t>(cmd::WARN_REG_BASE +
                              static_cast<uint8_t>(ch) * cmd::CHANNEL_STRIDE);
}

} // namespace

// ============================================================================
// Pure contracts and cooperative owner binding
// ============================================================================

bool INA3221::isReadableRegister(uint8_t reg) {
  return isValidRegister(reg);
}

bool INA3221::isWritableRegister(uint8_t reg) {
  return isWritableRegisterAddress(reg);
}

Status INA3221::conversionTiming(ConvTime ct, ConversionTiming& out) {
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  out.typicalUs = convTimeUs(ct);
  out.maximumUs = convTimeMaximumUs(ct);
  return Status::Ok();
}

Status INA3221::maximumCycleTimeUs(const DeviceProfile& profile, Mode mode,
                                   uint64_t& outUs) {
  outUs = 0;
  if (!isValidMode(mode) || !isValidAveraging(profile.averaging) ||
      !isValidConvTime(profile.vBusCt) || !isValidConvTime(profile.vShCt) ||
      (profile.enabledChannels & static_cast<ChannelMask>(~ALL_CHANNELS)) != 0U) {
    return Status::Error(Err::INVALID_PARAM, "Invalid timing profile");
  }
  if (isPowerDownMode(mode)) {
    return Status::Ok();
  }
  const uint8_t channelCount = static_cast<uint8_t>(
      ((profile.enabledChannels & CHANNEL_1) != 0U ? 1U : 0U) +
      ((profile.enabledChannels & CHANNEL_2) != 0U ? 1U : 0U) +
      ((profile.enabledChannels & CHANNEL_3) != 0U ? 1U : 0U));
  if (channelCount == 0U) {
    return Status::Error(Err::INVALID_CONFIG, "No enabled channel");
  }
  uint64_t perChannel = 0;
  if (modeReadsShunt(mode)) perChannel += convTimeMaximumUs(profile.vShCt);
  if (modeReadsBus(mode)) perChannel += convTimeMaximumUs(profile.vBusCt);
  const uint64_t samples = averagingSampleCount(profile.averaging);
  if (perChannel > std::numeric_limits<uint64_t>::max() / channelCount ||
      perChannel * channelCount > std::numeric_limits<uint64_t>::max() / samples) {
    return Status::Error(Err::ARITHMETIC_OVERFLOW, "Cycle time overflow");
  }
  outUs = perChannel * channelCount * samples;
  return Status::Ok();
}

Status INA3221::maximumJobTransfers(JobKind kind,
                                    const DeviceProfile& profile,
                                    uint16_t& outTransfers) {
  Status st = _validateProfile(profile);
  if (!st.ok()) return st;
  const uint8_t channels = static_cast<uint8_t>(
      ((profile.enabledChannels & CHANNEL_1) != 0U ? 1U : 0U) +
      ((profile.enabledChannels & CHANNEL_2) != 0U ? 1U : 0U) +
      ((profile.enabledChannels & CHANNEL_3) != 0U ? 1U : 0U));
  switch (kind) {
    case JobKind::INITIALIZE:
      outTransfers = static_cast<uint16_t>(2U + 3U * MANAGED_PROFILE_REGISTER_COUNT);
      return Status::Ok();
    case JobKind::APPLY_PROFILE:
    case JobKind::RECONCILE:
      outTransfers = static_cast<uint16_t>(3U * MANAGED_PROFILE_REGISTER_COUNT);
      return Status::Ok();
    case JobKind::TRIGGERED_SAMPLE:
      outTransfers = static_cast<uint16_t>(2U + 2U * channels);
      return Status::Ok();
    case JobKind::CONTINUOUS_SAMPLE:
      outTransfers = static_cast<uint16_t>(1U + 2U * channels);
      return Status::Ok();
    case JobKind::POWER_DOWN:
      outTransfers = 3U;
      return Status::Ok();
    case JobKind::NONE:
    default:
      return Status::Error(Err::INVALID_PARAM, "Invalid job kind");
  }
}

Status INA3221::decodeShuntMicroVolts(uint16_t raw, int32_t& outMicroVolts) {
  outMicroVolts = static_cast<int32_t>(signExtendField(raw, cmd::DATA_SHIFT, 13)) * 40;
  return Status::Ok();
}

Status INA3221::decodeBusMilliVolts(uint16_t raw, int32_t& outMilliVolts) {
  outMilliVolts = static_cast<int32_t>(signExtendField(raw, cmd::DATA_SHIFT, 13)) * 8;
  return Status::Ok();
}

Status INA3221::channelIndex(Channel channel, uint8_t& outIndex) {
  if (!isValidChannel(channel)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  outIndex = static_cast<uint8_t>(channel);
  return Status::Ok();
}

Status INA3221::channelBit(Channel channel, ChannelMask& outBit) {
  uint8_t index = 0;
  Status st = channelIndex(channel, index);
  if (!st.ok()) return st;
  outBit = static_cast<ChannelMask>(1U << index);
  return Status::Ok();
}

Status INA3221::contains(ChannelMask mask, Channel channel,
                         bool& outContains) {
  outContains = false;
  if ((mask & static_cast<ChannelMask>(~ALL_CHANNELS)) != 0U) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel mask");
  }
  ChannelMask bit = 0;
  Status st = channelBit(channel, bit);
  if (!st.ok()) return st;
  outContains = (mask & bit) != 0U;
  return Status::Ok();
}

Status INA3221::enabledChannelCount(ChannelMask mask, uint8_t& outCount) {
  outCount = 0;
  if ((mask & static_cast<ChannelMask>(~ALL_CHANNELS)) != 0U) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel mask");
  }
  if ((mask & CHANNEL_1) != 0U) ++outCount;
  if ((mask & CHANNEL_2) != 0U) ++outCount;
  if ((mask & CHANNEL_3) != 0U) ++outCount;
  return Status::Ok();
}

int64_t INA3221::_roundDivide(int64_t numerator, int64_t denominator) {
  if (denominator <= 0) return 0;
  const int64_t half = denominator / 2;
  return numerator >= 0 ? (numerator + half) / denominator
                        : (numerator - half) / denominator;
}

Status INA3221::encodeShuntMicroVolts(int32_t microVolts, uint16_t& outRaw) {
  const int64_t code = _roundDivide(microVolts, 40);
  if (code < -4096 || code > 4095) {
    return Status::Error(Err::OUT_OF_RANGE, "Shunt voltage out of range");
  }
  outRaw = static_cast<uint16_t>(static_cast<int16_t>(code)) << cmd::DATA_SHIFT;
  return Status::Ok();
}

Status INA3221::encodeBusMilliVolts(int32_t milliVolts, uint16_t& outRaw) {
  const int64_t code = _roundDivide(milliVolts, 8);
  if (code < -4096 || code > 4095) {
    return Status::Error(Err::OUT_OF_RANGE, "Bus voltage out of range");
  }
  outRaw = static_cast<uint16_t>(static_cast<int16_t>(code)) << cmd::DATA_SHIFT;
  return Status::Ok();
}

Status INA3221::encodeShuntSumMicroVolts(int32_t microVolts, uint16_t& outRaw) {
  const int64_t code = _roundDivide(microVolts, 40);
  if (code < -16384 || code > 16383) {
    return Status::Error(Err::OUT_OF_RANGE, "Shunt sum out of range");
  }
  outRaw = static_cast<uint16_t>(static_cast<int16_t>(code)) << cmd::SUM_DATA_SHIFT;
  return Status::Ok();
}

Status INA3221::validatePowerValidWindow(uint32_t lowerMilliVolts,
                                         uint32_t upperMilliVolts) {
  if (lowerMilliVolts >= upperMilliVolts || upperMilliVolts > kMaximumBusMilliVolts) {
    return Status::Error(Err::OUT_OF_RANGE, "Invalid power-valid window");
  }
  uint16_t ignored = 0;
  Status st = encodeBusMilliVolts(static_cast<int32_t>(lowerMilliVolts), ignored);
  if (!st.ok()) return st;
  return encodeBusMilliVolts(static_cast<int32_t>(upperMilliVolts), ignored);
}

Status INA3221::calculateCurrentMilliAmps(
    int32_t shuntMicroVolts, const ShuntCalibration& calibration,
    int32_t& outMilliAmps) {
  if (calibration.resistanceMicroOhms == 0U ||
      static_cast<uint8_t>(calibration.direction) >
          static_cast<uint8_t>(CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid shunt calibration");
  }
  int64_t numerator = static_cast<int64_t>(shuntMicroVolts) * 1000;
  if (calibration.direction ==
      CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT) {
    numerator = -numerator;
  }
  const int64_t result = _roundDivide(numerator, calibration.resistanceMicroOhms);
  if (result < INT32_MIN || result > INT32_MAX) {
    return Status::Error(Err::ARITHMETIC_OVERFLOW, "Current overflow");
  }
  outMilliAmps = static_cast<int32_t>(result);
  return Status::Ok();
}

Status INA3221::calculatePowerMilliWatts(int32_t busMilliVolts,
                                         int32_t currentMilliAmps,
                                         int32_t& outMilliWatts) {
  const int64_t result = _roundDivide(
      static_cast<int64_t>(busMilliVolts) * currentMilliAmps, 1000);
  if (result < INT32_MIN || result > INT32_MAX) {
    return Status::Error(Err::ARITHMETIC_OVERFLOW, "Power overflow");
  }
  outMilliWatts = static_cast<int32_t>(result);
  return Status::Ok();
}

Status INA3221::convertRawChannel(uint16_t shuntRaw, uint16_t busRaw,
                                  QuantityMask requestedRawQuantities,
                                  const ShuntCalibration& calibration,
                                  FixedChannelReading& out) {
  out = FixedChannelReading{};
  const QuantityMask shuntBit = static_cast<QuantityMask>(Quantity::SHUNT);
  const QuantityMask busBit = static_cast<QuantityMask>(Quantity::BUS);
  const QuantityMask allowed = static_cast<QuantityMask>(shuntBit | busBit);
  if (requestedRawQuantities == 0U ||
      (requestedRawQuantities & static_cast<QuantityMask>(~allowed)) != 0U) {
    return Status::Error(Err::INVALID_PARAM, "Invalid requested raw quantities");
  }
  if ((requestedRawQuantities & shuntBit) != 0U) {
    Status st = decodeShuntMicroVolts(shuntRaw, out.shuntMicroVolts);
    if (!st.ok()) return st;
    out.validQuantities |= shuntBit;
    st = calculateCurrentMilliAmps(out.shuntMicroVolts, calibration,
                                   out.currentMilliAmps);
    if (!st.ok()) return st;
    out.validQuantities |= static_cast<QuantityMask>(Quantity::CURRENT);
  }
  if ((requestedRawQuantities & busBit) != 0U) {
    Status st = decodeBusMilliVolts(busRaw, out.busMilliVolts);
    if (!st.ok()) return st;
    if (out.busMilliVolts >= 0 &&
        out.busMilliVolts <= static_cast<int32_t>(kMaximumBusMilliVolts)) {
      out.validQuantities |= busBit;
    }
  }
  const QuantityMask powerInputs = static_cast<QuantityMask>(
      busBit | static_cast<QuantityMask>(Quantity::CURRENT));
  if ((out.validQuantities & powerInputs) == powerInputs) {
    Status st = calculatePowerMilliWatts(out.busMilliVolts,
                                         out.currentMilliAmps,
                                         out.powerMilliWatts);
    if (!st.ok()) return st;
    out.validQuantities |= static_cast<QuantityMask>(Quantity::POWER);
  }
  return Status::Ok();
}

Status INA3221::_validateTransport(const TransportConfig& transport) const {
  if (transport.i2cWrite == nullptr || transport.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (transport.defaultTransferTimeoutMs == 0U) {
    return Status::Error(Err::INVALID_CONFIG, "Transfer timeout must be non-zero");
  }
  return Status::Ok();
}

Status INA3221::_validateProfile(const DeviceProfile& profile) {
  if (profile.i2cAddress < kMinAddress || profile.i2cAddress > kMaxAddress) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address");
  }
  if ((profile.enabledChannels & static_cast<ChannelMask>(~ALL_CHANNELS)) != 0U ||
      (profile.alerts.summationChannels & static_cast<ChannelMask>(~ALL_CHANNELS)) != 0U) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid channel mask");
  }
  if (!isValidAveraging(profile.averaging) || !isValidConvTime(profile.vBusCt) ||
      !isValidConvTime(profile.vShCt) || !isValidMode(profile.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid profile enum");
  }
  if (!isPowerDownMode(profile.mode) && profile.enabledChannels == 0U) {
    return Status::Error(Err::INVALID_CONFIG, "No enabled channel");
  }
  if ((profile.alerts.summationChannels &
       static_cast<ChannelMask>(~profile.enabledChannels)) != 0U) {
    return Status::Error(Err::INVALID_CONFIG, "Summation channel disabled");
  }
  for (uint8_t i = 0; i < 3U; ++i) {
    const bool enabled = (profile.enabledChannels & static_cast<ChannelMask>(1U << i)) != 0U;
    if (enabled && profile.shunts[i].resistanceMicroOhms == 0U) {
      return Status::Error(Err::INVALID_CONFIG, "Enabled channel lacks shunt calibration");
    }
    if (static_cast<uint8_t>(profile.shunts[i].direction) > 1U) {
      return Status::Error(Err::INVALID_CONFIG, "Invalid current direction");
    }
    uint16_t encoded = 0;
    Status st = encodeShuntMicroVolts(profile.alerts.criticalLimitMicroVolts[i], encoded);
    if (!st.ok()) return st;
    st = encodeShuntMicroVolts(profile.alerts.warningLimitMicroVolts[i], encoded);
    if (!st.ok()) return st;
  }
  uint16_t encoded = 0;
  Status st = encodeShuntSumMicroVolts(profile.alerts.shuntSumLimitMicroVolts, encoded);
  if (!st.ok()) return st;
  return validatePowerValidWindow(profile.alerts.powerValidLowerMilliVolts,
                                  profile.alerts.powerValidUpperMilliVolts);
}

Status INA3221::bind(const TransportConfig& transport,
                     const DeviceProfile& profile) {
  Status st = _validateTransport(transport);
  if (!st.ok()) return st;
  st = _validateProfile(profile);
  if (!st.ok()) return st;
  if (_jobState == JobTerminalState::ACTIVE) {
    return Status::Error(Err::JOB_BUSY, "Owner job active");
  }
  if (_hasPendingJobResult) {
    return Status::Error(Err::RESULT_PENDING, "Take terminal result first");
  }
  unbind();
  _transport = transport;
  if (_transport.offlineThreshold == 0U) _transport.offlineThreshold = 1U;
  _profile = profile;
  _pendingProfile = profile;
  _bound = true;
  _measurementConfigState = AppliedConfigState::UNKNOWN;
  _alertConfigState = AppliedConfigState::UNKNOWN;
  _syncLegacyConfigViewFromProfile();
  return Status::Ok();
}

void INA3221::unbind() {
  _bound = false;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _transport = TransportConfig{};
  _legacyConfigView = Config{};
  _measurementConfigState = AppliedConfigState::UNKNOWN;
  _alertConfigState = AppliedConfigState::UNKNOWN;
  _profileGeneration = 0;
  _clearLegacyConversionState();
  _maskEnableWritableCache = 0;
  _lastCallbackInvoked = false;
  _clearPollJob();
  _resetOperationState();
  _hasPendingJobResult = false;
  _hasLastGoodSample = false;
  _retainedAlerts = AlertSnapshot{};
  _clearHardwareConfigDirty();
}

void INA3221::_syncLegacyConfigViewFromProfile() {
  _legacyConfigView.i2cWrite = _transport.i2cWrite;
  _legacyConfigView.i2cWriteRead = _transport.i2cWriteRead;
  _legacyConfigView.i2cUser = _transport.i2cUser;
  _legacyConfigView.nowMs = _transport.nowMs;
  _legacyConfigView.cooperativeYield = _transport.cooperativeYield;
  _legacyConfigView.timeUser = _transport.timeUser;
  _legacyConfigView.i2cTimeoutMs = _transport.defaultTransferTimeoutMs;
  _legacyConfigView.offlineThreshold = _transport.offlineThreshold;
  _legacyConfigView.i2cAddress = _profile.i2cAddress;
  _legacyConfigView.ch1Enable = (_profile.enabledChannels & CHANNEL_1) != 0U;
  _legacyConfigView.ch2Enable = (_profile.enabledChannels & CHANNEL_2) != 0U;
  _legacyConfigView.ch3Enable = (_profile.enabledChannels & CHANNEL_3) != 0U;
  _legacyConfigView.averaging = _profile.averaging;
  _legacyConfigView.vBusCt = _profile.vBusCt;
  _legacyConfigView.vShCt = _profile.vShCt;
  _legacyConfigView.mode = _profile.mode;
  for (uint8_t i = 0; i < 3U; ++i) {
    _legacyConfigView.shuntResistance[i] =
        static_cast<float>(_profile.shunts[i].resistanceMicroOhms) / 1000000.0f;
  }
}

Status INA3221::_legacyToContracts(const Config& config,
                                   TransportConfig& transport,
                                   DeviceProfile& profile) {
  transport.i2cWrite = config.i2cWrite;
  transport.i2cWriteRead = config.i2cWriteRead;
  transport.i2cUser = config.i2cUser;
  transport.nowMs = config.nowMs;
  transport.cooperativeYield = config.cooperativeYield;
  transport.timeUser = config.timeUser;
  transport.defaultTransferTimeoutMs = config.i2cTimeoutMs;
  transport.offlineThreshold = config.offlineThreshold;
  profile.i2cAddress = config.i2cAddress;
  profile.enabledChannels = static_cast<ChannelMask>(
      (config.ch1Enable ? CHANNEL_1 : 0U) |
      (config.ch2Enable ? CHANNEL_2 : 0U) |
      (config.ch3Enable ? CHANNEL_3 : 0U));
  profile.averaging = config.averaging;
  profile.vBusCt = config.vBusCt;
  profile.vShCt = config.vShCt;
  profile.mode = config.mode;
  for (uint8_t i = 0; i < 3U; ++i) {
    const bool enabled =
        (profile.enabledChannels & static_cast<ChannelMask>(1U << i)) != 0U;
    if (!isPositiveFinite(config.shuntResistance[i])) {
      if (!enabled) {
        profile.shunts[i].resistanceMicroOhms = 0U;
        continue;
      }
      return Status::Error(Err::INVALID_CONFIG, "Invalid shunt resistance");
    }
    const double microOhms = static_cast<double>(config.shuntResistance[i]) * 1000000.0;
    if (microOhms < 1.0 || microOhms > UINT32_MAX) {
      if (!enabled) {
        profile.shunts[i].resistanceMicroOhms = 0U;
        continue;
      }
      return Status::Error(Err::OUT_OF_RANGE, "Shunt resistance out of range");
    }
    profile.shunts[i].resistanceMicroOhms =
        static_cast<uint32_t>(microOhms + 0.5);
  }
  return Status::Ok();
}

Status INA3221::_requireNoOwnerJob() const {
  return _jobState == JobTerminalState::ACTIVE
             ? Status::Error(Err::JOB_BUSY, "Owner job active")
             : Status::Ok();
}

void INA3221::_resetOperationState() {
  _jobKind = JobKind::NONE;
  _jobStage = OperationStage::IDLE;
  _jobState = JobTerminalState::IDLE;
  _jobRequestId = 0;
  _jobDeadlineMs = 0;
  _jobTransfers = 0;
  _jobTransfersLastPoll = 0;
  _jobProfileIndex = 0;
  _jobChannelIndex = 0;
  _jobConsumeAlerts = false;
  _jobAnyWriteConfirmed = false;
  _jobSampleMode = Mode::SHUNT_BUS_TRIG;
  _jobConversionStartMs = 0;
  _jobReadyAtMs = 0;
  _jobWaitDurationMs = 0;
  _jobWaitOriginAfterMs = 0;
  _jobDesiredRegisterValue = 0;
  _jobDesiredRegisterAddress = 0;
  _sampleWork = SampleBatch{};
  for (uint8_t i = 0; i < 3U; ++i) _sampleRaw[i] = ChannelRawMeasurement{};
}

Status INA3221::_startJob(JobKind kind, uint32_t requestId,
                          uint64_t deadlineMs) {
  if (!_bound) return Status::Error(Err::NOT_INITIALIZED, "Driver not bound");
  if (_hasPendingJobResult) {
    return Status::Error(Err::RESULT_PENDING, "Take terminal result first");
  }
  if (_jobState == JobTerminalState::ACTIVE) {
    return Status::Error(Err::JOB_BUSY, "Owner job active");
  }
  if (deadlineMs == 0U) {
    return Status::Error(Err::INVALID_PARAM, "Absolute deadline required");
  }
  if (requestId == 0U) {
    return Status::Error(Err::INVALID_PARAM, "Request ID must be non-zero");
  }
  if (_conversionStarted &&
      (kind == JobKind::TRIGGERED_SAMPLE ||
       kind == JobKind::CONTINUOUS_SAMPLE)) {
    return Status::Error(Err::CONVERSION_BUSY, "Legacy conversion active");
  }
  // Every accepted owner job takes over conversion provenance. Sample jobs
  // reject an active legacy trigger above; completed legacy-ready evidence and
  // lifecycle-superseded bookkeeping are cache-only and safe to discard.
  _clearLegacyConversionState();
  _clearPollJob();
  _resetOperationState();
  _jobKind = kind;
  _jobState = JobTerminalState::ACTIVE;
  _jobRequestId = requestId;
  _jobDeadlineMs = deadlineMs;
  return Status{Err::IN_PROGRESS, 0, "Owner job started"};
}

Status INA3221::_startProfileJob(JobKind kind, const DeviceProfile& profile,
                                 uint32_t requestId, uint64_t deadlineMs) {
  Status st = _validateProfile(profile);
  if (!st.ok()) return st;
  if (_bound && profile.i2cAddress != _profile.i2cAddress) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Profile address change requires rebind");
  }
  st = _startJob(kind, requestId, deadlineMs);
  if (!st.inProgress()) return st;
  _pendingProfile = profile;
  _jobStage = kind == JobKind::INITIALIZE
                  ? OperationStage::READ_MANUFACTURER_ID
                  : OperationStage::PROFILE_READ;
  return st;
}

Status INA3221::startInitialize(uint32_t requestId, uint64_t deadlineMs) {
  return _startProfileJob(JobKind::INITIALIZE, _profile, requestId, deadlineMs);
}

Status INA3221::startApplyProfile(const DeviceProfile& profile,
                                  uint32_t requestId, uint64_t deadlineMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _startProfileJob(JobKind::APPLY_PROFILE, profile, requestId, deadlineMs);
}

Status INA3221::startReconcile(uint32_t requestId, uint64_t deadlineMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _startProfileJob(JobKind::RECONCILE, _profile, requestId, deadlineMs);
}

Status INA3221::startTriggeredSample(Mode mode, uint32_t requestId,
                                     uint64_t deadlineMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isTriggeredMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Triggered sample mode required");
  }
  if (_profile.mode != mode) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Triggered sample mode must match verified profile");
  }
  if (_measurementConfigState != AppliedConfigState::APPLIED) {
    return Status::Error(Err::CONFIG_UNKNOWN, "Measurement profile not verified");
  }
  Status st = _startJob(JobKind::TRIGGERED_SAMPLE, requestId, deadlineMs);
  if (!st.inProgress()) return st;
  _jobSampleMode = mode;
  _jobStage = OperationStage::SAMPLE_WRITE_CONFIG;
  _sampleWork.enabledChannels = _profile.enabledChannels;
  _sampleWork.coherence = SampleCoherence::TRIGGERED_ATOMIC;
  _sampleWork.profileGeneration = _profileGeneration;
  _sampleWork.requestId = requestId;
  for (uint8_t i = 0; i < 3U; ++i) {
    _sampleRaw[i].channelEnabled =
        (_profile.enabledChannels & static_cast<ChannelMask>(1U << i)) != 0U;
  }
  return st;
}

Status INA3221::startContinuousSample(uint32_t requestId, uint64_t deadlineMs,
                                      bool consumeAlertSnapshot) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isContinuousMode(_profile.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Continuous profile required");
  }
  if (_measurementConfigState != AppliedConfigState::APPLIED) {
    return Status::Error(Err::CONFIG_UNKNOWN, "Measurement profile not verified");
  }
  Status st = _startJob(JobKind::CONTINUOUS_SAMPLE, requestId, deadlineMs);
  if (!st.inProgress()) return st;
  _jobSampleMode = _profile.mode;
  _jobConsumeAlerts = consumeAlertSnapshot;
  _jobStage = consumeAlertSnapshot ? OperationStage::SAMPLE_READ_MASK
                                    : OperationStage::SAMPLE_READ_CHANNELS;
  _sampleWork.enabledChannels = _profile.enabledChannels;
  _sampleWork.coherence = SampleCoherence::CONTINUOUS_MIXED_AGE;
  _sampleWork.profileGeneration = _profileGeneration;
  _sampleWork.requestId = requestId;
  for (uint8_t i = 0; i < 3U; ++i) {
    _sampleRaw[i].channelEnabled =
        (_profile.enabledChannels & static_cast<ChannelMask>(1U << i)) != 0U;
  }
  return st;
}

Status INA3221::startPowerDown(uint32_t requestId, uint64_t deadlineMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = _startJob(JobKind::POWER_DOWN, requestId, deadlineMs);
  if (!st.inProgress()) return st;
  _pendingProfile = _profile;
  _pendingProfile.mode = Mode::POWER_DOWN;
  _jobStage = OperationStage::POWER_READ_CONFIG;
  return st;
}

uint64_t INA3221::_effectiveDeadline(const PollContext& context) const {
  if (_jobDeadlineMs == 0U) return context.deadlineMs;
  if (context.deadlineMs == 0U) return _jobDeadlineMs;
  return context.deadlineMs < _jobDeadlineMs ? context.deadlineMs : _jobDeadlineMs;
}

bool INA3221::_deadlineExpired(const PollContext& context) const {
  const uint64_t deadline = _effectiveDeadline(context);
  return deadline != 0U && context.nowMs >= deadline;
}

uint32_t INA3221::_clampedTransferTimeout(const PollContext& context) const {
  uint32_t timeout = _transport.defaultTransferTimeoutMs;
  if (context.transferTimeoutMs != 0U &&
      context.transferTimeoutMs < timeout) {
    timeout = context.transferTimeoutMs;
  }
  const uint64_t deadline = _effectiveDeadline(context);
  if (deadline != 0U) {
    if (context.nowMs >= deadline) return 0U;
    const uint64_t remaining = deadline - context.nowMs;
    const uint32_t remainingMs = remaining > UINT32_MAX
                                     ? UINT32_MAX
                                     : static_cast<uint32_t>(remaining);
    const uint32_t perTransferDeadlineMs =
        context.maxTransfers > 1U
            ? remainingMs / static_cast<uint32_t>(context.maxTransfers)
            : remainingMs;
    if (timeout > perTransferDeadlineMs) timeout = perTransferDeadlineMs;
  }
  return timeout;
}

Status INA3221::_readRegister16WithTimeout(uint8_t reg, uint16_t& value,
                                           uint32_t timeoutMs, bool tracked) {
  if (reg == cmd::REG_MASK_ENABLE) {
    return _readMaskEnableWithTimeout(value, nullptr, timeoutMs, tracked);
  }
  uint8_t rx[2] = {0, 0};
  Status st = tracked
                  ? _i2cWriteReadTracked(&reg, 1U, rx, sizeof(rx), timeoutMs)
                  : _i2cWriteReadRaw(&reg, 1U, rx, sizeof(rx), timeoutMs);
  if (!st.ok()) return st;
  value = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8U) | rx[1]);
  return Status::Ok();
}

Status INA3221::_writeRegister16WithTimeout(uint8_t reg, uint16_t value,
                                            uint32_t timeoutMs, bool tracked) {
  const uint8_t tx[3] = {
      reg,
      static_cast<uint8_t>((value >> 8U) & 0xFFU),
      static_cast<uint8_t>(value & 0xFFU)};
  return tracked ? _i2cWriteTracked(tx, sizeof(tx), timeoutMs)
                 : _i2cWriteRaw(tx, sizeof(tx), timeoutMs);
}

void INA3221::_retainMaskEnable(uint16_t raw, AlertSnapshot* consumed) {
  AlertSnapshot current{};
  current.raw = raw;
  current.events = static_cast<uint16_t>(raw & kDestructiveAlertEvents);
  current.writableBits = static_cast<uint16_t>(raw & kMaskEnableWritable);
  current.powerValid = (raw & cmd::MASK_PVF) != 0U;
  current.timingControl = (raw & cmd::MASK_TCF) != 0U;
  current.timingControlFault = !current.timingControl;
  current.conversionReady = (raw & cmd::MASK_CVRF) != 0U;
  _retainedAlerts.raw = raw;
  _retainedAlerts.events = static_cast<uint16_t>(
      _retainedAlerts.events | current.events);
  _retainedAlerts.writableBits = current.writableBits;
  _retainedAlerts.powerValid = current.powerValid;
  _retainedAlerts.timingControl = current.timingControl;
  _retainedAlerts.timingControlFault = current.timingControlFault;
  _retainedAlerts.conversionReady = current.conversionReady;
  _maskEnableWritableCache = current.writableBits;
  if (consumed != nullptr) {
    consumed->raw = current.raw;
    consumed->events = static_cast<uint16_t>(consumed->events | current.events);
    consumed->writableBits = current.writableBits;
    consumed->powerValid = current.powerValid;
    consumed->timingControl = current.timingControl;
    consumed->timingControlFault = current.timingControlFault;
    consumed->conversionReady = current.conversionReady;
  }
}

void INA3221::_decodeAlertFlags(uint16_t raw, AlertFlags& flags) {
  flags.criticalCh1 = (raw & cmd::MASK_CF1) != 0U;
  flags.criticalCh2 = (raw & cmd::MASK_CF2) != 0U;
  flags.criticalCh3 = (raw & cmd::MASK_CF3) != 0U;
  flags.summation = (raw & cmd::MASK_SF) != 0U;
  flags.warningCh1 = (raw & cmd::MASK_WF1) != 0U;
  flags.warningCh2 = (raw & cmd::MASK_WF2) != 0U;
  flags.warningCh3 = (raw & cmd::MASK_WF3) != 0U;
  flags.powerValid = (raw & cmd::MASK_PVF) != 0U;
  flags.timingControl = (raw & cmd::MASK_TCF) != 0U;
  flags.timingControlFault = !flags.timingControl;
  flags.conversionReady = (raw & cmd::MASK_CVRF) != 0U;
}

Status INA3221::_readMaskEnableWithTimeout(uint16_t& value,
                                           AlertSnapshot* consumed,
                                           uint32_t timeoutMs, bool tracked) {
  const uint8_t reg = cmd::REG_MASK_ENABLE;
  uint8_t rx[2] = {0, 0};
  Status st = tracked
                  ? _i2cWriteReadTracked(&reg, 1U, rx, sizeof(rx), timeoutMs)
                  : _i2cWriteReadRaw(&reg, 1U, rx, sizeof(rx), timeoutMs);
  if (!st.ok() && _writeMayHaveReachedDevice(st)) {
    // A failed combined transfer may still have reached this destructive read.
    // The callback does not expose a trustworthy partial-byte count, so make
    // possible lost alert evidence observable until the application takes it.
    _retainedAlerts.evidenceUncertain = true;
    if (consumed != nullptr) consumed->evidenceUncertain = true;
  }
  if (!st.ok()) return st;
  value = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8U) | rx[1]);
  _retainMaskEnable(value, consumed);
  return Status::Ok();
}

Status INA3221::_jobReadRegister(uint8_t reg, uint16_t& value,
                                 const PollContext& context,
                                 uint8_t& transfersLeft) {
  _lastCallbackInvoked = false;
  if (transfersLeft == 0U) return Status{Err::IN_PROGRESS, 0, "Transfer budget exhausted"};
  const uint32_t timeout = _clampedTransferTimeout(context);
  if (timeout == 0U) return Status::Error(Err::DEADLINE_EXPIRED, "Deadline expired");
  --transfersLeft;
  if (_jobTransfers < UINT16_MAX) ++_jobTransfers;
  return _readRegister16WithTimeout(reg, value, timeout, true);
}

Status INA3221::_jobWriteRegister(uint8_t reg, uint16_t value,
                                  const PollContext& context,
                                  uint8_t& transfersLeft) {
  _lastCallbackInvoked = false;
  if (transfersLeft == 0U) return Status{Err::IN_PROGRESS, 0, "Transfer budget exhausted"};
  const uint32_t timeout = _clampedTransferTimeout(context);
  if (timeout == 0U) return Status::Error(Err::DEADLINE_EXPIRED, "Deadline expired");
  --transfersLeft;
  if (_jobTransfers < UINT16_MAX) ++_jobTransfers;
  return _writeRegister16WithTimeout(reg, value, timeout, true);
}

bool INA3221::_writeMayHaveReachedDevice(const Status& status) const {
  return _lastCallbackInvoked &&
         status.code != Err::I2C_NACK_ADDR &&
         status.code != Err::DEVICE_NOT_FOUND &&
         status.code != Err::INVALID_CONFIG &&
         status.code != Err::INVALID_PARAM;
}

bool INA3221::_registerIsMeasurementConfig(uint8_t reg) {
  return reg == cmd::REG_CONFIG;
}

void INA3221::_markRegisterUnknown(uint8_t reg) {
  if (_registerIsMeasurementConfig(reg)) {
    _measurementConfigState = AppliedConfigState::UNKNOWN;
  } else if (isWritableRegisterAddress(reg)) {
    _alertConfigState = AppliedConfigState::UNKNOWN;
  }
  _hardwareConfigDirty = true;
  _hardwareConfigDirtyStatus =
      Status::Error(Err::CONFIG_UNKNOWN, "Hardware register state unknown", reg);
}

void INA3221::_markRegisterDirty(uint8_t reg) {
  if (_registerIsMeasurementConfig(reg)) {
    if (_measurementConfigState != AppliedConfigState::UNKNOWN)
      _measurementConfigState = AppliedConfigState::DIRTY;
  } else if (isWritableRegisterAddress(reg)) {
    if (_alertConfigState != AppliedConfigState::UNKNOWN)
      _alertConfigState = AppliedConfigState::DIRTY;
  }
  _hardwareConfigDirty = true;
  _hardwareConfigDirtyStatus =
      Status::Error(Err::CONFIG_UNKNOWN, "Hardware profile requires verification", reg);
}

Status INA3221::_desiredRegister(uint8_t index, const DeviceProfile& profile,
                                 uint8_t& reg, uint16_t& value) const {
  value = 0;
  if (index == 0U) {
    reg = cmd::REG_CONFIG;
    if ((profile.enabledChannels & CHANNEL_1) != 0U) value |= cmd::MASK_CH1EN;
    if ((profile.enabledChannels & CHANNEL_2) != 0U) value |= cmd::MASK_CH2EN;
    if ((profile.enabledChannels & CHANNEL_3) != 0U) value |= cmd::MASK_CH3EN;
    value |= static_cast<uint16_t>(
        (static_cast<uint16_t>(profile.averaging) << cmd::BIT_AVG) & cmd::MASK_AVG);
    value |= static_cast<uint16_t>(
        (static_cast<uint16_t>(profile.vBusCt) << cmd::BIT_VBUSCT) & cmd::MASK_VBUSCT);
    value |= static_cast<uint16_t>(
        (static_cast<uint16_t>(profile.vShCt) << cmd::BIT_VSHCT) & cmd::MASK_VSHCT);
    value |= static_cast<uint16_t>(
        (static_cast<uint16_t>(profile.mode) << cmd::BIT_MODE) & cmd::MASK_MODE);
    return Status::Ok();
  }
  if (index >= 1U && index <= 6U) {
    const uint8_t channel = static_cast<uint8_t>((index - 1U) / 2U);
    const bool warning = ((index - 1U) & 1U) != 0U;
    reg = warning ? warnRegAddr(static_cast<Channel>(channel))
                  : critRegAddr(static_cast<Channel>(channel));
    return encodeShuntMicroVolts(
        warning ? profile.alerts.warningLimitMicroVolts[channel]
                : profile.alerts.criticalLimitMicroVolts[channel], value);
  }
  if (index == 7U) {
    reg = cmd::REG_SHUNT_SUM_LIMIT;
    return encodeShuntSumMicroVolts(profile.alerts.shuntSumLimitMicroVolts, value);
  }
  if (index == 8U) {
    reg = cmd::REG_MASK_ENABLE;
    if ((profile.alerts.summationChannels & CHANNEL_1) != 0U) value |= cmd::MASK_SCC1;
    if ((profile.alerts.summationChannels & CHANNEL_2) != 0U) value |= cmd::MASK_SCC2;
    if ((profile.alerts.summationChannels & CHANNEL_3) != 0U) value |= cmd::MASK_SCC3;
    if (profile.alerts.warningLatch) value |= cmd::MASK_WEN;
    if (profile.alerts.criticalLatch) value |= cmd::MASK_CEN;
    return Status::Ok();
  }
  if (index == 9U || index == 10U) {
    reg = index == 9U ? cmd::REG_PV_UPPER_LIMIT : cmd::REG_PV_LOWER_LIMIT;
    return encodeBusMilliVolts(static_cast<int32_t>(
        index == 9U ? profile.alerts.powerValidUpperMilliVolts
                    : profile.alerts.powerValidLowerMilliVolts), value);
  }
  return Status::Error(Err::INVALID_PARAM, "Invalid profile register index");
}

bool INA3221::_registerMatches(uint8_t reg, uint16_t actual, uint16_t desired) {
  if (reg == cmd::REG_MASK_ENABLE) {
    return (actual & kMaskEnableWritable) == (desired & kMaskEnableWritable);
  }
  if (reg == cmd::REG_CONFIG) {
    return (actual & static_cast<uint16_t>(~cmd::MASK_RST)) ==
           (desired & static_cast<uint16_t>(~cmd::MASK_RST));
  }
  return actual == desired;
}

void INA3221::_finishJob(JobTerminalState state, const Status& status,
                         HardwareEffect effect) {
  _jobState = state;
  _pendingJobResult = JobResult{};
  _pendingJobResult.kind = _jobKind;
  _pendingJobResult.state = state;
  _pendingJobResult.hardwareEffect = effect;
  _pendingJobResult.status = status;
  _pendingJobResult.requestId = _jobRequestId;
  _pendingJobResult.transfers = _jobTransfers;
  _pendingJobResult.profileGeneration = _profileGeneration;
  _hasPendingJobResult = true;
}

void INA3221::_finishJobFailure(const Status& status, bool writeStage,
                                uint8_t writeRegister) {
  const bool writeUncertain =
      writeStage && _writeMayHaveReachedDevice(status);
  if (writeUncertain && isWritableRegisterAddress(writeRegister)) {
    _markRegisterUnknown(writeRegister);
  }
  if (_jobAnyWriteConfirmed && _jobKind == JobKind::POWER_DOWN) {
    _markRegisterDirty(cmd::REG_CONFIG);
  }
  const HardwareEffect effect = writeUncertain
                                    ? HardwareEffect::INDETERMINATE
                                    : (_jobAnyWriteConfirmed
                                           ? HardwareEffect::PARTIAL
                                           : HardwareEffect::NONE);
  JobTerminalState state = JobTerminalState::FAILED;
  if (status.code == Err::DEADLINE_EXPIRED) {
    state = JobTerminalState::TIMED_OUT;
  } else if (writeUncertain) {
    state = JobTerminalState::INDETERMINATE;
  } else if (_jobAnyWriteConfirmed) {
    state = JobTerminalState::PARTIAL;
  }
  _finishJob(state, status, effect);
}

void INA3221::_finishJobSuccess() {
  const AppliedConfigState priorAlertState = _alertConfigState;
  if (_jobKind == JobKind::INITIALIZE || _jobKind == JobKind::APPLY_PROFILE ||
      _jobKind == JobKind::RECONCILE || _jobKind == JobKind::POWER_DOWN) {
    _profile = _pendingProfile;
    _syncLegacyConfigViewFromProfile();
    _measurementConfigState = AppliedConfigState::APPLIED;
    _alertConfigState = AppliedConfigState::APPLIED;
    _initialized = true;
    _driverState = DriverState::READY;
    _consecutiveFailures = 0;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
    _clearHardwareConfigDirty();
    if (_jobKind == JobKind::POWER_DOWN) {
      _alertConfigState = priorAlertState;
      if (_alertConfigState != AppliedConfigState::APPLIED) {
        _hardwareConfigDirty = true;
        _hardwareConfigDirtyStatus = Status::Error(
            Err::CONFIG_UNKNOWN, "Alert profile remains unverified");
      }
    }
  } else if (_jobKind == JobKind::TRIGGERED_SAMPLE ||
             _jobKind == JobKind::CONTINUOUS_SAMPLE) {
    _lastGoodSample = _sampleWork;
    _hasLastGoodSample = true;
  }
  _finishJob(JobTerminalState::SUCCEEDED, Status::Ok(),
             _jobAnyWriteConfirmed ? HardwareEffect::CONFIRMED
                                   : HardwareEffect::NONE);
  if (_jobKind == JobKind::TRIGGERED_SAMPLE ||
      _jobKind == JobKind::CONTINUOUS_SAMPLE) {
    _pendingJobResult.sampleValid = true;
    _pendingJobResult.sample = _lastGoodSample;
  }
  _pendingJobResult.profileGeneration = _profileGeneration;
}

Status INA3221::cancelJob() {
  if (_jobState != JobTerminalState::ACTIVE) {
    return Status::Error(Err::NO_ACTIVE_JOB, "No active owner job");
  }
  if (_jobAnyWriteConfirmed) {
    if (_jobKind == JobKind::POWER_DOWN)
      _measurementConfigState = AppliedConfigState::DIRTY;
  }
  _sampleWork = SampleBatch{};
  for (uint8_t i = 0; i < 3U; ++i) _sampleRaw[i] = ChannelRawMeasurement{};
  const HardwareEffect effect = _jobAnyWriteConfirmed
                                    ? HardwareEffect::PARTIAL
                                    : HardwareEffect::NONE;
  const Status cancelled = Status::Error(Err::CANCELLED, "Owner job cancelled");
  _finishJob(JobTerminalState::CANCELLED, cancelled, effect);
  return cancelled;
}

Status INA3221::takeJobResult(JobResult& out) {
  if (!_hasPendingJobResult) {
    return Status::Error(Err::NO_RESULT, "No terminal result pending");
  }
  out = _pendingJobResult;
  _hasPendingJobResult = false;
  _pendingJobResult = JobResult{};
  _resetOperationState();
  return Status::Ok();
}

Status INA3221::peekLastSample(SampleBatch& out) const {
  if (!_hasLastGoodSample) return Status::Error(Err::NO_RESULT, "No completed sample");
  out = _lastGoodSample;
  return Status::Ok();
}

Status INA3221::peekAlertEvents(AlertSnapshot& out) const {
  out = _retainedAlerts;
  return Status::Ok();
}

Status INA3221::takeAlertEvents(AlertSnapshot& out) {
  out = _retainedAlerts;
  _retainedAlerts.events = 0;
  _retainedAlerts.evidenceUncertain = false;
  return Status::Ok();
}

Status INA3221::getJobProgress(JobProgress& out) const {
  out = JobProgress{};
  out.kind = _jobKind;
  out.state = _jobState;
  out.requestId = _jobRequestId;
  out.totalTransfers = _jobTransfers;
  out.lastPollTransfers = _jobTransfersLastPoll;
  out.deadlineMs = _jobDeadlineMs;
  out.readyAtMs = _jobReadyAtMs;
  out.resultPending = _hasPendingJobResult;
  if (_hasPendingJobResult) {
    out.stage = JobStage::TERMINAL;
    return Status::Ok();
  }
  switch (_jobStage) {
    case OperationStage::READ_MANUFACTURER_ID:
    case OperationStage::READ_DIE_ID: out.stage = JobStage::READ_IDENTITY; break;
    case OperationStage::PROFILE_READ: out.stage = JobStage::READ_PROFILE; break;
    case OperationStage::PROFILE_WRITE: out.stage = JobStage::WRITE_PROFILE; break;
    case OperationStage::PROFILE_VERIFY: out.stage = JobStage::VERIFY_PROFILE; break;
    case OperationStage::SAMPLE_WRITE_CONFIG: out.stage = JobStage::TRIGGER_SAMPLE; break;
    case OperationStage::SAMPLE_WAIT_ORIGIN:
    case OperationStage::SAMPLE_WAIT: out.stage = JobStage::WAIT_CONVERSION; break;
    case OperationStage::SAMPLE_READ_MASK: out.stage = JobStage::READ_ALERTS; break;
    case OperationStage::SAMPLE_READ_CHANNELS: out.stage = JobStage::READ_CHANNELS; break;
    case OperationStage::POWER_READ_CONFIG: out.stage = JobStage::READ_POWER_STATE; break;
    case OperationStage::POWER_WRITE_CONFIG: out.stage = JobStage::WRITE_POWER_STATE; break;
    case OperationStage::POWER_VERIFY_CONFIG: out.stage = JobStage::VERIFY_POWER_STATE; break;
    case OperationStage::IDLE:
    default:
      out.stage = _hasPendingJobResult ? JobStage::TERMINAL : JobStage::IDLE;
      break;
  }
  return Status::Ok();
}

Status INA3221::_driveCompatibilityJob(uint32_t maximumTransfers) {
  const uint32_t maxPolls = maximumTransfers * 3U + 8U;
  for (uint32_t poll = 0; poll < maxPolls && !_hasPendingJobResult; ++poll) {
    PollContext context{};
    context.nowMs = _nowMs();
    context.transferTimeoutMs = _transport.defaultTransferTimeoutMs;
    context.maxTransfers = 1U;
    Status st = pollJob(context);
    if (!st.ok() && !st.inProgress() && !_hasPendingJobResult) return st;
  }
  if (!_hasPendingJobResult) {
    (void)cancelJob();
  }
  JobResult result{};
  Status take = takeJobResult(result);
  if (!take.ok()) return take;
  return result.status;
}

Status INA3221::_compatibilityDurationMs(JobKind kind,
                                         const DeviceProfile& profile,
                                         Mode mode, uint64_t& outMs) const {
  uint16_t transfers = 0;
  Status st = maximumJobTransfers(kind, profile, transfers);
  if (!st.ok()) return st;
  uint64_t duration = static_cast<uint64_t>(transfers) *
                      _transport.defaultTransferTimeoutMs;
  if (kind == JobKind::TRIGGERED_SAMPLE) {
    uint64_t cycleUs = 0;
    st = maximumCycleTimeUs(profile, mode, cycleUs);
    if (!st.ok()) return st;
    if (cycleUs > UINT64_MAX - CONVERSION_WAKE_MARGIN_US) {
      return Status::Error(Err::ARITHMETIC_OVERFLOW,
                           "Compatibility duration overflow");
    }
    cycleUs += CONVERSION_WAKE_MARGIN_US;
    const uint64_t waitMs = (cycleUs + 999U) / 1000U;
    if (duration > UINT64_MAX - waitMs) {
      return Status::Error(Err::ARITHMETIC_OVERFLOW,
                           "Compatibility duration overflow");
    }
    duration += waitMs;
  }
  if (duration > UINT64_MAX - COMPATIBILITY_SCHEDULING_MARGIN_MS) {
    return Status::Error(Err::ARITHMETIC_OVERFLOW,
                         "Compatibility duration overflow");
  }
  outMs = duration + COMPATIBILITY_SCHEDULING_MARGIN_MS;
  return Status::Ok();
}

Status INA3221::_prepareCompatibilityPoll(uint32_t nowMs,
                                          uint8_t maxTransfers,
                                          PollContext& out) {
  if (!_pollTimeInitialized) {
    _pollTimeInitialized = true;
    _pollLastNowMs = nowMs;
    _pollExtendedNowMs = nowMs;
  } else {
    _pollExtendedNowMs += static_cast<uint32_t>(nowMs - _pollLastNowMs);
    _pollLastNowMs = nowMs;
  }
  if (_jobDeadlineMs == UINT64_MAX) {
    if (_pollDeadlineDurationMs == 0U ||
        _pollExtendedNowMs > UINT64_MAX - _pollDeadlineDurationMs) {
      const Status st = Status::Error(Err::ARITHMETIC_OVERFLOW,
                                      "Compatibility deadline overflow");
      _finishJob(JobTerminalState::TIMED_OUT, st,
                 _jobAnyWriteConfirmed ? HardwareEffect::PARTIAL
                                       : HardwareEffect::NONE);
      return st;
    }
    _jobDeadlineMs = _pollExtendedNowMs + _pollDeadlineDurationMs;
  }
  out = PollContext{};
  out.nowMs = _pollExtendedNowMs;
  out.transferTimeoutMs = _transport.defaultTransferTimeoutMs;
  out.maxTransfers = maxTransfers;
  return Status::Ok();
}

Status INA3221::_pollProfileJob(const PollContext& context,
                                uint8_t& transfersLeft) {
  while (_jobState == JobTerminalState::ACTIVE) {
    if (_jobStage == OperationStage::READ_MANUFACTURER_ID) {
      uint16_t value = 0;
      Status st = _jobReadRegister(cmd::REG_MANUFACTURER_ID, value, context,
                                   transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      if (value != cmd::MANUFACTURER_ID_VALUE) {
        st = Status::Error(Err::MANUFACTURER_ID_MISMATCH,
                           "Manufacturer ID mismatch", value);
        _recordFailure(st);
        _finishJobFailure(st);
        return st;
      }
      _jobStage = OperationStage::READ_DIE_ID;
      continue;
    }
    if (_jobStage == OperationStage::READ_DIE_ID) {
      uint16_t value = 0;
      Status st = _jobReadRegister(cmd::REG_DIE_ID, value, context,
                                   transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      if (value != cmd::DIE_ID_VALUE) {
        st = Status::Error(Err::DIE_ID_MISMATCH, "Die ID mismatch", value);
        _recordFailure(st);
        _finishJobFailure(st);
        return st;
      }
      _jobStage = OperationStage::PROFILE_READ;
      continue;
    }
    if (_jobProfileIndex >= MANAGED_PROFILE_REGISTER_COUNT) {
      _finishJobSuccess();
      return Status::Ok();
    }
    if (_jobStage == OperationStage::PROFILE_READ) {
      Status st = _desiredRegister(_jobProfileIndex, _pendingProfile,
                                   _jobDesiredRegisterAddress,
                                   _jobDesiredRegisterValue);
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      uint16_t actual = 0;
      st = _jobReadRegister(_jobDesiredRegisterAddress, actual, context,
                            transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      if (_registerMatches(_jobDesiredRegisterAddress, actual,
                           _jobDesiredRegisterValue)) {
        ++_jobProfileIndex;
        continue;
      }
      _jobStage = OperationStage::PROFILE_WRITE;
      continue;
    }
    if (_jobStage == OperationStage::PROFILE_WRITE) {
      Status st = _jobWriteRegister(_jobDesiredRegisterAddress,
                                    _jobDesiredRegisterValue, context,
                                    transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _finishJobFailure(st, true, _jobDesiredRegisterAddress);
        return st;
      }
      _jobAnyWriteConfirmed = true;
      _markRegisterDirty(_jobDesiredRegisterAddress);
      _jobStage = OperationStage::PROFILE_VERIFY;
      continue;
    }
    if (_jobStage == OperationStage::PROFILE_VERIFY) {
      uint16_t actual = 0;
      Status st = _jobReadRegister(_jobDesiredRegisterAddress, actual, context,
                                   transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _markRegisterDirty(_jobDesiredRegisterAddress);
        _finishJobFailure(st);
        return st;
      }
      if (!_registerMatches(_jobDesiredRegisterAddress, actual,
                            _jobDesiredRegisterValue)) {
        st = Status::Error(Err::PROFILE_MISMATCH,
                           "Profile register verification mismatch",
                           _jobDesiredRegisterAddress);
        _markRegisterDirty(_jobDesiredRegisterAddress);
        _finishJob(JobTerminalState::PARTIAL, st, HardwareEffect::PARTIAL);
        _pendingJobResult.mismatchValid = true;
        _pendingJobResult.mismatchRegister = _jobDesiredRegisterAddress;
        _pendingJobResult.mismatchExpected = _jobDesiredRegisterValue;
        _pendingJobResult.mismatchActual = actual;
        _pendingJobResult.mismatchMask =
            _jobDesiredRegisterAddress == cmd::REG_CONFIG
                ? static_cast<uint16_t>(~cmd::MASK_RST)
                : (_jobDesiredRegisterAddress == cmd::REG_MASK_ENABLE
                       ? kMaskEnableWritable
                       : 0xFFFFU);
        return st;
      }
      ++_jobProfileIndex;
      _jobStage = OperationStage::PROFILE_READ;
      continue;
    }
    const Status bad = Status::Error(Err::INVALID_CONFIG,
                                     "Invalid profile job stage");
    _finishJobFailure(bad);
    return bad;
  }
  return Status::Error(Err::RESULT_PENDING, "Terminal result pending");
}

Status INA3221::_buildFixedReading(uint8_t index, Mode mode,
                                   FixedChannelReading& out) const {
  if (index >= 3U) return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  const ChannelRawMeasurement& raw = _sampleRaw[index];
  QuantityMask requested = 0;
  if (modeReadsShunt(mode)) {
    if (!raw.shuntValid) return Status::Error(Err::NO_RESULT, "Shunt raw value missing");
    requested |= static_cast<QuantityMask>(Quantity::SHUNT);
  }
  if (modeReadsBus(mode)) {
    if (!raw.busValid) return Status::Error(Err::NO_RESULT, "Bus raw value missing");
    requested |= static_cast<QuantityMask>(Quantity::BUS);
  }
  return convertRawChannel(static_cast<uint16_t>(raw.shuntRaw),
                           static_cast<uint16_t>(raw.busRaw), requested,
                           _profile.shunts[index], out);
}

Status INA3221::_pollSampleOperation(const PollContext& context,
                                     uint8_t& transfersLeft) {
  while (_jobState == JobTerminalState::ACTIVE) {
    if (_jobStage == OperationStage::SAMPLE_WRITE_CONFIG) {
      uint8_t configReg = 0;
      uint16_t configValue = 0;
      DeviceProfile sampleProfile = _profile;
      sampleProfile.mode = _jobSampleMode;
      Status st = _desiredRegister(0U, sampleProfile, configReg, configValue);
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      uint64_t cycleUs = 0;
      st = maximumCycleTimeUs(_profile, _jobSampleMode, cycleUs);
      if (!st.ok() || cycleUs > UINT64_MAX - CONVERSION_WAKE_MARGIN_US) {
        if (st.ok()) st = Status::Error(Err::ARITHMETIC_OVERFLOW,
                                        "Ready deadline overflow");
        _finishJobFailure(st);
        return st;
      }
      cycleUs += CONVERSION_WAKE_MARGIN_US;
      const uint64_t waitMs = (cycleUs + 999U) / 1000U;

      // Admission is bus-silent unless the success path's maximum conversion
      // wait and callback bounds can fit the effective owner deadline. The
      // caller still owns poll cadence, so it must schedule subsequent polls.
      const uint64_t deadline = _effectiveDeadline(context);
      if (deadline != 0U) {
        uint16_t successTransfers = 0;
        st = maximumJobTransfers(JobKind::TRIGGERED_SAMPLE, _profile,
                                 successTransfers);
        const uint64_t transferBoundMs =
            static_cast<uint64_t>(successTransfers) *
            _transport.defaultTransferTimeoutMs;
        const bool overflow =
            !st.ok() || waitMs > UINT64_MAX - transferBoundMs - 1U ||
            context.nowMs > UINT64_MAX - waitMs - transferBoundMs - 1U;
        const uint64_t completionBound =
            overflow ? UINT64_MAX
                     : context.nowMs + 1U + waitMs + transferBoundMs;
        if (overflow || completionBound > deadline) {
          st = Status::Error(Err::DEADLINE_EXPIRED,
                             "Triggered profile cannot fit owner deadline");
          _finishJob(JobTerminalState::TIMED_OUT, st, HardwareEffect::NONE);
          return st;
        }
      }
      st = _jobWriteRegister(configReg, configValue, context, transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _finishJobFailure(st, true, cmd::REG_CONFIG);
        return st;
      }
      _jobAnyWriteConfirmed = true;
      _jobWaitDurationMs = waitMs;
      _jobWaitOriginAfterMs = context.nowMs;
      _jobReadyAtMs = 0;
      _jobStage = OperationStage::SAMPLE_WAIT_ORIGIN;
      return Status{Err::IN_PROGRESS, 0, "Trigger accepted; fresh time required"};
    }
    if (_jobStage == OperationStage::SAMPLE_WAIT_ORIGIN) {
      if (context.nowMs <= _jobWaitOriginAfterMs) {
        return Status{Err::IN_PROGRESS, 0, "Fresh later time required"};
      }
      if (context.nowMs > UINT64_MAX - _jobWaitDurationMs) {
        const Status st = Status::Error(Err::ARITHMETIC_OVERFLOW,
                                        "Ready deadline overflow");
        _finishJobFailure(st);
        return st;
      }
      _jobConversionStartMs = context.nowMs;
      _jobReadyAtMs = context.nowMs + _jobWaitDurationMs;
      const uint64_t deadline = _effectiveDeadline(context);
      if (deadline != 0U && _jobReadyAtMs >= deadline) {
        if (_sampleWork.alertSnapshotValid) {
          // CVRF has already been observed low after the maximum conversion
          // time.  Do not start another transfer that cannot complete before
          // the deadline; remain bus-silent until the owner deadline instead.
          _jobReadyAtMs = deadline;
          _jobStage = OperationStage::SAMPLE_WAIT;
          return Status{Err::IN_PROGRESS, 0, "Waiting for owner deadline"};
        }
        const Status st = Status::Error(Err::DEADLINE_EXPIRED,
                                        "Conversion wait exceeds owner deadline");
        _finishJob(JobTerminalState::TIMED_OUT, st, HardwareEffect::PARTIAL);
        return st;
      }
      _jobStage = OperationStage::SAMPLE_WAIT;
      return Status{Err::IN_PROGRESS, 0, "Waiting for maximum conversion time"};
    }
    if (_jobStage == OperationStage::SAMPLE_WAIT) {
      if (context.nowMs < _jobReadyAtMs) {
        return Status{Err::IN_PROGRESS, 0, "Waiting for maximum conversion time"};
      }
      _jobStage = OperationStage::SAMPLE_READ_MASK;
      continue;
    }
    if (_jobStage == OperationStage::SAMPLE_READ_MASK) {
      uint16_t raw = 0;
      _lastCallbackInvoked = false;
      const uint32_t timeout = _clampedTransferTimeout(context);
      if (transfersLeft == 0U)
        return Status{Err::IN_PROGRESS, 0, "Transfer budget exhausted"};
      if (timeout == 0U)
        return Status::Error(Err::DEADLINE_EXPIRED, "Deadline expired");
      --transfersLeft;
      if (_jobTransfers < UINT16_MAX) ++_jobTransfers;
      Status st = _readMaskEnableWithTimeout(raw, &_sampleWork.alerts,
                                             timeout, true);
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      _sampleWork.alertSnapshotValid = true;
      if (_jobKind == JobKind::TRIGGERED_SAMPLE &&
          (raw & cmd::MASK_CVRF) == 0U) {
        _jobWaitDurationMs = CVRF_FAULT_RECHECK_MS;
        _jobWaitOriginAfterMs = context.nowMs;
        _jobReadyAtMs = 0;
        _jobStage = OperationStage::SAMPLE_WAIT_ORIGIN;
        return Status{Err::IN_PROGRESS, 0, "CVRF not ready"};
      }
      _jobStage = OperationStage::SAMPLE_READ_CHANNELS;
      continue;
    }
    if (_jobStage == OperationStage::SAMPLE_READ_CHANNELS) {
      while (_jobChannelIndex < 3U &&
             !_sampleRaw[_jobChannelIndex].channelEnabled) {
        ++_jobChannelIndex;
      }
      if (_jobChannelIndex >= 3U) {
        QuantityMask required = 0;
        if (modeReadsShunt(_jobSampleMode)) {
          required |= static_cast<QuantityMask>(Quantity::SHUNT);
          required |= static_cast<QuantityMask>(Quantity::CURRENT);
        }
        if (modeReadsBus(_jobSampleMode)) required |= static_cast<QuantityMask>(Quantity::BUS);
        if (modeReadsBus(_jobSampleMode) && modeReadsShunt(_jobSampleMode))
          required |= static_cast<QuantityMask>(Quantity::POWER);
        for (uint8_t i = 0; i < 3U; ++i) {
          if (!_sampleRaw[i].channelEnabled) continue;
          Status st = _buildFixedReading(i, _jobSampleMode,
                                         _sampleWork.channels[i]);
          if (!st.ok()) {
            _finishJobFailure(st);
            return st;
          }
          if ((_sampleWork.channels[i].validQuantities & required) == required) {
            _sampleWork.validChannels |= static_cast<ChannelMask>(1U << i);
          }
        }
        _sampleWork.captureUptimeMs = context.nowMs;
        _finishJobSuccess();
        return Status::Ok();
      }
      const Channel ch = static_cast<Channel>(_jobChannelIndex);
      uint8_t reg = 0;
      bool readingBus = false;
      if (modeReadsShunt(_jobSampleMode) &&
          !_sampleRaw[_jobChannelIndex].shuntValid) {
        reg = shuntRegAddr(ch);
      } else if (modeReadsBus(_jobSampleMode) &&
                 !_sampleRaw[_jobChannelIndex].busValid) {
        reg = busRegAddr(ch);
        readingBus = true;
      } else {
        ++_jobChannelIndex;
        continue;
      }
      uint16_t value = 0;
      Status st = _jobReadRegister(reg, value, context, transfersLeft);
      if (st.inProgress()) return st;
      if (!st.ok()) {
        _finishJobFailure(st);
        return st;
      }
      if (readingBus) {
        _sampleRaw[_jobChannelIndex].busRaw = static_cast<int16_t>(value);
        _sampleRaw[_jobChannelIndex].busValid = true;
      } else {
        _sampleRaw[_jobChannelIndex].shuntRaw = static_cast<int16_t>(value);
        _sampleRaw[_jobChannelIndex].shuntValid = true;
      }
      continue;
    }
    const Status bad = Status::Error(Err::INVALID_CONFIG,
                                     "Invalid sample job stage");
    _finishJobFailure(bad);
    return bad;
  }
  return Status::Error(Err::RESULT_PENDING, "Terminal result pending");
}

Status INA3221::_pollPowerDownOperation(const PollContext& context,
                                        uint8_t& transfersLeft) {
  uint8_t reg = 0;
  uint16_t desired = 0;
  Status st = _desiredRegister(0U, _pendingProfile, reg, desired);
  if (!st.ok()) {
    _finishJobFailure(st);
    return st;
  }
  if (_jobStage == OperationStage::POWER_READ_CONFIG) {
    uint16_t actual = 0;
    st = _jobReadRegister(reg, actual, context, transfersLeft);
    if (st.inProgress()) return st;
    if (!st.ok()) {
      _finishJobFailure(st);
      return st;
    }
    if (_registerMatches(reg, actual, desired)) {
      _finishJobSuccess();
      return Status::Ok();
    }
    _jobStage = OperationStage::POWER_WRITE_CONFIG;
  }
  if (_jobStage == OperationStage::POWER_WRITE_CONFIG) {
    st = _jobWriteRegister(reg, desired, context, transfersLeft);
    if (st.inProgress()) return st;
    if (!st.ok()) {
      _finishJobFailure(st, true, reg);
      return st;
    }
    _jobAnyWriteConfirmed = true;
    _markRegisterDirty(reg);
    _jobStage = OperationStage::POWER_VERIFY_CONFIG;
  }
  if (_jobStage == OperationStage::POWER_VERIFY_CONFIG) {
    uint16_t actual = 0;
    st = _jobReadRegister(reg, actual, context, transfersLeft);
    if (st.inProgress()) return st;
    if (!st.ok()) {
      _finishJobFailure(st);
      return st;
    }
    if (!_registerMatches(reg, actual, desired)) {
      st = Status::Error(Err::PROFILE_MISMATCH,
                         "Power-down verification mismatch", reg);
      _finishJob(JobTerminalState::PARTIAL, st, HardwareEffect::PARTIAL);
      _pendingJobResult.mismatchValid = true;
      _pendingJobResult.mismatchRegister = reg;
      _pendingJobResult.mismatchExpected = desired;
      _pendingJobResult.mismatchActual = actual;
      _pendingJobResult.mismatchMask = static_cast<uint16_t>(~cmd::MASK_RST);
      return st;
    }
    _finishJobSuccess();
    return Status::Ok();
  }
  return Status{Err::IN_PROGRESS, 0, "Power-down in progress"};
}

Status INA3221::pollJob(const PollContext& context) {
  _jobTransfersLastPoll = 0;
  if (_hasPendingJobResult) {
    return Status::Error(Err::RESULT_PENDING, "Take terminal result first");
  }
  if (_jobState != JobTerminalState::ACTIVE) {
    return Status::Error(Err::NO_ACTIVE_JOB, "No active owner job");
  }
  if (_deadlineExpired(context)) {
    const Status timeout = Status::Error(Err::DEADLINE_EXPIRED,
                                         "Owner deadline expired");
    _finishJobFailure(timeout);
    return timeout;
  }
  uint8_t transfersLeft = context.maxTransfers;
  const uint16_t before = _jobTransfers;
  Status st;
  switch (_jobKind) {
    case JobKind::INITIALIZE:
    case JobKind::APPLY_PROFILE:
    case JobKind::RECONCILE:
      st = _pollProfileJob(context, transfersLeft);
      break;
    case JobKind::TRIGGERED_SAMPLE:
    case JobKind::CONTINUOUS_SAMPLE:
      st = _pollSampleOperation(context, transfersLeft);
      break;
    case JobKind::POWER_DOWN:
      st = _pollPowerDownOperation(context, transfersLeft);
      break;
    case JobKind::NONE:
    default:
      st = Status::Error(Err::NO_ACTIVE_JOB, "No active owner job");
      break;
  }
  const uint16_t used = static_cast<uint16_t>(_jobTransfers - before);
  _jobTransfersLastPoll = used > UINT8_MAX ? UINT8_MAX
                                           : static_cast<uint8_t>(used);
  if (st.code == Err::DEADLINE_EXPIRED &&
      _jobState == JobTerminalState::ACTIVE) {
    _finishJobFailure(st);
  }
  return st;
}

// ============================================================================
// Lifecycle
// ============================================================================

Status INA3221::begin(const Config& config) {
  TransportConfig transport{};
  DeviceProfile profile{};
  Status st = _legacyToContracts(config, transport, profile);
  if (!st.ok()) return st;
  st = bind(transport, profile);
  if (!st.ok()) return st;
  st = startInitialize(1U, UINT64_MAX);
  if (!st.inProgress()) {
    unbind();
    return st;
  }
  st = _driveCompatibilityJob(35U);
  if (!st.ok()) unbind();
  return st;
}

void INA3221::tick(uint32_t nowMs) {
  (void)tickStatus(nowMs);
}

Status INA3221::tickStatus(uint32_t nowMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if (_jobState == JobTerminalState::ACTIVE) {
    return Status::Error(Err::JOB_BUSY, "Owner job active");
  }
  if (_conversionStarted && !_conversionReady) {
    bool ready = false;
    return _readConversionReadyAt(nowMs, ready);
  }
  return Status::Ok();
}

Status INA3221::powerDown() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  Status st = startPowerDown(UINT32_MAX, UINT64_MAX);
  if (!st.inProgress()) return st;
  return _driveCompatibilityJob(3U);
}

void INA3221::end() {
  unbind();
}

// ============================================================================
// Diagnostics
// ============================================================================

Status INA3221::probe() {
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!_bound) return Status::Error(Err::NOT_INITIALIZED, "Driver not bound");
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

  Status st = startInitialize(UINT32_MAX - 1U, UINT64_MAX);
  if (!st.inProgress()) return st;
  return _driveCompatibilityJob(35U);
}

Status INA3221::getSettings(SettingsSnapshot& out) const {
  out.initialized = _initialized;
  out.state = _driverState;
  out.i2cAddress = _legacyConfigView.i2cAddress;
  out.i2cTimeoutMs = _legacyConfigView.i2cTimeoutMs;
  out.offlineThreshold = _legacyConfigView.offlineThreshold;
  out.hasNowMsHook = _legacyConfigView.nowMs != nullptr;
  out.hasCooperativeYieldHook =
      _legacyConfigView.cooperativeYield != nullptr;
  out.ch1Enable = _legacyConfigView.ch1Enable;
  out.ch2Enable = _legacyConfigView.ch2Enable;
  out.ch3Enable = _legacyConfigView.ch3Enable;
  out.averaging = _legacyConfigView.averaging;
  out.vBusCt = _legacyConfigView.vBusCt;
  out.vShCt = _legacyConfigView.vShCt;
  out.mode = _legacyConfigView.mode;
  out.shuntResistance[0] = _legacyConfigView.shuntResistance[0];
  out.shuntResistance[1] = _legacyConfigView.shuntResistance[1];
  out.shuntResistance[2] = _legacyConfigView.shuntResistance[2];
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
  Status valid = validateMeasurementRead(_profile, _initialized, ch, true, false);
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
  Status valid = validateMeasurementRead(_profile, _initialized, ch, false, true);
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
  Status valid = validateMeasurementRead(_profile, _initialized, ch, true, false);
  if (!valid.ok()) {
    return valid;
  }
  float shuntMv = 0.0f;
  Status st = readShuntVoltage(ch, shuntMv);
  if (!st.ok()) {
    return st;
  }
  const uint8_t index = static_cast<uint8_t>(ch);
  const float rShunt = static_cast<float>(
      _profile.shunts[index].resistanceMicroOhms) / 1000000.0f;
  // I = Vshunt / Rshunt, Vshunt in mV, Rshunt in ohms -> I in mA
  mA = applyCurrentDirection(shuntMv / rShunt, _profile.shunts[index]);
  return Status::Ok();
}

Status INA3221::readPower(Channel ch, float& mW) {
  Status valid = validateMeasurementRead(_profile, _initialized, ch, true, true);
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
  Status valid = validateMeasurementRead(_profile, _initialized, ch, true, true);
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

  const uint8_t index = static_cast<uint8_t>(ch);
  const float rShunt = static_cast<float>(
      _profile.shunts[index].resistanceMicroOhms) / 1000000.0f;
  out.current_mA = applyCurrentDirection(out.shuntVoltage_mV / rShunt,
                                         _profile.shunts[index]);
  out.power_mW = out.busVoltage_V * out.current_mA;

  return Status::Ok();
}

Status INA3221::readShuntSumRaw(int16_t& raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!modeReadsShunt(_profile.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Mode does not measure shunt");
  }
  if (_profile.alerts.summationChannels == 0U) {
    return Status::Error(Err::INVALID_CONFIG, "No summation channel selected");
  }
  if (_alertConfigState != AppliedConfigState::APPLIED) {
    return Status::Error(Err::CONFIG_UNKNOWN, "Alert profile not verified");
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (_isContinuousMode()) {
    return Status::Error(Err::CONVERSION_BUSY, "Continuous mode active");
  }
  if (!_isTriggeredMode()) {
    return Status::Error(Err::INVALID_CONFIG, "Triggered mode required");
  }
  if (_enabledChannelCount() == 0) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  if (_conversionStarted) {
    return Status::Error(Err::CONVERSION_BUSY, "Conversion already in progress");
  }

  // Writing to config register triggers single-shot conversion
  Status st = _applyConfigVerified(_profile);
  if (!st.ok()) return st;
  _syncLegacyConfigViewFromProfile();
  _handleConfigWriteSideEffects(_profile);
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status INA3221::startConversion(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isTriggeredMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Must be a triggered mode");
  }
  if (_enabledChannelCount() == 0) {
    return Status::Error(Err::INVALID_CONFIG, "At least one channel must be enabled");
  }
  if (_conversionStarted) {
    return Status::Error(Err::CONVERSION_BUSY, "Conversion already in progress");
  }

  DeviceProfile candidate = _profile;
  candidate.mode = mode;
  Status st = _validateProfile(candidate);
  if (!st.ok()) return st;
  st = _applyConfigVerified(candidate);
  if (!st.ok()) return st;
  _profile = candidate;
  _syncLegacyConfigViewFromProfile();
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  _handleConfigWriteSideEffects(_profile);
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status INA3221::cancelConversion() {
  if (!_conversionStarted) {
    return Status::Error(Err::NO_ACTIVE_JOB, "No legacy conversion active");
  }
  _clearLegacyConversionState();
  return Status::Ok();
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
  (void)pollConversionReady;
  Mode mode = _isTriggeredMode() ? _profile.mode : Mode::SHUNT_BUS_TRIG;
  return startSingleShot(mode, true);
}

Status INA3221::startSingleShot(Mode mode, bool pollConversionReady) {
  (void)pollConversionReady;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isTriggeredMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Must be a triggered mode");
  }
  uint64_t durationMs = 0;
  Status st = _compatibilityDurationMs(JobKind::TRIGGERED_SAMPLE, _profile,
                                       mode, durationMs);
  if (!st.ok()) return st;
  st = startTriggeredSample(mode, UINT32_MAX - 2U, UINT64_MAX);
  if (st.inProgress()) {
    _clearPollJob();
    _pollJobKind = PollJobKind::SINGLE_SHOT;
    _pollJobStage = PollJobStage::WRITE_CONFIG;
    _pollSampleMode = mode;
    _pollConversionReady = true;
    _pollDeadlineDurationMs = durationMs;
  }
  return st;
}

Status INA3221::pollSingleShot(uint32_t nowMs, uint8_t maxInstructions) {
  if (_pollJobKind != PollJobKind::SINGLE_SHOT ||
      _jobKind != JobKind::TRIGGERED_SAMPLE)
    return Status::Error(Err::JOB_KIND_MISMATCH, "Single-shot job is not active");
  PollContext context{};
  Status prepared = _prepareCompatibilityPoll(nowMs, maxInstructions, context);
  if (!prepared.ok()) return prepared;
  Status st = pollJob(context);
  JobProgress progress{};
  (void)getJobProgress(progress);
  _pollInstructionsLast = progress.lastPollTransfers;
  _pollInstructionsTotal = progress.totalTransfers;
  _pollNextChannel = _jobChannelIndex;
  _pollConversionStartMs = static_cast<uint32_t>(_jobConversionStartMs);
  if (progress.stage == JobStage::WAIT_CONVERSION) _pollJobStage = PollJobStage::WAIT_CONVERSION;
  else if (progress.stage == JobStage::READ_ALERTS) _pollJobStage = PollJobStage::READ_READY;
  else if (progress.stage == JobStage::READ_CHANNELS) _pollJobStage = PollJobStage::READ_CHANNELS;
  if (_hasPendingJobResult) {
    ChannelRawMeasurement completedRaw[3] = {
        _sampleRaw[0], _sampleRaw[1], _sampleRaw[2]};
    JobResult result{};
    (void)takeJobResult(result);
    if (result.sampleValid) {
      for (uint8_t i = 0; i < 3U; ++i) _pollChannels[i] = completedRaw[i];
      _pollObservedReady = result.sample.alerts.conversionReady;
      _pollJobStage = PollJobStage::COMPLETE;
    }
    return result.status;
  }
  return st;
}

Status INA3221::startContinuousRead(bool pollConversionReady) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!_isContinuousMode()) {
    return Status::Error(Err::INVALID_CONFIG, "Continuous mode required");
  }
  uint64_t durationMs = 0;
  Status st = _compatibilityDurationMs(JobKind::CONTINUOUS_SAMPLE, _profile,
                                       _profile.mode, durationMs);
  if (!st.ok()) return st;
  st = startContinuousSample(UINT32_MAX - 3U, UINT64_MAX,
                             pollConversionReady);
  if (st.inProgress()) {
    _clearPollJob();
    _pollJobKind = PollJobKind::CONTINUOUS_READ;
    _pollJobStage = pollConversionReady ? PollJobStage::READ_READY
                                        : PollJobStage::READ_CHANNELS;
    _pollSampleMode = _profile.mode;
    _pollConversionReady = pollConversionReady;
    _pollDeadlineDurationMs = durationMs;
  }
  return st;
}

Status INA3221::pollContinuousRead(uint32_t nowMs, uint8_t maxInstructions) {
  if (_pollJobKind != PollJobKind::CONTINUOUS_READ ||
      _jobKind != JobKind::CONTINUOUS_SAMPLE)
    return Status::Error(Err::JOB_KIND_MISMATCH, "Continuous job is not active");
  PollContext context{};
  Status prepared = _prepareCompatibilityPoll(nowMs, maxInstructions, context);
  if (!prepared.ok()) return prepared;
  Status st = pollJob(context);
  JobProgress progress{};
  (void)getJobProgress(progress);
  _pollInstructionsLast = progress.lastPollTransfers;
  _pollInstructionsTotal = progress.totalTransfers;
  _pollNextChannel = _jobChannelIndex;
  _pollJobStage = progress.stage == JobStage::READ_ALERTS
                      ? PollJobStage::READ_READY
                      : PollJobStage::READ_CHANNELS;
  if (_hasPendingJobResult) {
    ChannelRawMeasurement completedRaw[3] = {
        _sampleRaw[0], _sampleRaw[1], _sampleRaw[2]};
    JobResult result{};
    (void)takeJobResult(result);
    if (result.sampleValid) {
      for (uint8_t i = 0; i < 3U; ++i) _pollChannels[i] = completedRaw[i];
      _pollJobStage = PollJobStage::COMPLETE;
    }
    return result.status;
  }
  return st;
}

Status INA3221::pollJob(uint32_t nowMs, uint8_t maxInstructions) {
  switch (_pollJobKind) {
    case PollJobKind::SINGLE_SHOT: return pollSingleShot(nowMs, maxInstructions);
    case PollJobKind::CONTINUOUS_READ: return pollContinuousRead(nowMs, maxInstructions);
    case PollJobKind::APPLY_MASK_ENABLE: return pollApplyMaskEnable(nowMs, maxInstructions);
    case PollJobKind::NONE:
    default: return Status::Error(Err::NO_ACTIVE_JOB, "No compatibility job active");
  }
}

Status INA3221::readChannelRawStep(Channel ch) {
  (void)ch;
  return Status::Error(Err::JOB_BUSY,
                       "Manual channel stepping is unavailable during owner jobs");
}

Status INA3221::getPollJobSnapshot(PollJobSnapshot& out) const {
  out.kind = _pollJobKind;
  out.stage = _pollJobStage;
  out.active = _jobState == JobTerminalState::ACTIVE;
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
  if (timeoutMs == AUTO_BLOCKING_TIMEOUT_MS) {
    const JobKind kind = _isContinuousMode()
                             ? JobKind::CONTINUOUS_SAMPLE
                             : JobKind::TRIGGERED_SAMPLE;
    const Mode sampleMode = _isTriggeredMode() ? _profile.mode
                                                : Mode::SHUNT_BUS_TRIG;
    uint64_t durationMs = 0;
    Status durationStatus =
        _compatibilityDurationMs(kind, _profile, sampleMode, durationMs);
    if (!durationStatus.ok()) return durationStatus;
    if (durationMs == 0U || durationMs > static_cast<uint64_t>(INT32_MAX)) {
      return Status::Error(Err::ARITHMETIC_OVERFLOW,
                           "Derived blocking timeout out of range");
    }
    timeoutMs = static_cast<uint32_t>(durationMs);
  }
  if (timeoutMs > static_cast<uint32_t>(INT32_MAX)) {
    return Status::Error(Err::INVALID_PARAM, "Timeout too large");
  }

  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (_transport.nowMs == nullptr && _isTriggeredMode()) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Blocking triggered sample requires monotonic time hook");
  }
  if (timeoutMs == 0U) {
    return Status::Error(Err::INVALID_PARAM, "Timeout must be non-zero");
  }
  const uint32_t startMs = _nowMs();
  const uint64_t deadlineMs = static_cast<uint64_t>(startMs) + timeoutMs;
  Status st = _isContinuousMode()
                  ? startContinuousSample(UINT32_MAX - 5U, deadlineMs, false)
                  : startTriggeredSample(_isTriggeredMode() ? _profile.mode
                                                            : Mode::SHUNT_BUS_TRIG,
                                           UINT32_MAX - 5U, deadlineMs);
  if (!st.inProgress()) return st;
  uint32_t lastElapsed = 0;
  uint32_t stalledSpins = 0;
  while (!_hasPendingJobResult) {
    const uint32_t elapsed = static_cast<uint32_t>(_nowMs() - startMs);
    PollContext context{};
    context.nowMs = static_cast<uint64_t>(startMs) + elapsed;
    context.deadlineMs = deadlineMs;
    context.transferTimeoutMs = _transport.defaultTransferTimeoutMs;
    context.maxTransfers = 1U;
    const uint16_t transfersBefore = _jobTransfers;
    st = pollJob(context);
    if (!st.ok() && !st.inProgress() && !_hasPendingJobResult) return st;
    if (elapsed != lastElapsed || _jobTransfers != transfersBefore) {
      lastElapsed = elapsed;
      stalledSpins = 0;
    } else if (++stalledSpins > STALLED_CLOCK_SPIN_LIMIT) {
      break;
    }
    if (!_hasPendingJobResult) _cooperativeYield();
  }
  if (!_hasPendingJobResult) (void)cancelJob();
  JobResult result{};
  Status take = takeJobResult(result);
  if (!take.ok()) return take;
  if (!result.status.ok()) return result.status;
  auto copyChannel = [&result](uint8_t index, ChannelMeasurement* output) {
    if (output == nullptr) return;
    const FixedChannelReading& input = result.sample.channels[index];
    output->shuntVoltage_mV = static_cast<float>(input.shuntMicroVolts) / 1000.0f;
    output->busVoltage_V = static_cast<float>(input.busMilliVolts) / 1000.0f;
    output->current_mA = static_cast<float>(input.currentMilliAmps);
    output->power_mW = static_cast<float>(input.powerMilliWatts);
  };
  copyChannel(0U, ch1);
  copyChannel(1U, ch2);
  copyChannel(2U, ch3);
  return Status::Ok();
}

// ============================================================================
// Configuration
// ============================================================================

Status INA3221::setMode(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isValidMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mode");
  }
  DeviceProfile candidate = _profile;
  candidate.mode = mode;
  Status st = _validateProfile(candidate);
  if (!st.ok()) return st;
  st = _applyConfigVerified(candidate);
  if (!st.ok()) return st;
  _profile = candidate;
  _syncLegacyConfigViewFromProfile();
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  _handleConfigWriteSideEffects(_profile);
  if (_isTriggeredMode()) {
    return Status{Err::IN_PROGRESS, 0, "Conversion started"};
  }
  return Status::Ok();
}

Status INA3221::setAveraging(Averaging avg) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isValidAveraging(avg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid averaging");
  }
  DeviceProfile candidate = _profile;
  candidate.averaging = avg;
  Status st = _applyConfigVerified(candidate);
  if (!st.ok()) return st;
  _profile = candidate;
  _syncLegacyConfigViewFromProfile();
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  _handleConfigWriteSideEffects(_profile);
  return Status::Ok();
}

Status INA3221::setVBusConvTime(ConvTime ct) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  DeviceProfile candidate = _profile;
  candidate.vBusCt = ct;
  Status st = _applyConfigVerified(candidate);
  if (!st.ok()) return st;
  _profile = candidate;
  _syncLegacyConfigViewFromProfile();
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  _handleConfigWriteSideEffects(_profile);
  return Status::Ok();
}

Status INA3221::setVShuntConvTime(ConvTime ct) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isValidConvTime(ct)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  DeviceProfile candidate = _profile;
  candidate.vShCt = ct;
  Status st = _applyConfigVerified(candidate);
  if (!st.ok()) return st;
  _profile = candidate;
  _syncLegacyConfigViewFromProfile();
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  _handleConfigWriteSideEffects(_profile);
  return Status::Ok();
}

Status INA3221::setChannelEnable(Channel ch, bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  DeviceProfile nextProfile = _profile;
  const ChannelMask bit =
      static_cast<ChannelMask>(1U << static_cast<uint8_t>(ch));
  if (enable) {
    nextProfile.enabledChannels =
        static_cast<ChannelMask>(nextProfile.enabledChannels | bit);
  } else {
    nextProfile.enabledChannels = static_cast<ChannelMask>(
        nextProfile.enabledChannels & static_cast<ChannelMask>(~bit));
  }
  Status st = _validateProfile(nextProfile);
  if (!st.ok()) return st;
  st = _applyConfigVerified(nextProfile);
  if (!st.ok()) return st;
  _profile = nextProfile;
  _syncLegacyConfigViewFromProfile();
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  _handleConfigWriteSideEffects(_profile);
  return Status::Ok();
}

bool INA3221::getChannelEnable(Channel ch) const {
  switch (ch) {
    case Channel::CH1: return _legacyConfigView.ch1Enable;
    case Channel::CH2: return _legacyConfigView.ch2Enable;
    case Channel::CH3: return _legacyConfigView.ch3Enable;
    default: return false;
  }
}

Status INA3221::setShuntResistance(Channel ch, float ohms) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isValidChannel(ch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel");
  }
  if (!isPositiveFinite(ohms)) {
    return Status::Error(Err::INVALID_PARAM, "Shunt resistance must be finite and > 0");
  }
  const double microOhms = static_cast<double>(ohms) * 1000000.0;
  if (microOhms < 1.0 || microOhms > UINT32_MAX) {
    return Status::Error(Err::OUT_OF_RANGE, "Shunt resistance out of range");
  }
  const uint8_t index = static_cast<uint8_t>(ch);
  _profile.shunts[index].resistanceMicroOhms =
      static_cast<uint32_t>(microOhms + 0.5);
  _legacyConfigView.shuntResistance[index] =
      static_cast<float>(_profile.shunts[index].resistanceMicroOhms) /
      1000000.0f;
  if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  return Status::Ok();
}

float INA3221::getShuntResistance(Channel ch) const {
  if (!isValidChannel(ch)) {
    return 0.0f;
  }
  return _legacyConfigView.shuntResistance[static_cast<uint8_t>(ch)];
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;

  if ((config & cmd::MASK_RST) != 0U) {
    Status st = _writeRegister16Tracked(cmd::REG_CONFIG, config);
    if (!st.ok()) {
      if (_writeMayHaveReachedDevice(st)) _handleResetWriteEffect(false);
      return st;
    }
    _handleResetWriteEffect(true);
    return Status::Ok();
  }

  DeviceProfile activeProfile = _profile;
  activeProfile.enabledChannels = static_cast<ChannelMask>(
      ((config & cmd::MASK_CH1EN) != 0U ? CHANNEL_1 : 0U) |
      ((config & cmd::MASK_CH2EN) != 0U ? CHANNEL_2 : 0U) |
      ((config & cmd::MASK_CH3EN) != 0U ? CHANNEL_3 : 0U));
  activeProfile.averaging = static_cast<Averaging>(
      (config & cmd::MASK_AVG) >> cmd::BIT_AVG);
  activeProfile.vBusCt = static_cast<ConvTime>(
      (config & cmd::MASK_VBUSCT) >> cmd::BIT_VBUSCT);
  activeProfile.vShCt = static_cast<ConvTime>(
      (config & cmd::MASK_VSHCT) >> cmd::BIT_VSHCT);
  activeProfile.mode = static_cast<Mode>(
      (config & cmd::MASK_MODE) >> cmd::BIT_MODE);
  if (!profileHasRequiredChannels(activeProfile)) {
    return Status::Error(Err::INVALID_PARAM, "At least one channel must be enabled");
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, config);
  if (!st.ok()) {
    if (_writeMayHaveReachedDevice(st)) {
      _clearLegacyConversionState();
      _clearPollJob();
      _markRegisterUnknown(cmd::REG_CONFIG);
    }
    return st;
  }

  _legacyConfigView.ch1Enable =
      (activeProfile.enabledChannels & CHANNEL_1) != 0U;
  _legacyConfigView.ch2Enable =
      (activeProfile.enabledChannels & CHANNEL_2) != 0U;
  _legacyConfigView.ch3Enable =
      (activeProfile.enabledChannels & CHANNEL_3) != 0U;
  _legacyConfigView.averaging = activeProfile.averaging;
  _legacyConfigView.vBusCt = activeProfile.vBusCt;
  _legacyConfigView.vShCt = activeProfile.vShCt;
  _legacyConfigView.mode = activeProfile.mode;
  _handleConfigWriteSideEffects(activeProfile);
  _markRegisterUnknown(cmd::REG_CONFIG);
  return Status::Ok();
}

Status INA3221::softReset() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, cmd::MASK_RST);
  if (!st.ok()) {
    if (_writeMayHaveReachedDevice(st)) _handleResetWriteEffect(false);
    return st;
  }
  _handleResetWriteEffect(true);
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
  const uint16_t encoded = static_cast<uint16_t>(raw) & kShuntLimitWritable;
  DeviceProfile candidate = _profile;
  (void)decodeShuntMicroVolts(
      encoded,
      candidate.alerts.criticalLimitMicroVolts[static_cast<uint8_t>(ch)]);
  Status st = _writeManagedRegisterVerified(critRegAddr(ch), encoded);
  if (st.ok()) {
    _profile = candidate;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
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
  const uint16_t encoded = static_cast<uint16_t>(raw) & kShuntLimitWritable;
  DeviceProfile candidate = _profile;
  (void)decodeShuntMicroVolts(
      encoded,
      candidate.alerts.warningLimitMicroVolts[static_cast<uint8_t>(ch)]);
  Status st = _writeManagedRegisterVerified(warnRegAddr(ch), encoded);
  if (st.ok()) {
    _profile = candidate;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
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
  const uint16_t encoded = static_cast<uint16_t>(raw) & kShuntSumLimitWritable;
  DeviceProfile candidate = _profile;
  candidate.alerts.shuntSumLimitMicroVolts =
      static_cast<int32_t>(
          signExtendField(encoded, cmd::SUM_DATA_SHIFT, 15)) * 40;
  Status st = _writeManagedRegisterVerified(cmd::REG_SHUNT_SUM_LIMIT, encoded);
  if (st.ok()) {
    _profile = candidate;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  const uint16_t encoded = static_cast<uint16_t>(raw) & kPowerValidLimitWritable;
  int32_t milliVolts = 0;
  (void)decodeBusMilliVolts(encoded, milliVolts);
  if (!validatePowerValidWindow(_profile.alerts.powerValidLowerMilliVolts,
                                static_cast<uint32_t>(milliVolts)).ok()) {
    return Status::Error(Err::OUT_OF_RANGE, "Invalid power-valid upper limit");
  }
  DeviceProfile candidate = _profile;
  candidate.alerts.powerValidUpperMilliVolts =
      static_cast<uint32_t>(milliVolts);
  Status st = _writeManagedRegisterVerified(cmd::REG_PV_UPPER_LIMIT, encoded);
  if (st.ok()) {
    _profile = candidate;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  const uint16_t encoded = static_cast<uint16_t>(raw) & kPowerValidLimitWritable;
  int32_t milliVolts = 0;
  (void)decodeBusMilliVolts(encoded, milliVolts);
  if (!validatePowerValidWindow(static_cast<uint32_t>(milliVolts),
                                _profile.alerts.powerValidUpperMilliVolts).ok()) {
    return Status::Error(Err::OUT_OF_RANGE, "Invalid power-valid lower limit");
  }
  DeviceProfile candidate = _profile;
  candidate.alerts.powerValidLowerMilliVolts =
      static_cast<uint32_t>(milliVolts);
  Status st = _writeManagedRegisterVerified(cmd::REG_PV_LOWER_LIMIT, encoded);
  if (st.ok()) {
    _profile = candidate;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
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

  _decodeAlertFlags(regVal, flags);

  if (flags.conversionReady && _conversionStarted) {
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;

  DeviceProfile nextProfile = _profile;
  nextProfile.alerts.summationChannels = static_cast<ChannelMask>(
      (ch1 ? CHANNEL_1 : 0U) | (ch2 ? CHANNEL_2 : 0U) |
      (ch3 ? CHANNEL_3 : 0U));
  Status st = _validateProfile(nextProfile);
  if (!st.ok()) return st;

  uint8_t reg = 0;
  uint16_t regVal = 0;
  st = _desiredRegister(8U, nextProfile, reg, regVal);
  if (!st.ok()) return st;
  st = _writeManagedRegisterVerified(reg, regVal);
  if (st.ok()) {
    _profile = nextProfile;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
}

Status INA3221::setAlertLatchEnable(bool warningLatch, bool criticalLatch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;

  DeviceProfile candidate = _profile;
  candidate.alerts.warningLatch = warningLatch;
  candidate.alerts.criticalLatch = criticalLatch;
  uint8_t reg = 0;
  uint16_t regVal = 0;
  Status st = _desiredRegister(8U, candidate, reg, regVal);
  if (!st.ok()) return st;
  st = _writeManagedRegisterVerified(reg, regVal);
  if (st.ok()) {
    _profile = candidate;
    if (_profileGeneration < UINT32_MAX) ++_profileGeneration;
  }
  return st;
}

Status INA3221::startApplyMaskEnable(uint16_t writableBits) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  DeviceProfile next = _profile;
  const uint16_t bits = static_cast<uint16_t>(writableBits & kMaskEnableWritable);
  next.alerts.summationChannels = static_cast<ChannelMask>(
      ((bits & cmd::MASK_SCC1) != 0U ? CHANNEL_1 : 0U) |
      ((bits & cmd::MASK_SCC2) != 0U ? CHANNEL_2 : 0U) |
      ((bits & cmd::MASK_SCC3) != 0U ? CHANNEL_3 : 0U));
  next.alerts.warningLatch = (bits & cmd::MASK_WEN) != 0U;
  next.alerts.criticalLatch = (bits & cmd::MASK_CEN) != 0U;
  uint64_t durationMs = 0;
  Status st = _compatibilityDurationMs(JobKind::APPLY_PROFILE, next,
                                       next.mode, durationMs);
  if (!st.ok()) return st;
  st = startApplyProfile(next, UINT32_MAX - 4U, UINT64_MAX);
  if (st.inProgress()) {
    _clearPollJob();
    _pollJobKind = PollJobKind::APPLY_MASK_ENABLE;
    _pollJobStage = PollJobStage::WRITE_MASK_ENABLE;
    _pollDeadlineDurationMs = durationMs;
  }
  return st;
}

Status INA3221::pollApplyMaskEnable(uint32_t nowMs, uint8_t maxInstructions) {
  if (_pollJobKind != PollJobKind::APPLY_MASK_ENABLE ||
      _jobKind != JobKind::APPLY_PROFILE) {
    return Status::Error(Err::JOB_KIND_MISMATCH, "Mask/Enable job is not active");
  }
  PollContext context{};
  Status prepared = _prepareCompatibilityPoll(nowMs, maxInstructions, context);
  if (!prepared.ok()) return prepared;
  Status st = pollJob(context);
  JobProgress progress{};
  (void)getJobProgress(progress);
  _pollInstructionsLast = progress.lastPollTransfers;
  _pollInstructionsTotal = progress.totalTransfers;
  if (_hasPendingJobResult) {
    JobResult result{};
    (void)takeJobResult(result);
    if (result.status.ok()) _pollJobStage = PollJobStage::COMPLETE;
    return result.status;
  }
  return st;
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
  uint32_t shuntUs = convTimeUs(_legacyConfigView.vShCt);
  uint32_t busUs = convTimeUs(_legacyConfigView.vBusCt);

  Mode mode = _legacyConfigView.mode;
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
  const uint8_t channels = static_cast<uint8_t>(
      (_legacyConfigView.ch1Enable ? 1U : 0U) +
      (_legacyConfigView.ch2Enable ? 1U : 0U) +
      (_legacyConfigView.ch3Enable ? 1U : 0U));
  return getConversionTimeUs() * channels *
         averagingSampleCount(_legacyConfigView.averaging);
}

// ============================================================================
// Transport Wrappers
// ============================================================================

Status INA3221::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                 uint8_t* rxBuf, size_t rxLen) {
  return _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen,
                          _transport.defaultTransferTimeoutMs);
}

Status INA3221::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                 uint8_t* rxBuf, size_t rxLen,
                                 uint32_t timeoutMs) {
  _lastCallbackInvoked = false;
  if (_transport.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback missing");
  }
  if (txBuf == nullptr || txLen == 0 || rxBuf == nullptr || rxLen == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  _lastCallbackInvoked = true;
  return _transport.i2cWriteRead(_profile.i2cAddress, txBuf, txLen,
                                 rxBuf, rxLen, timeoutMs,
                                 _transport.i2cUser);
}

Status INA3221::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  return _i2cWriteRaw(buf, len, _transport.defaultTransferTimeoutMs);
}

Status INA3221::_i2cWriteRaw(const uint8_t* buf, size_t len,
                             uint32_t timeoutMs) {
  _lastCallbackInvoked = false;
  if (_transport.i2cWrite == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback missing");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }
  _lastCallbackInvoked = true;
  return _transport.i2cWrite(_profile.i2cAddress, buf, len,
                             timeoutMs, _transport.i2cUser);
}

Status INA3221::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen) {
  return _i2cWriteReadTracked(txBuf, txLen, rxBuf, rxLen,
                              _transport.defaultTransferTimeoutMs);
}

Status INA3221::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen,
                                     uint32_t timeoutMs) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen, timeoutMs);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status INA3221::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  return _i2cWriteTracked(buf, len, _transport.defaultTransferTimeoutMs);
}

Status INA3221::_i2cWriteTracked(const uint8_t* buf, size_t len,
                                 uint32_t timeoutMs) {
  Status st = _i2cWriteRaw(buf, len, timeoutMs);
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  return _readRegister16Tracked(reg, value);
}

Status INA3221::writeRegister16(uint8_t reg, uint16_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register address");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (!isWritableRegisterAddress(reg)) {
    return Status::Error(Err::READ_ONLY_REGISTER, "Register is read-only", reg);
  }
  Status st = _writeRegister16Tracked(reg, value);
  const bool resetWrite = reg == cmd::REG_CONFIG &&
                          (value & cmd::MASK_RST) != 0U;
  const bool configWriteMayHaveTakenEffect =
      reg == cmd::REG_CONFIG && !resetWrite &&
      (st.ok() || _writeMayHaveReachedDevice(st));
  if (configWriteMayHaveTakenEffect) {
    // Generic register access deliberately does not decode/synchronize the
    // legacy Configuration view, so it cannot establish fresh provenance.
    _clearLegacyConversionState();
    _clearPollJob();
  }
  if (resetWrite && st.ok()) {
    _handleResetWriteEffect(true);
  } else if (resetWrite && _writeMayHaveReachedDevice(st)) {
    _handleResetWriteEffect(false);
  } else if (st.ok()) {
    _markRegisterUnknown(reg);
  } else if (_writeMayHaveReachedDevice(st)) {
    _markRegisterUnknown(reg);
  }
  return st;
}

Status INA3221::_readRegister16Tracked(uint8_t reg, uint16_t& value) {
  if (reg == cmd::REG_MASK_ENABLE) {
    return _readMaskEnableWithTimeout(
        value, nullptr, _transport.defaultTransferTimeoutMs, true);
  }
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

Status INA3221::_writeManagedRegisterVerified(uint8_t reg, uint16_t value,
                                               bool* writeConfirmed) {
  if (writeConfirmed != nullptr) *writeConfirmed = false;
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  const bool measurement = _registerIsMeasurementConfig(reg);
  const AppliedConfigState priorState =
      measurement ? _measurementConfigState : _alertConfigState;
  const bool priorHardwareDirty = _hardwareConfigDirty;
  const Status priorHardwareDirtyStatus = _hardwareConfigDirtyStatus;
  Status st = _writeRegister16Tracked(reg, value);
  if (!st.ok()) {
    if (_writeMayHaveReachedDevice(st)) _markRegisterUnknown(reg);
    return st;
  }
  if (writeConfirmed != nullptr) *writeConfirmed = true;
  _markRegisterDirty(reg);
  uint16_t actual = 0;
  st = _readRegister16Tracked(reg, actual);
  if (!st.ok()) return st;
  if (!_registerMatches(reg, actual, value)) {
    return Status::Error(Err::PROFILE_MISMATCH,
                         "Managed register verification mismatch", reg);
  }
  if (measurement) {
    _measurementConfigState = AppliedConfigState::APPLIED;
  } else {
    _alertConfigState = priorState == AppliedConfigState::APPLIED
                            ? AppliedConfigState::APPLIED
                            : priorState;
  }
  if (_measurementConfigState == AppliedConfigState::APPLIED &&
      _alertConfigState == AppliedConfigState::APPLIED) {
    _clearHardwareConfigDirty();
  } else if (priorHardwareDirty) {
    _hardwareConfigDirty = true;
    _hardwareConfigDirtyStatus = priorHardwareDirtyStatus;
  }
  return Status::Ok();
}

Status INA3221::_applyConfigVerified(const DeviceProfile& profile) {
  bool writeConfirmed = false;
  const Status st = _writeManagedRegisterVerified(
      cmd::REG_CONFIG, _buildConfigRegister(profile), &writeConfirmed);
  if (!st.ok() &&
      (writeConfirmed || _writeMayHaveReachedDevice(st))) {
    _clearLegacyConversionState();
  }
  return st;
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
  if (st.inProgress()) {
    return st;
  }

  uint32_t nowMs = _nowMs();

  if (st.ok()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }

    if (_initialized) _driverState = DriverState::READY;
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
      if (_consecutiveFailures >= _transport.offlineThreshold) {
        _driverState = DriverState::OFFLINE;
      } else {
        _driverState = DriverState::DEGRADED;
      }
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
    if (_consecutiveFailures >= _transport.offlineThreshold) {
      _driverState = DriverState::OFFLINE;
    } else {
      _driverState = DriverState::DEGRADED;
    }
  }

  return st;
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
  _pollTimeInitialized = false;
  _pollLastNowMs = 0;
  _pollExtendedNowMs = 0;
  _pollDeadlineDurationMs = 0;
  _pollNextChannel = 0;
  _pollInstructionsLast = 0;
  _pollInstructionsTotal = 0;
  _resetPollSampleCache();
}

void INA3221::_clearLegacyConversionState() {
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _conversionReadyDelayMs = 0;
}

void INA3221::_resetPollSampleCache() {
  for (uint8_t i = 0; i < 3; ++i) {
    _pollChannels[i] = ChannelRawMeasurement{};
  }
}

Status INA3221::_readConversionReadyAt(uint32_t nowMs, bool& ready) {
  ready = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;

  if (_conversionReady) {
    ready = true;
    return Status::Ok();
  }

  const Mode activeMode = _legacyConfigView.mode;
  if (isTriggeredMode(activeMode)) {
    if (!_conversionStarted) {
      return Status::Ok();
    }
    if ((nowMs - _conversionStartMs) < _conversionReadyDelayMs) {
      return Status::Ok();
    }
  }

  if (!isTriggeredMode(activeMode) && !isContinuousMode(activeMode)) {
    return Status::Ok();
  }

  uint16_t maskEn = 0;
  Status st = readRegister16(cmd::REG_MASK_ENABLE, maskEn);
  if (!st.ok()) {
    return st;
  }

  if ((maskEn & cmd::MASK_CVRF) != 0U) {
    ready = true;
    if (isTriggeredMode(activeMode)) {
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
  Status available = _requireNoOwnerJob();
  if (!available.ok()) return available;
  if (_measurementConfigState != AppliedConfigState::APPLIED) {
    return Status::Error(Err::CONFIG_UNKNOWN, "Measurement profile not verified");
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

void INA3221::_handleConfigWriteSideEffects(
    const DeviceProfile& activeProfile) {
  _clearPollJob();
  if (isTriggeredMode(activeProfile.mode)) {
    uint64_t cycleUs = 0;
    const Status timing = maximumCycleTimeUs(
        activeProfile, activeProfile.mode, cycleUs);
    if (timing.ok() && cycleUs <= UINT64_MAX - CONVERSION_WAKE_MARGIN_US) {
      cycleUs += CONVERSION_WAKE_MARGIN_US;
      const uint64_t cycleMs = (cycleUs + 999U) / 1000U;
      _conversionReadyDelayMs = cycleMs > UINT32_MAX
                                    ? UINT32_MAX
                                    : static_cast<uint32_t>(cycleMs);
    } else {
      _conversionReadyDelayMs = UINT32_MAX;
    }
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = _nowMs();
    return;
  }

  _clearLegacyConversionState();
}

void INA3221::_handleResetWriteEffect(bool confirmed) {
  _clearLegacyConversionState();
  _clearPollJob();
  if (confirmed) {
    _maskEnableWritableCache = 0;
    _legacyConfigView.ch1Enable = true;
    _legacyConfigView.ch2Enable = true;
    _legacyConfigView.ch3Enable = true;
    _legacyConfigView.averaging = Averaging::AVG_1;
    _legacyConfigView.vBusCt = ConvTime::CT_1100US;
    _legacyConfigView.vShCt = ConvTime::CT_1100US;
    _legacyConfigView.mode = Mode::SHUNT_BUS_CONT;
    _measurementConfigState = AppliedConfigState::DIRTY;
    _alertConfigState = AppliedConfigState::DIRTY;
    _markRegisterDirty(cmd::REG_CONFIG);
  } else {
    _measurementConfigState = AppliedConfigState::UNKNOWN;
    _alertConfigState = AppliedConfigState::UNKNOWN;
    _markRegisterUnknown(cmd::REG_CONFIG);
  }
}

uint16_t INA3221::_buildConfigRegister(const DeviceProfile& profile) const {
  uint16_t config = 0;
  if ((profile.enabledChannels & CHANNEL_1) != 0U) config |= cmd::MASK_CH1EN;
  if ((profile.enabledChannels & CHANNEL_2) != 0U) config |= cmd::MASK_CH2EN;
  if ((profile.enabledChannels & CHANNEL_3) != 0U) config |= cmd::MASK_CH3EN;
  config |= (static_cast<uint16_t>(profile.averaging) << cmd::BIT_AVG) & cmd::MASK_AVG;
  config |= (static_cast<uint16_t>(profile.vBusCt) << cmd::BIT_VBUSCT) & cmd::MASK_VBUSCT;
  config |= (static_cast<uint16_t>(profile.vShCt) << cmd::BIT_VSHCT) & cmd::MASK_VSHCT;
  config |= (static_cast<uint16_t>(profile.mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  return config;
}

uint32_t INA3221::_nowMs() const {
  if (_transport.nowMs != nullptr) {
    return _transport.nowMs(_transport.timeUser);
  }
  return 0;
}

void INA3221::_cooperativeYield() const {
  if (_transport.cooperativeYield != nullptr) {
    _transport.cooperativeYield(_transport.timeUser);
    return;
  }
}

uint8_t INA3221::_enabledChannelCount() const {
  return countEnabledChannels(_profile);
}

bool INA3221::_isTriggeredMode() const {
  return isTriggeredMode(_profile.mode);
}

bool INA3221::_isContinuousMode() const {
  return isContinuousMode(_profile.mode);
}

} // namespace INA3221
