/// @file test_owner_operations.cpp
/// @brief Cooperative-owner, timing, and fault-injection contract tests.

#include <unity.h>

#include <cstdint>
#include <limits>
#include <type_traits>

#include "INA3221/INA3221.h"
#include "support/ScriptedTransport.h"

using namespace INA3221;
using INA3221Test::ScriptedTransport;

namespace {

static constexpr size_t NO_FAILURE = static_cast<size_t>(-1);
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 10;

struct ExpectedRegister {
  uint8_t reg;
  uint16_t value;
};

DeviceProfile makeProfile(uint8_t address = 0x42,
                          ChannelMask enabled = ALL_CHANNELS,
                          Mode mode = Mode::SHUNT_BUS_CONT) {
  DeviceProfile profile;
  profile.i2cAddress = address;
  profile.enabledChannels = enabled;
  profile.averaging = Averaging::AVG_1;
  profile.vBusCt = ConvTime::CT_140US;
  profile.vShCt = ConvTime::CT_140US;
  profile.mode = mode;
  for (uint8_t i = 0; i < 3U; ++i) {
    profile.shunts[i].resistanceMicroOhms =
        (enabled & static_cast<ChannelMask>(1U << i)) != 0U ? 100000U : 0U;
  }
  profile.alerts.criticalLimitMicroVolts[0] = -40000;
  profile.alerts.warningLimitMicroVolts[0] = 20000;
  profile.alerts.criticalLimitMicroVolts[1] = 80000;
  profile.alerts.warningLimitMicroVolts[1] = 60000;
  profile.alerts.criticalLimitMicroVolts[2] = 120000;
  profile.alerts.warningLimitMicroVolts[2] = 100000;
  profile.alerts.summationChannels =
      static_cast<ChannelMask>(enabled & static_cast<ChannelMask>(CHANNEL_1 | CHANNEL_3));
  profile.alerts.shuntSumLimitMicroVolts = 160000;
  profile.alerts.powerValidLowerMilliVolts = 8000;
  profile.alerts.powerValidUpperMilliVolts = 12000;
  profile.alerts.warningLatch = true;
  profile.alerts.criticalLatch = true;
  return profile;
}

uint16_t configValue(const DeviceProfile& profile, Mode mode) {
  uint16_t value = 0;
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
      (static_cast<uint16_t>(mode) << cmd::BIT_MODE) & cmd::MASK_MODE);
  return value;
}

size_t buildExpectedProfile(const DeviceProfile& profile,
                            ExpectedRegister (&out)[11]) {
  out[0] = {cmd::REG_CONFIG, configValue(profile, profile.mode)};
  uint16_t raw = 0;
  for (uint8_t channel = 0; channel < 3U; ++channel) {
    (void)INA3221::INA3221::encodeShuntMicroVolts(
        profile.alerts.criticalLimitMicroVolts[channel], raw);
    out[1U + channel * 2U] = {
        static_cast<uint8_t>(cmd::REG_CH1_CRIT_LIMIT + channel * 2U), raw};
    (void)INA3221::INA3221::encodeShuntMicroVolts(
        profile.alerts.warningLimitMicroVolts[channel], raw);
    out[2U + channel * 2U] = {
        static_cast<uint8_t>(cmd::REG_CH1_WARN_LIMIT + channel * 2U), raw};
  }
  (void)INA3221::INA3221::encodeShuntSumMicroVolts(
      profile.alerts.shuntSumLimitMicroVolts, raw);
  out[7] = {cmd::REG_SHUNT_SUM_LIMIT, raw};
  raw = 0;
  if ((profile.alerts.summationChannels & CHANNEL_1) != 0U) raw |= cmd::MASK_SCC1;
  if ((profile.alerts.summationChannels & CHANNEL_2) != 0U) raw |= cmd::MASK_SCC2;
  if ((profile.alerts.summationChannels & CHANNEL_3) != 0U) raw |= cmd::MASK_SCC3;
  if (profile.alerts.warningLatch) raw |= cmd::MASK_WEN;
  if (profile.alerts.criticalLatch) raw |= cmd::MASK_CEN;
  out[8] = {cmd::REG_MASK_ENABLE, raw};
  (void)INA3221::INA3221::encodeBusMilliVolts(
      static_cast<int32_t>(profile.alerts.powerValidUpperMilliVolts), raw);
  out[9] = {cmd::REG_PV_UPPER_LIMIT, raw};
  (void)INA3221::INA3221::encodeBusMilliVolts(
      static_cast<int32_t>(profile.alerts.powerValidLowerMilliVolts), raw);
  out[10] = {cmd::REG_PV_LOWER_LIMIT, raw};
  return 11U;
}

DeviceProfile changedAtManagedRegister(const DeviceProfile& initial,
                                       uint8_t index) {
  DeviceProfile changed = initial;
  switch (index) {
    case 0U: changed.averaging = Averaging::AVG_4; break;
    case 1U: changed.alerts.criticalLimitMicroVolts[0] += 40; break;
    case 2U: changed.alerts.warningLimitMicroVolts[0] += 40; break;
    case 3U: changed.alerts.criticalLimitMicroVolts[1] += 40; break;
    case 4U: changed.alerts.warningLimitMicroVolts[1] += 40; break;
    case 5U: changed.alerts.criticalLimitMicroVolts[2] += 40; break;
    case 6U: changed.alerts.warningLimitMicroVolts[2] += 40; break;
    case 7U: changed.alerts.shuntSumLimitMicroVolts += 40; break;
    case 8U: changed.alerts.warningLatch = !initial.alerts.warningLatch; break;
    case 9U: changed.alerts.powerValidUpperMilliVolts += 8U; break;
    case 10U: changed.alerts.powerValidLowerMilliVolts -= 8U; break;
    default: break;
  }
  return changed;
}

void forceAllProfileMismatches(ScriptedTransport& bus,
                               const ExpectedRegister (&expected)[11]) {
  for (size_t i = 0; i < 11U; ++i) {
    uint16_t different = static_cast<uint16_t>(expected[i].value ^ 0x0008U);
    if (expected[i].reg == cmd::REG_CONFIG) {
      different = static_cast<uint16_t>(expected[i].value ^ cmd::MASK_CH2EN);
    } else if (expected[i].reg == cmd::REG_MASK_ENABLE) {
      different = static_cast<uint16_t>(expected[i].value ^ cmd::MASK_WEN);
    }
    bus.setRegister(expected[i].reg, different);
  }
}

bool queueProfileSequence(ScriptedTransport& bus, const DeviceProfile& profile,
                          bool includeIdentity, size_t failureIndex = NO_FAILURE,
                          Status failure = Status::Error(
                              Err::I2C_NACK_ADDR, "injected stage failure", -71)) {
  ExpectedRegister expected[11]{};
  (void)buildExpectedProfile(profile, expected);
  forceAllProfileMismatches(bus, expected);
  size_t transfer = 0;

  auto addRead = [&](uint8_t reg) {
    const bool fail = transfer == failureIndex;
    const bool queued = bus.expectRead(reg, fail ? failure : Status::Ok());
    ++transfer;
    return queued && !fail;
  };
  auto addWrite = [&](uint8_t reg, uint16_t value) {
    const bool fail = transfer == failureIndex;
    const bool queued = bus.expectWrite(reg, value, fail ? failure : Status::Ok());
    ++transfer;
    return queued && !fail;
  };

  if (includeIdentity) {
    if (!addRead(cmd::REG_MANUFACTURER_ID)) return true;
    if (!addRead(cmd::REG_DIE_ID)) return true;
  }
  for (size_t i = 0; i < 11U; ++i) {
    if (!addRead(expected[i].reg)) return true;
    if (!addWrite(expected[i].reg, expected[i].value)) return true;
    if (!addRead(expected[i].reg)) return true;
  }
  return !bus.hasViolation();
}

PollContext pollContext(uint64_t nowMs, uint64_t deadlineMs = 1000,
                        uint32_t timeoutMs = DEFAULT_TIMEOUT_MS,
                        uint8_t maxTransfers = 35U) {
  PollContext context;
  context.nowMs = nowMs;
  context.deadlineMs = deadlineMs;
  context.transferTimeoutMs = timeoutMs;
  context.maxTransfers = maxTransfers;
  return context;
}

bool initializeApplied(INA3221::INA3221& device, ScriptedTransport& bus,
                       const DeviceProfile& profile, uint32_t requestId = 1U) {
  if (!device.bind(bus.makeTransportConfig(), profile).ok()) return false;
  if (!queueProfileSequence(bus, profile, true)) return false;
  if (!device.startInitialize(requestId, 1000).inProgress()) return false;
  if (!device.pollJob(pollContext(1)).ok()) return false;
  JobResult result{};
  if (!device.takeJobResult(result).ok()) return false;
  return result.state == JobTerminalState::SUCCEEDED && result.status.ok() &&
         result.requestId == requestId && bus.scriptConsumed() &&
         device.measurementConfigState() == AppliedConfigState::APPLIED &&
         device.alertConfigState() == AppliedConfigState::APPLIED;
}

bool queueTriggeredSample(ScriptedTransport& bus, const DeviceProfile& profile,
                          Mode mode, uint16_t maskValue) {
  bus.setRegister(cmd::REG_MASK_ENABLE, maskValue);
  if (!bus.expectWrite(cmd::REG_CONFIG, configValue(profile, mode))) return false;
  if (!bus.expectRead(cmd::REG_MASK_ENABLE)) return false;
  for (uint8_t i = 0; i < 3U; ++i) {
    if ((profile.enabledChannels & static_cast<ChannelMask>(1U << i)) == 0U) continue;
    if (mode == Mode::SHUNT_TRIG || mode == Mode::SHUNT_BUS_TRIG) {
      if (!bus.expectRead(static_cast<uint8_t>(cmd::REG_CH1_SHUNT + i * 2U))) return false;
    }
    if (mode == Mode::BUS_TRIG || mode == Mode::SHUNT_BUS_TRIG) {
      if (!bus.expectRead(static_cast<uint8_t>(cmd::REG_CH1_BUS + i * 2U))) return false;
    }
  }
  return true;
}

bool runTriggeredToTerminal(INA3221::INA3221& device,
                            uint64_t startMs = 100U) {
  Status st = device.pollJob(pollContext(startMs, startMs + 100U,
                                         DEFAULT_TIMEOUT_MS, 1U));
  if (!st.inProgress()) return false;
  st = device.pollJob(pollContext(startMs + 1U, startMs + 100U,
                                  DEFAULT_TIMEOUT_MS, 0U));
  if (!st.inProgress()) return false;
  JobProgress progress{};
  if (!device.getJobProgress(progress).ok()) return false;
  st = device.pollJob(pollContext(progress.readyAtMs, startMs + 100U,
                                  DEFAULT_TIMEOUT_MS, 3U));
  return st.ok();
}

void test_owner_bind_unbind_and_invalid_rebind_are_bus_silent() {
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  DeviceProfile profile = makeProfile();

  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  TEST_ASSERT_TRUE(device.isBound());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());

  DeviceProfile invalid = profile;
  invalid.shunts[0].resistanceMicroOhms = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(
                              device.bind(bus.makeTransportConfig(), invalid).code));
  TEST_ASSERT_TRUE(device.isBound());
  TEST_ASSERT_EQUAL_HEX8(profile.i2cAddress, device.deviceProfile().i2cAddress);
  TEST_ASSERT_EQUAL_PTR(&bus, device.transportConfig().i2cUser);

  TransportConfig badTransport = bus.makeTransportConfig();
  badTransport.i2cWrite = nullptr;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(device.bind(badTransport, profile).code));
  TEST_ASSERT_TRUE(device.isBound());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());

  device.unbind();
  TEST_ASSERT_FALSE(device.isBound());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
}

void test_owner_bind_requires_explicit_calibration_only_for_enabled_channels() {
  ScriptedTransport bus;
  INA3221::INA3221 device;
  DeviceProfile profile = makeProfile(0x40, CHANNEL_1);
  profile.shunts[1].resistanceMicroOhms = 0;
  profile.shunts[2].resistanceMicroOhms = 0;
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  device.unbind();

  profile.shunts[0].resistanceMicroOhms = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(
                              device.bind(bus.makeTransportConfig(), profile).code));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
}

void test_compatibility_profile_setters_reject_invalid_candidates_bus_silent() {
  const DeviceProfile profile =
      makeProfile(0x42, static_cast<ChannelMask>(CHANNEL_1 | CHANNEL_2));
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::INVALID_CONFIG),
      static_cast<uint8_t>(device.setChannelEnable(Channel::CH1, false).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::INVALID_CONFIG),
      static_cast<uint8_t>(device.setChannelEnable(Channel::CH3, true).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::INVALID_CONFIG),
      static_cast<uint8_t>(
          device.setSummationChannels(false, false, true).code));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
  TEST_ASSERT_EQUAL_HEX8(profile.enabledChannels,
                         device.deviceProfile().enabledChannels);
  TEST_ASSERT_EQUAL_HEX8(profile.alerts.summationChannels,
                         device.deviceProfile().alerts.summationChannels);
}

void test_owner_trigger_admission_and_post_callback_time_origin_are_safe() {
  {
    DeviceProfile profile =
        makeProfile(0x42, ALL_CHANNELS, Mode::SHUNT_BUS_TRIG);
    profile.averaging = Averaging::AVG_1024;
    profile.vBusCt = ConvTime::CT_8244US;
    profile.vShCt = ConvTime::CT_8244US;
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    TEST_ASSERT_TRUE(device.startTriggeredSample(
        Mode::SHUNT_BUS_TRIG, 211U, 1000U).inProgress());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Err::DEADLINE_EXPIRED),
        static_cast<uint8_t>(device.pollJob(
            pollContext(0U, 1000U, DEFAULT_TIMEOUT_MS, 1U)).code));
    TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::TIMED_OUT),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::NONE),
                            static_cast<uint8_t>(result.hardwareEffect));
  }
  {
    const DeviceProfile profile =
        makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CVRF);
    bus.expectWrite(cmd::REG_CONFIG,
                    configValue(profile, Mode::SHUNT_BUS_TRIG));
    bus.expectRead(cmd::REG_MASK_ENABLE);
    bus.expectRead(cmd::REG_CH1_SHUNT);
    bus.expectRead(cmd::REG_CH1_BUS);
    TEST_ASSERT_TRUE(device.startTriggeredSample(
        Mode::SHUNT_BUS_TRIG, 212U, 1000U).inProgress());
    TEST_ASSERT_TRUE(device.pollJob(
        pollContext(100U, 1000U, DEFAULT_TIMEOUT_MS, 1U)).inProgress());
    const uint32_t afterTrigger = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_TRUE(device.pollJob(
        pollContext(100U, 1000U, DEFAULT_TIMEOUT_MS, 1U)).inProgress());
    TEST_ASSERT_EQUAL_UINT32(afterTrigger, bus.callCount());
    JobProgress progress{};
    TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
    TEST_ASSERT_EQUAL_UINT64(0U, progress.readyAtMs);
    TEST_ASSERT_TRUE(device.pollJob(
        pollContext(120U, 1000U, DEFAULT_TIMEOUT_MS, 0U)).inProgress());
    TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
    TEST_ASSERT_EQUAL_UINT64(121U, progress.readyAtMs);
    TEST_ASSERT_TRUE(device.pollJob(
        pollContext(121U, 1000U, DEFAULT_TIMEOUT_MS, 3U)).ok());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_TRUE(result.sampleValid);
  }
}

void test_owner_initialize_full_profile_exact_sequence_and_verification() {
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  const DeviceProfile profile = makeProfile();
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  DeviceProfile beforeInitialize = profile;
  beforeInitialize.averaging = Averaging::AVG_4;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(device.startApplyProfile(
                              beforeInitialize, 1U, 1000U).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(
                              device.startReconcile(2U, 1000U).code));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
  TEST_ASSERT_TRUE(queueProfileSequence(bus, profile, true));
  TEST_ASSERT_TRUE(device.startInitialize(0x11223344U, 1000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1)).ok());
  TEST_ASSERT_TRUE(bus.scriptConsumed());
  TEST_ASSERT_EQUAL_UINT32(35U, bus.callCount());

  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobKind::INITIALIZE),
                          static_cast<uint8_t>(result.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::SUCCEEDED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::CONFIRMED),
                          static_cast<uint8_t>(result.hardwareEffect));
  TEST_ASSERT_EQUAL_UINT32(0x11223344U, result.requestId);
  TEST_ASSERT_EQUAL_UINT16(35U, result.transfers);
  TEST_ASSERT_EQUAL_UINT32(1U, result.profileGeneration);
  TEST_ASSERT_TRUE(device.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::APPLIED),
                          static_cast<uint8_t>(device.measurementConfigState()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::APPLIED),
                          static_cast<uint8_t>(device.alertConfigState()));

  bus.resetHarness();
  DeviceProfile movedAddress = profile;
  movedAddress.i2cAddress = 0x43;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(device.startApplyProfile(
                              movedAddress, 3U, 2000U).code));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());

  DeviceProfile changed = profile;
  changed.averaging = Averaging::AVG_16;
  changed.alerts.warningLimitMicroVolts[1] = 40000;
  changed.alerts.powerValidUpperMilliVolts = 14000;
  TEST_ASSERT_TRUE(queueProfileSequence(bus, changed, false));
  TEST_ASSERT_TRUE(device.startApplyProfile(changed, 0x55667788U, 2000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(2, 2000)).ok());
  TEST_ASSERT_TRUE(bus.scriptConsumed());
  TEST_ASSERT_EQUAL_UINT32(33U, bus.callCount());
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobKind::APPLY_PROFILE),
                          static_cast<uint8_t>(result.kind));
  TEST_ASSERT_EQUAL_UINT32(2U, result.profileGeneration);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Averaging::AVG_16),
                          static_cast<uint8_t>(device.deviceProfile().averaging));
}

void test_owner_initialize_failure_at_every_transfer_stage() {
  const DeviceProfile profile = makeProfile();
  for (size_t failureIndex = 0; failureIndex < 35U; ++failureIndex) {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
    TEST_ASSERT_TRUE(queueProfileSequence(bus, profile, true, failureIndex));
    TEST_ASSERT_TRUE(device.startInitialize(100U + static_cast<uint32_t>(failureIndex),
                                            1000).inProgress());
    Status st = device.pollJob(pollContext(1));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_ADDR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_TRUE(bus.scriptConsumed());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    const JobTerminalState expected = failureIndex >= 4U
                                          ? JobTerminalState::PARTIAL
                                          : JobTerminalState::FAILED;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(failureIndex + 1U),
                             result.transfers);
  }
}

void test_owner_apply_profile_failure_at_every_transfer_stage() {
  const DeviceProfile initial = makeProfile();
  DeviceProfile changed = initial;
  changed.averaging = Averaging::AVG_4;
  changed.alerts.warningLimitMicroVolts[2] = 140000;
  changed.alerts.powerValidUpperMilliVolts = 13000;
  for (size_t failureIndex = 0; failureIndex < 33U; ++failureIndex) {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, initial));
    bus.resetHarness();
    TEST_ASSERT_TRUE(queueProfileSequence(bus, changed, false, failureIndex));
    TEST_ASSERT_TRUE(device.startApplyProfile(
        changed, 200U + static_cast<uint32_t>(failureIndex), 1000).inProgress());
    Status st = device.pollJob(pollContext(1));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_ADDR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_TRUE(bus.scriptConsumed());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    const JobTerminalState expected = failureIndex >= 2U
                                          ? JobTerminalState::PARTIAL
                                          : JobTerminalState::FAILED;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(result.state));
  }
}

void test_owner_budget_zero_one_and_many_are_exact() {
  const DeviceProfile profile = makeProfile();
  uint16_t maximumTransfers = 0;
  TEST_ASSERT_TRUE(INA3221::INA3221::maximumJobTransfers(
      JobKind::INITIALIZE, profile, maximumTransfers).ok());
  TEST_ASSERT_EQUAL_UINT16(35U, maximumTransfers);
  TEST_ASSERT_TRUE(INA3221::INA3221::maximumJobTransfers(
      JobKind::APPLY_PROFILE, profile, maximumTransfers).ok());
  TEST_ASSERT_EQUAL_UINT16(33U, maximumTransfers);
  TEST_ASSERT_TRUE(INA3221::INA3221::maximumJobTransfers(
      JobKind::TRIGGERED_SAMPLE, profile, maximumTransfers).ok());
  TEST_ASSERT_EQUAL_UINT16(8U, maximumTransfers);
  TEST_ASSERT_TRUE(INA3221::INA3221::maximumJobTransfers(
      JobKind::CONTINUOUS_SAMPLE, profile, maximumTransfers).ok());
  TEST_ASSERT_EQUAL_UINT16(7U, maximumTransfers);
  TEST_ASSERT_TRUE(INA3221::INA3221::maximumJobTransfers(
      JobKind::POWER_DOWN, profile, maximumTransfers).ok());
  TEST_ASSERT_EQUAL_UINT16(3U, maximumTransfers);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  TEST_ASSERT_TRUE(queueProfileSequence(bus, profile, true));
  TEST_ASSERT_TRUE(device.startInitialize(301U, 1000).inProgress());

  TEST_ASSERT_TRUE(device.pollJob(pollContext(1, 1000, DEFAULT_TIMEOUT_MS, 0)).inProgress());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
  JobProgress progress{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_EQUAL_UINT8(0U, progress.lastPollTransfers);

  for (uint8_t transfer = 0; transfer < 35U; ++transfer) {
    const Status st = device.pollJob(pollContext(1, 1000, DEFAULT_TIMEOUT_MS, 1));
    if (transfer < 34U) TEST_ASSERT_TRUE(st.inProgress());
    else TEST_ASSERT_TRUE(st.ok());
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(transfer) + 1U, bus.callCount());
    TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
    TEST_ASSERT_EQUAL_UINT8(1U, progress.lastPollTransfers);
  }
  TEST_ASSERT_TRUE(bus.scriptConsumed());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());

  bus.resetHarness();
  device.unbind();
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  TEST_ASSERT_TRUE(queueProfileSequence(bus, profile, true));
  TEST_ASSERT_TRUE(device.startInitialize(302U, 1000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1)).ok());
  TEST_ASSERT_EQUAL_UINT32(35U, bus.callCount());
}

void test_owner_timeout_clamps_to_remaining_deadline_and_exact_deadline_expires() {
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  const DeviceProfile profile = makeProfile();
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  bus.expectRead(cmd::REG_MANUFACTURER_ID);
  TEST_ASSERT_TRUE(device.startInitialize(401U, 105U).inProgress());
  bus.setExpectedTransport(0x42, 5U);
  TEST_ASSERT_TRUE(device.pollJob(pollContext(100U, 0U, 20U, 1U)).inProgress());
  TEST_ASSERT_EQUAL_UINT32(1U, bus.callCount());
  TEST_ASSERT_EQUAL_UINT32(5U, bus.call(0)->timeoutMs);

  const uint32_t before = static_cast<uint32_t>(bus.callCount());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEADLINE_EXPIRED),
                          static_cast<uint8_t>(
                              device.pollJob(pollContext(105U, 0U, 20U, 1U)).code));
  TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::TIMED_OUT),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::NONE),
                          static_cast<uint8_t>(result.hardwareEffect));

  bus.resetHarness();
  device.unbind();
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  bus.expectRead(cmd::REG_MANUFACTURER_ID);
  bus.expectRead(cmd::REG_DIE_ID);
  bus.setExpectedTransport(0x42, 2U);
  TEST_ASSERT_TRUE(device.startInitialize(402U, 105U).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(100U, 0U, 20U, 2U)).inProgress());
  TEST_ASSERT_EQUAL_UINT32(2U, bus.callCount());
  TEST_ASSERT_EQUAL_UINT32(2U, bus.call(0)->timeoutMs);
  TEST_ASSERT_EQUAL_UINT32(2U, bus.call(1)->timeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(device.cancelJob().code));
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
}

void test_owner_interior_deadline_is_bus_silent_and_terminalizes_timed_out() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  TEST_ASSERT_TRUE(device.startInitialize(499U, 100U).inProgress());

  PollContext context = pollContext(99U, 100U, DEFAULT_TIMEOUT_MS, 2U);
  const Status st = device.pollJob(context);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEADLINE_EXPIRED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::TIMED_OUT),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::NONE),
                          static_cast<uint8_t>(result.hardwareEffect));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::UNKNOWN),
                          static_cast<uint8_t>(device.measurementConfigState()));
}

void test_owner_triggered_interior_deadline_is_partial_but_keeps_config_applied() {
  const DeviceProfile profile =
      makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.expectWrite(cmd::REG_CONFIG,
                  configValue(profile, Mode::SHUNT_BUS_TRIG));

  TEST_ASSERT_TRUE(
      device.startTriggeredSample(Mode::SHUNT_BUS_TRIG, 500U, 100U)
          .inProgress());
  TEST_ASSERT_TRUE(
      device.pollJob(pollContext(0U, 100U, DEFAULT_TIMEOUT_MS, 1U))
          .inProgress());
  TEST_ASSERT_TRUE(
      device.pollJob(pollContext(1U, 100U, DEFAULT_TIMEOUT_MS, 0U))
          .inProgress());
  JobProgress progress{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_LESS_THAN_UINT64(99U, progress.readyAtMs);

  const uint32_t callsBefore = static_cast<uint32_t>(bus.callCount());
  const Status st =
      device.pollJob(pollContext(99U, 100U, DEFAULT_TIMEOUT_MS, 2U));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEADLINE_EXPIRED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(callsBefore, bus.callCount());
  TEST_ASSERT_TRUE(bus.scriptConsumed());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::TIMED_OUT),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                          static_cast<uint8_t>(result.hardwareEffect));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::APPLIED),
                          static_cast<uint8_t>(device.measurementConfigState()));
}

void test_owner_cancel_is_bus_silent_before_and_after_hardware_effects() {
  const DeviceProfile profile =
      makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
    TEST_ASSERT_TRUE(device.startInitialize(501U, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::NONE),
                            static_cast<uint8_t>(result.hardwareEffect));
  }
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
    bus.expectRead(cmd::REG_MANUFACTURER_ID);
    TEST_ASSERT_TRUE(device.startInitialize(504U, 1000).inProgress());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(1, 1000, DEFAULT_TIMEOUT_MS, 1))
                         .inProgress());
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::NONE),
                            static_cast<uint8_t>(result.hardwareEffect));
  }
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    DeviceProfile changed = profile;
    changed.averaging = Averaging::AVG_4;
    ExpectedRegister expected[11]{};
    buildExpectedProfile(changed, expected);
    bus.expectRead(cmd::REG_CONFIG);
    bus.expectWrite(cmd::REG_CONFIG, expected[0].value);
    TEST_ASSERT_TRUE(device.startApplyProfile(changed, 502U, 1000).inProgress());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(1, 1000, DEFAULT_TIMEOUT_MS, 2)).inProgress());
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                            static_cast<uint8_t>(result.hardwareEffect));
    TEST_ASSERT_FALSE(result.mismatchValid);
  }
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    bus.expectWrite(cmd::REG_CONFIG, configValue(profile, Mode::SHUNT_BUS_TRIG));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::IN_PROGRESS),
                            static_cast<uint8_t>(device.startTriggeredSample(
                                Mode::SHUNT_BUS_TRIG, 503U, 1000).code));
    TEST_ASSERT_TRUE(device.pollJob(pollContext(10, 1000, DEFAULT_TIMEOUT_MS, 1))
                         .inProgress());
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                            static_cast<uint8_t>(result.hardwareEffect));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::APPLIED),
                            static_cast<uint8_t>(device.measurementConfigState()));
  }
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CVRF);
    bus.expectWrite(cmd::REG_CONFIG, configValue(profile, Mode::SHUNT_BUS_TRIG));
    bus.expectRead(cmd::REG_MASK_ENABLE);
    bus.expectRead(cmd::REG_CH1_SHUNT);
    TEST_ASSERT_TRUE(device.startTriggeredSample(Mode::SHUNT_BUS_TRIG, 505U, 1000)
                         .inProgress());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(10, 1000, DEFAULT_TIMEOUT_MS, 1))
                         .inProgress());
    JobProgress origin{};
    TEST_ASSERT_TRUE(device.getJobProgress(origin).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobStage::WAIT_CONVERSION),
                            static_cast<uint8_t>(origin.stage));
    TEST_ASSERT_EQUAL_UINT64(0U, origin.readyAtMs);
    TEST_ASSERT_TRUE(device.pollJob(pollContext(11, 1000,
                                                DEFAULT_TIMEOUT_MS, 0))
                         .inProgress());
    JobProgress progress{};
    TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs, 1000,
                                                DEFAULT_TIMEOUT_MS, 2))
                         .inProgress());
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    TEST_ASSERT_TRUE(bus.scriptConsumed());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_FALSE(result.sampleValid);
  }
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    DeviceProfile poweredDown = profile;
    poweredDown.mode = Mode::POWER_DOWN;
    bus.expectRead(cmd::REG_CONFIG);
    bus.expectWrite(cmd::REG_CONFIG, configValue(poweredDown, Mode::POWER_DOWN));
    TEST_ASSERT_TRUE(device.startPowerDown(506U, 1000).inProgress());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(1, 1000, DEFAULT_TIMEOUT_MS, 2))
                         .inProgress());
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                            static_cast<uint8_t>(result.hardwareEffect));
  }
}

void test_owner_cancel_is_bus_silent_across_profile_transfer_stages() {
  const DeviceProfile initial = makeProfile();
  for (uint8_t cancelAfter = 0U; cancelAfter < 35U; ++cancelAfter) {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), initial).ok());
    TEST_ASSERT_TRUE(queueProfileSequence(bus, initial, true));
    TEST_ASSERT_TRUE(device.startInitialize(520U + cancelAfter, 1000U)
                         .inProgress());
    if (cancelAfter != 0U) {
      TEST_ASSERT_TRUE(device.pollJob(pollContext(
          1U, 1000U, DEFAULT_TIMEOUT_MS, cancelAfter)).inProgress());
    }
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::CANCELLED),
                            static_cast<uint8_t>(result.state));
  }

  DeviceProfile changed = initial;
  changed.averaging = Averaging::AVG_4;
  changed.alerts.criticalLimitMicroVolts[0] += 40;
  changed.alerts.warningLimitMicroVolts[1] += 40;
  changed.alerts.powerValidUpperMilliVolts += 8U;
  for (uint8_t cancelAfter = 0U; cancelAfter < 33U; ++cancelAfter) {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, initial));
    bus.resetHarness();
    TEST_ASSERT_TRUE(queueProfileSequence(bus, changed, false));
    TEST_ASSERT_TRUE(device.startApplyProfile(
        changed, 560U + cancelAfter, 1000U).inProgress());
    if (cancelAfter != 0U) {
      TEST_ASSERT_TRUE(device.pollJob(pollContext(
          1U, 1000U, DEFAULT_TIMEOUT_MS, cancelAfter)).inProgress());
    }
    const uint32_t before = static_cast<uint32_t>(bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::CANCELLED),
                            static_cast<uint8_t>(result.state));
  }
}

void test_owner_permanently_low_cvrf_waits_until_owner_deadline() {
  const DeviceProfile profile =
      makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.setRegister(cmd::REG_MASK_ENABLE, 0U);
  bus.expectWrite(cmd::REG_CONFIG, configValue(profile, Mode::SHUNT_BUS_TRIG));
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::IN_PROGRESS),
                          static_cast<uint8_t>(device.startTriggeredSample(
                              Mode::SHUNT_BUS_TRIG, 601U, 100U).code));
  TEST_ASSERT_TRUE(device.pollJob(pollContext(0U, 0U, DEFAULT_TIMEOUT_MS, 1U))
                       .inProgress());
  JobProgress progress{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_EQUAL_UINT64(0U, progress.readyAtMs);
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1U, 0U,
                                              DEFAULT_TIMEOUT_MS, 0U)).inProgress());
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs, 0U,
                                              DEFAULT_TIMEOUT_MS, 1U)).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs + 1U, 0U,
                                              DEFAULT_TIMEOUT_MS, 0U)).inProgress());
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs, 0U,
                                              DEFAULT_TIMEOUT_MS, 1U)).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs + 1U, 0U,
                                              DEFAULT_TIMEOUT_MS, 0U)).inProgress());
  bus.setExpectedTransport(0x42, 1U);
  TEST_ASSERT_TRUE(device.pollJob(pollContext(99U, 0U, DEFAULT_TIMEOUT_MS, 1U))
                       .inProgress());
  const uint32_t before = static_cast<uint32_t>(bus.callCount());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEADLINE_EXPIRED),
                          static_cast<uint8_t>(
                              device.pollJob(pollContext(100U, 0U)).code));
  TEST_ASSERT_EQUAL_UINT32(before, bus.callCount());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::TIMED_OUT),
                          static_cast<uint8_t>(result.state));
}

void test_owner_triggered_sample_retains_alerts_across_low_cvrf_recheck() {
  const DeviceProfile profile =
      makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CF1);
  bus.setRegister(cmd::REG_CH1_SHUNT, 0x0190U);
  bus.setRegister(cmd::REG_CH1_BUS, 0x1770U);
  bus.expectWrite(cmd::REG_CONFIG, configValue(profile, Mode::SHUNT_BUS_TRIG));
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bus.expectRead(cmd::REG_CH1_SHUNT);
  bus.expectRead(cmd::REG_CH1_BUS);

  TEST_ASSERT_TRUE(device.startTriggeredSample(
      Mode::SHUNT_BUS_TRIG, 602U, 1000U).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(
      pollContext(0U, 1000U, DEFAULT_TIMEOUT_MS, 1U)).inProgress());
  JobProgress progress{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_EQUAL_UINT64(0U, progress.readyAtMs);
  TEST_ASSERT_TRUE(device.pollJob(
      pollContext(1U, 1000U, DEFAULT_TIMEOUT_MS, 0U)).inProgress());
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(
      progress.readyAtMs, 1000U, DEFAULT_TIMEOUT_MS, 1U)).inProgress());
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_EQUAL_UINT64(0U, progress.readyAtMs);
  const uint32_t afterLowCvrf = static_cast<uint32_t>(bus.callCount());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(
      2U, 1000U, DEFAULT_TIMEOUT_MS, 1U)).inProgress());
  TEST_ASSERT_EQUAL_UINT32(afterLowCvrf, bus.callCount());
  bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CVRF);
  TEST_ASSERT_TRUE(device.pollJob(pollContext(
      3U, 1000U, DEFAULT_TIMEOUT_MS, 0U)).inProgress());
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(
      progress.readyAtMs, 1000U, DEFAULT_TIMEOUT_MS, 3U)).ok());

  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_TRUE(result.sampleValid);
  TEST_ASSERT_TRUE(result.sample.alertSnapshotValid);
  TEST_ASSERT_BITS_HIGH(cmd::MASK_CF1, result.sample.alerts.events);
  TEST_ASSERT_TRUE(result.sample.alerts.conversionReady);
  TEST_ASSERT_TRUE(bus.scriptConsumed());
}

void test_owner_result_identity_pending_take_once_and_stale_prevention() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(device.bind(bus.makeTransportConfig(), profile).ok());
  TEST_ASSERT_TRUE(queueProfileSequence(bus, profile, true));
  TEST_ASSERT_TRUE(device.startInitialize(0xA5A55A5AU, 1000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1)).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::RESULT_PENDING),
                          static_cast<uint8_t>(device.startReconcile(7U, 1000).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::RESULT_PENDING),
                          static_cast<uint8_t>(
                              device.bind(bus.makeTransportConfig(), profile).code));

  JobProgress progress{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_TRUE(progress.resultPending);
  TEST_ASSERT_EQUAL_UINT32(0xA5A55A5AU, progress.requestId);
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT32(0xA5A55A5AU, result.requestId);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NO_RESULT),
                          static_cast<uint8_t>(device.takeJobResult(result).code));

  bus.resetHarness();
  ExpectedRegister expected[11]{};
  buildExpectedProfile(profile, expected);
  for (size_t i = 0; i < 11U; ++i) bus.expectRead(expected[i].reg);
  TEST_ASSERT_TRUE(device.startReconcile(7U, 1000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1)).ok());
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT32(7U, result.requestId);
}

void test_owner_commit_before_timeout_is_indeterminate_and_not_retried() {
  const DeviceProfile initial = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, initial));
  bus.resetHarness();
  DeviceProfile changed = initial;
  changed.averaging = Averaging::AVG_4;
  ExpectedRegister expected[11]{};
  buildExpectedProfile(changed, expected);
  bus.expectRead(cmd::REG_CONFIG);
  const Status timeout = Status::Error(Err::I2C_TIMEOUT,
                                       "committed before timeout", -72);
  bus.expectWrite(cmd::REG_CONFIG, expected[0].value, timeout,
                  ScriptedTransport::WriteEffect::COMMIT_BEFORE_STATUS);

  TEST_ASSERT_TRUE(device.startApplyProfile(changed, 701U, 1000).inProgress());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(device.pollJob(pollContext(1)).code));
  TEST_ASSERT_TRUE(bus.scriptConsumed());
  TEST_ASSERT_EQUAL_UINT32(2U, bus.callCount());
  TEST_ASSERT_EQUAL_HEX16(expected[0].value, bus.registerValue(cmd::REG_CONFIG));
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::INDETERMINATE),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::INDETERMINATE),
                          static_cast<uint8_t>(result.hardwareEffect));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::UNKNOWN),
                          static_cast<uint8_t>(device.measurementConfigState()));
}

void test_owner_every_managed_write_ambiguity_is_indeterminate_without_retry() {
  const DeviceProfile initial = makeProfile();
  for (uint8_t index = 0; index < 11U; ++index) {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, initial));
    bus.resetHarness();

    const DeviceProfile changed = changedAtManagedRegister(initial, index);
    ExpectedRegister expected[11]{};
    buildExpectedProfile(changed, expected);
    for (uint8_t prior = 0; prior < index; ++prior) {
      bus.expectRead(expected[prior].reg);
    }
    bus.expectRead(expected[index].reg);
    const Status timeout =
        Status::Error(Err::I2C_TIMEOUT, "managed write ambiguous", -720 - index);
    bus.expectWrite(expected[index].reg, expected[index].value, timeout,
                    ScriptedTransport::WriteEffect::COMMIT_BEFORE_STATUS);

    TEST_ASSERT_TRUE(device.startApplyProfile(
        changed, 720U + index, 1000U).inProgress());
    const Status status = device.pollJob(pollContext(1U));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_INT32(-720 - index, status.detail);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(index) + 2U,
                             bus.callCount());
    TEST_ASSERT_TRUE(bus.scriptConsumed());
    TEST_ASSERT_EQUAL_HEX16(expected[index].value,
                            bus.registerValue(expected[index].reg));

    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(JobTerminalState::INDETERMINATE),
        static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::INDETERMINATE),
                            static_cast<uint8_t>(result.hardwareEffect));
    if (index == 0U) {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::UNKNOWN),
                              static_cast<uint8_t>(device.measurementConfigState()));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::APPLIED),
                              static_cast<uint8_t>(device.alertConfigState()));
    } else {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::APPLIED),
                              static_cast<uint8_t>(device.measurementConfigState()));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::UNKNOWN),
                              static_cast<uint8_t>(device.alertConfigState()));
    }
  }
}

void test_owner_every_managed_readback_mismatch_is_partial_and_dirty() {
  const DeviceProfile initial = makeProfile();
  for (uint8_t index = 0; index < 11U; ++index) {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, initial));
    bus.resetHarness();

    const DeviceProfile changed = changedAtManagedRegister(initial, index);
    ExpectedRegister expected[11]{};
    buildExpectedProfile(changed, expected);
    for (uint8_t prior = 0; prior < index; ++prior) {
      bus.expectRead(expected[prior].reg);
    }
    bus.expectRead(expected[index].reg);
    bus.expectWrite(expected[index].reg, expected[index].value);
    uint16_t mismatch = static_cast<uint16_t>(expected[index].value ^ 0x0008U);
    if (expected[index].reg == cmd::REG_CONFIG) {
      mismatch = static_cast<uint16_t>(expected[index].value ^ cmd::MASK_CH2EN);
    } else if (expected[index].reg == cmd::REG_MASK_ENABLE) {
      mismatch = static_cast<uint16_t>(expected[index].value ^ cmd::MASK_WEN);
    }
    TEST_ASSERT_TRUE(bus.mutateAfterLastStep(expected[index].reg, mismatch));
    bus.expectRead(expected[index].reg);

    TEST_ASSERT_TRUE(device.startApplyProfile(
        changed, 760U + index, 1000U).inProgress());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Err::PROFILE_MISMATCH),
        static_cast<uint8_t>(device.pollJob(pollContext(1U)).code));
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(index) + 3U,
                             bus.callCount());
    TEST_ASSERT_TRUE(bus.scriptConsumed());
    JobResult result{};
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::PARTIAL),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                            static_cast<uint8_t>(result.hardwareEffect));
    TEST_ASSERT_TRUE(result.mismatchValid);
    TEST_ASSERT_EQUAL_HEX8(expected[index].reg, result.mismatchRegister);
    TEST_ASSERT_EQUAL_HEX16(expected[index].value, result.mismatchExpected);
    TEST_ASSERT_EQUAL_HEX16(mismatch, result.mismatchActual);
    const uint16_t expectedMask =
        expected[index].reg == cmd::REG_CONFIG
            ? static_cast<uint16_t>(~cmd::MASK_RST)
            : (expected[index].reg == cmd::REG_MASK_ENABLE ? 0x7C00U : 0xFFFFU);
    TEST_ASSERT_EQUAL_HEX16(expectedMask, result.mismatchMask);
    const AppliedConfigState state = index == 0U
                                         ? device.measurementConfigState()
                                         : device.alertConfigState();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::DIRTY),
                            static_cast<uint8_t>(state));
  }
}

void test_failed_partial_mask_read_exposes_alert_evidence_uncertainty() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.setRegister(cmd::REG_MASK_ENABLE,
                  static_cast<uint16_t>(cmd::MASK_CF1 | cmd::MASK_WF2));
  const Status shortRead =
      Status::Error(Err::I2C_ERROR, "partial destructive read", -811);
  bus.expectRead(cmd::REG_MASK_ENABLE, shortRead,
                 ScriptedTransport::ReadErrorEffect::FIRST_BYTE_ONLY);

  AlertFlags flags{};
  const Status status = device.readAlertFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_INT32(-811, status.detail);
  TEST_ASSERT_TRUE(bus.scriptConsumed());
  TEST_ASSERT_TRUE(bus.call(0)->destructiveReadOccurred);
  TEST_ASSERT_EQUAL_HEX16(0U, bus.registerValue(cmd::REG_MASK_ENABLE));

  AlertSnapshot retained{};
  TEST_ASSERT_TRUE(device.peekAlertEvents(retained).ok());
  TEST_ASSERT_TRUE(retained.evidenceUncertain);
  TEST_ASSERT_TRUE(device.takeAlertEvents(retained).ok());
  TEST_ASSERT_TRUE(retained.evidenceUncertain);
  AlertSnapshot acknowledged{};
  TEST_ASSERT_TRUE(device.peekAlertEvents(acknowledged).ok());
  TEST_ASSERT_FALSE(acknowledged.evidenceUncertain);
}

void test_owner_reconcile_reads_first_and_verification_mismatch_is_partial() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  ExpectedRegister expected[11]{};
  buildExpectedProfile(profile, expected);
  for (size_t i = 0; i < 11U; ++i) bus.expectRead(expected[i].reg);
  TEST_ASSERT_TRUE(device.startReconcile(801U, 1000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1)).ok());
  TEST_ASSERT_EQUAL_UINT32(11U, bus.writeReadCallCount());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCallCount());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());

  bus.resetHarness();
  bus.setRegister(cmd::REG_CONFIG,
                  static_cast<uint16_t>(expected[0].value ^ cmd::MASK_CH2EN));
  bus.expectRead(cmd::REG_CONFIG);
  bus.expectWrite(cmd::REG_CONFIG, expected[0].value);
  TEST_ASSERT_TRUE(bus.mutateAfterLastStep(
      cmd::REG_CONFIG, static_cast<uint16_t>(expected[0].value ^ cmd::MASK_CH1EN)));
  bus.expectRead(cmd::REG_CONFIG);
  TEST_ASSERT_TRUE(device.startReconcile(802U, 1000).inProgress());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::PROFILE_MISMATCH),
                          static_cast<uint8_t>(device.pollJob(pollContext(1)).code));
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::PARTIAL),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                          static_cast<uint8_t>(result.hardwareEffect));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::PROFILE_MISMATCH),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_EQUAL_INT32(cmd::REG_CONFIG, result.status.detail);
  TEST_ASSERT_EQUAL_STRING("Profile register verification mismatch", result.status.msg);
  TEST_ASSERT_TRUE(result.mismatchValid);
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_CONFIG, result.mismatchRegister);
  TEST_ASSERT_EQUAL_HEX16(expected[0].value, result.mismatchExpected);
  TEST_ASSERT_EQUAL_HEX16(
      static_cast<uint16_t>(expected[0].value ^ cmd::MASK_CH1EN),
      result.mismatchActual);
  TEST_ASSERT_EQUAL_HEX16(static_cast<uint16_t>(~cmd::MASK_RST),
                          result.mismatchMask);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::DIRTY),
                          static_cast<uint8_t>(device.measurementConfigState()));
}

void test_owner_power_down_mismatch_retains_full_register_evidence() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();

  DeviceProfile poweredDown = profile;
  poweredDown.mode = Mode::POWER_DOWN;
  const uint16_t expected = configValue(poweredDown, Mode::POWER_DOWN);
  const uint16_t actual = static_cast<uint16_t>(expected ^ cmd::MASK_CH1EN);
  bus.expectRead(cmd::REG_CONFIG);
  bus.expectWrite(cmd::REG_CONFIG, expected);
  TEST_ASSERT_TRUE(bus.mutateAfterLastStep(cmd::REG_CONFIG, actual));
  bus.expectRead(cmd::REG_CONFIG);

  TEST_ASSERT_TRUE(device.startPowerDown(803U, 1000U).inProgress());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::PROFILE_MISMATCH),
                          static_cast<uint8_t>(device.pollJob(pollContext(1U)).code));
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobTerminalState::PARTIAL),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareEffect::PARTIAL),
                          static_cast<uint8_t>(result.hardwareEffect));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::PROFILE_MISMATCH),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_EQUAL_INT32(cmd::REG_CONFIG, result.status.detail);
  TEST_ASSERT_EQUAL_STRING("Power-down verification mismatch", result.status.msg);
  TEST_ASSERT_TRUE(result.mismatchValid);
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_CONFIG, result.mismatchRegister);
  TEST_ASSERT_EQUAL_HEX16(expected, result.mismatchExpected);
  TEST_ASSERT_EQUAL_HEX16(actual, result.mismatchActual);
  TEST_ASSERT_EQUAL_HEX16(static_cast<uint16_t>(~cmd::MASK_RST),
                          result.mismatchMask);
  TEST_ASSERT_TRUE(bus.scriptConsumed());
}

void test_owner_active_job_excludes_legacy_hardware_io_without_callbacks() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  TEST_ASSERT_TRUE(device.startReconcile(901U, 1000).inProgress());
  uint16_t reg = 0;
  int16_t raw = 0;
  AlertFlags flags{};
  auto isJobBusy = [](Status status) { return status.code == Err::JOB_BUSY; };
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.probe().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.recover().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.tickStatus(1).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.powerDown().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.readRegister16(cmd::REG_CONFIG, reg).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.writeRegister16(cmd::REG_CONFIG, reg).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.readShuntRaw(Channel::CH1, raw).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.readAlertFlags(flags).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.setMode(Mode::SHUNT_CONT).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.softReset().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                          static_cast<uint8_t>(device.startConversion().code));
  TEST_ASSERT_TRUE(isJobBusy(device.readBusRaw(Channel::CH1, raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.readShuntSumRaw(raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.readConfig(reg)));
  TEST_ASSERT_TRUE(isJobBusy(device.writeConfig(reg)));
  TEST_ASSERT_TRUE(isJobBusy(device.setAveraging(Averaging::AVG_4)));
  TEST_ASSERT_TRUE(isJobBusy(device.setVBusConvTime(ConvTime::CT_204US)));
  TEST_ASSERT_TRUE(isJobBusy(device.setVShuntConvTime(ConvTime::CT_204US)));
  TEST_ASSERT_TRUE(isJobBusy(device.setChannelEnable(Channel::CH1, false)));
  TEST_ASSERT_TRUE(isJobBusy(device.setShuntResistance(Channel::CH1, 0.2F)));
  TEST_ASSERT_TRUE(isJobBusy(device.setCriticalAlertLimit(Channel::CH1, 0)));
  TEST_ASSERT_TRUE(isJobBusy(device.getCriticalAlertLimit(Channel::CH1, raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.setWarningAlertLimit(Channel::CH1, 0)));
  TEST_ASSERT_TRUE(isJobBusy(device.getWarningAlertLimit(Channel::CH1, raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.setShuntSumLimit(0)));
  TEST_ASSERT_TRUE(isJobBusy(device.getShuntSumLimit(raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.setPowerValidUpperLimit(0)));
  TEST_ASSERT_TRUE(isJobBusy(device.getPowerValidUpperLimit(raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.setPowerValidLowerLimit(0)));
  TEST_ASSERT_TRUE(isJobBusy(device.getPowerValidLowerLimit(raw)));
  TEST_ASSERT_TRUE(isJobBusy(device.setSummationChannels(true, false, false)));
  TEST_ASSERT_TRUE(isJobBusy(device.setAlertLatchEnable(true, true)));
  TEST_ASSERT_TRUE(isJobBusy(device.startApplyMaskEnable(0)));
  TEST_ASSERT_TRUE(isJobBusy(device.readManufacturerId(reg)));
  TEST_ASSERT_TRUE(isJobBusy(device.readDieId(reg)));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
  TEST_ASSERT_FALSE(bus.hasViolation());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(device.cancelJob().code));
}

void test_legacy_facade_rejected_starts_preserve_and_cannot_cross_drive_jobs() {
  {
    const DeviceProfile profile =
        makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    bus.expectWrite(cmd::REG_CONFIG,
                    configValue(profile, Mode::SHUNT_BUS_TRIG));
    TEST_ASSERT_TRUE(device.startSingleShot(Mode::SHUNT_BUS_TRIG, false)
                         .inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_BUSY),
                            static_cast<uint8_t>(device.startSingleShot(
                                Mode::SHUNT_BUS_TRIG, false).code));
    TEST_ASSERT_EQUAL_UINT32(0U, bus.callCount());
    TEST_ASSERT_TRUE(device.pollSingleShot(0U, 1U).inProgress());
    TEST_ASSERT_EQUAL_UINT32(1U, bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    JobResult cancelled{};
    TEST_ASSERT_TRUE(device.takeJobResult(cancelled).ok());
  }

  {
    const DeviceProfile profile =
        makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_CONT);
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    bus.expectRead(cmd::REG_CH1_SHUNT);
    bus.expectRead(cmd::REG_CH1_BUS);
    TEST_ASSERT_TRUE(device.startContinuousRead(false).inProgress());
    TEST_ASSERT_TRUE(device.pollContinuousRead(1U, 2U).ok());
    TEST_ASSERT_TRUE(device.startReconcile(703U, 1000U).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::JOB_KIND_MISMATCH),
                            static_cast<uint8_t>(
                                device.pollContinuousRead(2U, 1U).code));
    TEST_ASSERT_EQUAL_UINT32(2U, bus.callCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(device.cancelJob().code));
    JobResult cancelled{};
    TEST_ASSERT_TRUE(device.takeJobResult(cancelled).ok());
  }
}

void test_legacy_sample_snapshot_retains_cvrf_before_and_after_channel_reads() {
  for (uint8_t scenario = 0; scenario < 4U; ++scenario) {
    const bool triggered = scenario >= 2U;
    const bool failChannelRead = (scenario & 1U) != 0U;
    const Mode mode = triggered ? Mode::SHUNT_BUS_TRIG : Mode::SHUNT_BUS_CONT;
    const DeviceProfile profile = makeProfile(0x42, CHANNEL_1, mode);
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    uint32_t nowMs = 1000U;
    if (triggered) {
      bus.expectWrite(cmd::REG_CONFIG, configValue(profile, mode));
      TEST_ASSERT_TRUE(device.startSingleShot().inProgress());
      TEST_ASSERT_TRUE(device.pollSingleShot(nowMs, 1U).inProgress());
      TEST_ASSERT_TRUE(device.pollSingleShot(++nowMs, 0U).inProgress());
      JobProgress progress{};
      TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
      nowMs = static_cast<uint32_t>(progress.readyAtMs);
    } else {
      TEST_ASSERT_TRUE(device.startContinuousRead(true).inProgress());
    }
    PollJobSnapshot snapshot{};
    TEST_ASSERT_TRUE(device.getPollJobSnapshot(snapshot).ok());
    TEST_ASSERT_FALSE(snapshot.conversionReady);

    bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CVRF);
    bus.expectRead(cmd::REG_MASK_ENABLE);
    TEST_ASSERT_TRUE(device.pollJob(nowMs, 1U).inProgress());
    TEST_ASSERT_TRUE(device.getPollJobSnapshot(snapshot).ok());
    TEST_ASSERT_TRUE(snapshot.active);
    TEST_ASSERT_TRUE(snapshot.conversionReady);
    TEST_ASSERT_BITS_LOW(cmd::MASK_CVRF,
                         bus.registerValue(cmd::REG_MASK_ENABLE));

    bus.expectRead(cmd::REG_CH1_SHUNT,
                   failChannelRead ? Status::Error(Err::I2C_BUS, "channel read failed")
                                   : Status::Ok());
    if (!failChannelRead) bus.expectRead(cmd::REG_CH1_BUS);
    const Status result = device.pollJob(nowMs, 2U);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(failChannelRead ? Err::I2C_BUS : Err::OK),
        static_cast<uint8_t>(result.code));
    TEST_ASSERT_TRUE(device.getPollJobSnapshot(snapshot).ok());
    TEST_ASSERT_FALSE(snapshot.active);
    TEST_ASSERT_EQUAL(!failChannelRead, snapshot.complete);
    TEST_ASSERT_TRUE(snapshot.conversionReady);
    TEST_ASSERT_TRUE(bus.scriptConsumed());
  }
}

void test_legacy_single_shot_snapshot_keeps_initial_start_during_cvrf_recheck() {
  const DeviceProfile profile = makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.expectWrite(cmd::REG_CONFIG, configValue(profile, profile.mode));
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bus.setRegister(cmd::REG_MASK_ENABLE, 0U);
  TEST_ASSERT_TRUE(device.startSingleShot().inProgress());
  TEST_ASSERT_TRUE(device.pollSingleShot(1000U, 1U).inProgress());
  TEST_ASSERT_TRUE(device.pollSingleShot(1001U, 0U).inProgress());
  JobProgress progress{};
  PollJobSnapshot snapshot{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  TEST_ASSERT_TRUE(device.getPollJobSnapshot(snapshot).ok());
  TEST_ASSERT_EQUAL_UINT32(1001U, snapshot.conversionStartMs);
  const uint32_t readyAtMs = static_cast<uint32_t>(progress.readyAtMs);
  TEST_ASSERT_TRUE(device.pollSingleShot(readyAtMs, 1U).inProgress());
  const size_t callsBeforeRecheck = bus.callCount();
  TEST_ASSERT_TRUE(device.pollSingleShot(readyAtMs + 1U, 0U).inProgress());
  TEST_ASSERT_TRUE(device.getPollJobSnapshot(snapshot).ok());
  TEST_ASSERT_EQUAL_UINT32(1001U, snapshot.conversionStartMs);
  TEST_ASSERT_FALSE(snapshot.conversionReady);
  TEST_ASSERT_EQUAL_UINT32(callsBeforeRecheck, bus.callCount());
  TEST_ASSERT_TRUE(bus.scriptConsumed());
}

void test_raw_reset_updates_both_profile_certainty_families() {
  const DeviceProfile profile = makeProfile();
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    bus.expectWrite(cmd::REG_CONFIG, cmd::MASK_RST);
    TEST_ASSERT_TRUE(device.writeRegister16(cmd::REG_CONFIG,
                                            cmd::MASK_RST).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::DIRTY),
                            static_cast<uint8_t>(device.measurementConfigState()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::DIRTY),
                            static_cast<uint8_t>(device.alertConfigState()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_BUS_CONT),
                            static_cast<uint8_t>(device.getMode()));
  }
  {
    ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
    INA3221::INA3221 device;
    TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
    bus.resetHarness();
    const Status committedTimeout =
        Status::Error(Err::I2C_TIMEOUT, "reset committed before timeout", -79);
    bus.expectWrite(cmd::REG_CONFIG, cmd::MASK_RST, committedTimeout,
                    ScriptedTransport::WriteEffect::COMMIT_BEFORE_STATUS);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(device.writeRegister16(
                                cmd::REG_CONFIG, cmd::MASK_RST).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::UNKNOWN),
                            static_cast<uint8_t>(device.measurementConfigState()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AppliedConfigState::UNKNOWN),
                            static_cast<uint8_t>(device.alertConfigState()));
    TEST_ASSERT_EQUAL_UINT32(1U, bus.callCount());
  }
}

void test_owner_health_diagnostics_never_suppress_owner_requested_io() {
  DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  TransportConfig transport = bus.makeTransportConfig();
  transport.offlineThreshold = 1U;
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(device.bind(transport, profile).ok());
  TEST_ASSERT_TRUE(queueProfileSequence(bus, profile, true));
  TEST_ASSERT_TRUE(device.startInitialize(1U, 1000U).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(1U)).ok());
  JobResult initialized{};
  TEST_ASSERT_TRUE(device.takeJobResult(initialized).ok());
  bus.resetHarness();
  bus.expectRead(cmd::REG_CONFIG,
                 Status::Error(Err::I2C_TIMEOUT, "forced offline", -73));
  uint16_t value = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(device.readRegister16(
                              cmd::REG_CONFIG, value).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(device.state()));
  bus.expectRead(cmd::REG_CONFIG);
  TEST_ASSERT_TRUE(device.readRegister16(cmd::REG_CONFIG, value).ok());
  TEST_ASSERT_EQUAL_UINT32(2U, bus.callCount());
  TEST_ASSERT_TRUE(bus.scriptConsumed());
}

void test_owner_alert_events_are_retained_and_taken_exactly_by_owner() {
  const DeviceProfile profile = makeProfile();
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();

  const uint16_t first = static_cast<uint16_t>(
      cmd::MASK_CF1 | cmd::MASK_WF2 | cmd::MASK_CVRF | cmd::MASK_TCF);
  bus.setRegister(cmd::REG_MASK_ENABLE, first);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  uint16_t raw = 0;
  TEST_ASSERT_TRUE(device.readRegister16(cmd::REG_MASK_ENABLE, raw).ok());
  TEST_ASSERT_EQUAL_HEX16(first, raw);

  const uint16_t second = static_cast<uint16_t>(cmd::MASK_CF3 | cmd::MASK_SF |
                                                cmd::MASK_PVF | cmd::MASK_TCF);
  bus.setRegister(cmd::REG_MASK_ENABLE, second);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  AlertFlags flags{};
  TEST_ASSERT_TRUE(device.readAlertFlags(flags).ok());
  TEST_ASSERT_TRUE(flags.criticalCh3);
  TEST_ASSERT_TRUE(flags.summation);

  const uint16_t third = static_cast<uint16_t>(cmd::MASK_WF3 | cmd::MASK_CVRF);
  bus.setRegister(cmd::REG_MASK_ENABLE, third);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bool ready = false;
  TEST_ASSERT_TRUE(device.readConversionReady(ready).ok());
  TEST_ASSERT_TRUE(ready);

  AlertSnapshot snapshot{};
  TEST_ASSERT_TRUE(device.peekAlertEvents(snapshot).ok());
  TEST_ASSERT_EQUAL_HEX16(static_cast<uint16_t>(
      cmd::MASK_CF1 | cmd::MASK_WF2 | cmd::MASK_CVRF | cmd::MASK_CF3 |
      cmd::MASK_SF | cmd::MASK_WF3),
      snapshot.events);
  TEST_ASSERT_FALSE(snapshot.timingControl);
  TEST_ASSERT_TRUE(snapshot.timingControlFault);
  TEST_ASSERT_FALSE(snapshot.powerValid);
  TEST_ASSERT_TRUE(device.takeAlertEvents(snapshot).ok());
  AlertSnapshot after{};
  TEST_ASSERT_TRUE(device.peekAlertEvents(after).ok());
  TEST_ASSERT_EQUAL_HEX16(0U, after.events);
  TEST_ASSERT_FALSE(after.timingControl);
  TEST_ASSERT_TRUE(after.timingControlFault);
}

void test_owner_triggered_sample_is_atomic_and_preserves_last_good_on_failures() {
  const DeviceProfile profile =
      makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.setRegister(cmd::REG_CH1_SHUNT, 0x0190U);
  bus.setRegister(cmd::REG_CH1_BUS, 0x1770U);
  const uint16_t events = static_cast<uint16_t>(cmd::MASK_CVRF | cmd::MASK_CF1);
  TEST_ASSERT_TRUE(queueTriggeredSample(bus, profile, Mode::SHUNT_BUS_TRIG, events));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::IN_PROGRESS),
                          static_cast<uint8_t>(device.startTriggeredSample(
                              Mode::SHUNT_BUS_TRIG, 1001U, 1000).code));
  TEST_ASSERT_TRUE(runTriggeredToTerminal(device));
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_TRUE(result.sampleValid);
  TEST_ASSERT_EQUAL_UINT32(1001U, result.sample.requestId);
  TEST_ASSERT_EQUAL_INT32(2000, result.sample.channels[0].shuntMicroVolts);
  TEST_ASSERT_EQUAL_INT32(6000, result.sample.channels[0].busMilliVolts);
  TEST_ASSERT_EQUAL_INT32(20, result.sample.channels[0].currentMilliAmps);
  TEST_ASSERT_EQUAL_INT32(120, result.sample.channels[0].powerMilliWatts);
  TEST_ASSERT_BITS_HIGH(cmd::MASK_CF1, result.sample.alerts.events);
  TEST_ASSERT_TRUE(result.sample.alertSnapshotValid);

  for (uint8_t failureStage = 0; failureStage < 3U; ++failureStage) {
    bus.resetHarness();
    bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CVRF);
    bus.expectWrite(cmd::REG_CONFIG, configValue(profile, Mode::SHUNT_BUS_TRIG));
    const Status failure = Status::Error(Err::I2C_BUS, "mid-sample failure", -74);
    bus.expectRead(cmd::REG_MASK_ENABLE,
                   failureStage == 0U ? failure : Status::Ok());
    if (failureStage > 0U) {
      bus.expectRead(cmd::REG_CH1_SHUNT,
                     failureStage == 1U ? failure : Status::Ok());
    }
    if (failureStage > 1U) bus.expectRead(cmd::REG_CH1_BUS, failure);
    TEST_ASSERT_TRUE(device.startTriggeredSample(
        Mode::SHUNT_BUS_TRIG, 1010U + failureStage, 1000).inProgress());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(100U, 1000, DEFAULT_TIMEOUT_MS, 1U))
                         .inProgress());
    TEST_ASSERT_TRUE(device.pollJob(pollContext(101U, 1000,
                                                DEFAULT_TIMEOUT_MS, 0U))
                         .inProgress());
    JobProgress progress{};
    TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                            static_cast<uint8_t>(device.pollJob(
                                pollContext(progress.readyAtMs)).code));
    TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
    TEST_ASSERT_FALSE(result.sampleValid);
    SampleBatch last{};
    TEST_ASSERT_TRUE(device.peekLastSample(last).ok());
    TEST_ASSERT_EQUAL_UINT32(1001U, last.requestId);
    TEST_ASSERT_EQUAL_INT32(120, last.channels[0].powerMilliWatts);
  }
}

void test_owner_continuous_sample_reports_mixed_age_after_register_mutation() {
  const DeviceProfile profile = makeProfile(0x42, CHANNEL_1);
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.setRegister(cmd::REG_CH1_SHUNT, 0x0190U);
  bus.setRegister(cmd::REG_CH1_BUS, 0x1000U);
  bus.expectRead(cmd::REG_CH1_SHUNT);
  TEST_ASSERT_TRUE(bus.mutateAfterLastStep(cmd::REG_CH1_BUS, 0x1770U));
  bus.expectRead(cmd::REG_CH1_BUS);
  TEST_ASSERT_TRUE(device.startContinuousSample(1101U, 1000).inProgress());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(77U)).ok());
  JobResult result{};
  TEST_ASSERT_TRUE(device.takeJobResult(result).ok());
  TEST_ASSERT_TRUE(result.sampleValid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SampleCoherence::CONTINUOUS_MIXED_AGE),
                          static_cast<uint8_t>(result.sample.coherence));
  TEST_ASSERT_EQUAL_UINT64(77U, result.sample.captureUptimeMs);
  TEST_ASSERT_EQUAL_INT32(6000, result.sample.channels[0].busMilliVolts);
  TEST_ASSERT_FALSE(result.sample.alertSnapshotValid);
}

void test_owner_timing_tables_and_cycle_cartesian_are_exact() {
  static constexpr uint32_t typical[8] =
      {140, 204, 332, 588, 1100, 2116, 4156, 8244};
  static constexpr uint32_t maximum[8] =
      {154, 224, 365, 646, 1210, 2328, 4572, 9068};
  static constexpr uint32_t samples[8] =
      {1, 4, 16, 64, 128, 256, 512, 1024};
  for (uint8_t shunt = 0; shunt < 8U; ++shunt) {
    ConversionTiming timing{};
    TEST_ASSERT_TRUE(INA3221::INA3221::conversionTiming(
        static_cast<ConvTime>(shunt), timing).ok());
    TEST_ASSERT_EQUAL_UINT32(typical[shunt], timing.typicalUs);
    TEST_ASSERT_EQUAL_UINT32(maximum[shunt], timing.maximumUs);
    for (uint8_t busTime = 0; busTime < 8U; ++busTime) {
      for (uint8_t averaging = 0; averaging < 8U; ++averaging) {
        for (uint8_t channels = 1; channels <= 3U; ++channels) {
          DeviceProfile profile = makeProfile(
              0x40, static_cast<ChannelMask>((1U << channels) - 1U));
          profile.vShCt = static_cast<ConvTime>(shunt);
          profile.vBusCt = static_cast<ConvTime>(busTime);
          profile.averaging = static_cast<Averaging>(averaging);
          uint64_t cycle = 0;
          TEST_ASSERT_TRUE(INA3221::INA3221::maximumCycleTimeUs(
              profile, Mode::SHUNT_BUS_TRIG, cycle).ok());
          const uint64_t expected =
              static_cast<uint64_t>(maximum[shunt] + maximum[busTime]) *
              channels * samples[averaging];
          TEST_ASSERT_EQUAL_UINT64(expected, cycle);
        }
      }
    }
  }
  ConversionTiming invalid{};
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::conversionTiming(
                              static_cast<ConvTime>(8), invalid).code));
}

void test_owner_wait_uses_maximum_timing_and_crosses_uint32_boundary_without_io() {
  DeviceProfile profile = makeProfile(0x42, CHANNEL_1, Mode::SHUNT_BUS_TRIG);
  profile.vBusCt = ConvTime::CT_8244US;
  profile.vShCt = ConvTime::CT_8244US;
  ScriptedTransport bus(0x42, DEFAULT_TIMEOUT_MS);
  INA3221::INA3221 device;
  TEST_ASSERT_TRUE(initializeApplied(device, bus, profile));
  bus.resetHarness();
  bus.expectWrite(cmd::REG_CONFIG, configValue(profile, Mode::SHUNT_BUS_TRIG));
  bus.setRegister(cmd::REG_MASK_ENABLE, cmd::MASK_CVRF);
  bus.expectRead(cmd::REG_MASK_ENABLE);
  bus.expectRead(cmd::REG_CH1_SHUNT);
  bus.expectRead(cmd::REG_CH1_BUS);
  const uint64_t start = static_cast<uint64_t>(UINT32_MAX) - 2U;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::IN_PROGRESS),
                          static_cast<uint8_t>(device.startTriggeredSample(
                              Mode::SHUNT_BUS_TRIG, 1201U, start + 100U).code));
  TEST_ASSERT_TRUE(device.pollJob(pollContext(start, start + 100U,
                                              DEFAULT_TIMEOUT_MS, 1U)).inProgress());
  JobProgress origin{};
  TEST_ASSERT_TRUE(device.getJobProgress(origin).ok());
  TEST_ASSERT_EQUAL_UINT64(0U, origin.readyAtMs);
  TEST_ASSERT_TRUE(device.pollJob(pollContext(start + 1U, start + 100U,
                                              DEFAULT_TIMEOUT_MS, 0U)).inProgress());
  JobProgress progress{};
  TEST_ASSERT_TRUE(device.getJobProgress(progress).ok());
  const uint32_t callsAfterTrigger = static_cast<uint32_t>(bus.callCount());
  TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs - 1U,
                                              start + 100U)).inProgress());
  TEST_ASSERT_EQUAL_UINT32(callsAfterTrigger, bus.callCount());
  TEST_ASSERT_TRUE(progress.readyAtMs > UINT32_MAX);
  TEST_ASSERT_TRUE(device.pollJob(pollContext(progress.readyAtMs,
                                              start + 100U,
                                              DEFAULT_TIMEOUT_MS, 3U)).ok());
}

void test_owner_fixed_unit_helpers_cover_boundaries_reverse_and_overflow() {
  int32_t value = 0;
  TEST_ASSERT_TRUE(INA3221::INA3221::decodeShuntMicroVolts(0xFFF8U, value).ok());
  TEST_ASSERT_EQUAL_INT32(-40, value);
  TEST_ASSERT_TRUE(INA3221::INA3221::decodeShuntMicroVolts(0x8000U, value).ok());
  TEST_ASSERT_EQUAL_INT32(-163840, value);
  TEST_ASSERT_TRUE(INA3221::INA3221::decodeShuntMicroVolts(0x7FF8U, value).ok());
  TEST_ASSERT_EQUAL_INT32(163800, value);
  TEST_ASSERT_TRUE(INA3221::INA3221::decodeBusMilliVolts(0xFFF8U, value).ok());
  TEST_ASSERT_EQUAL_INT32(-8, value);

  uint16_t raw = 0;
  TEST_ASSERT_TRUE(INA3221::INA3221::encodeShuntMicroVolts(20, raw).ok());
  TEST_ASSERT_EQUAL_HEX16(0x0008U, raw);
  TEST_ASSERT_TRUE(INA3221::INA3221::encodeShuntMicroVolts(-20, raw).ok());
  TEST_ASSERT_EQUAL_HEX16(0xFFF8U, raw);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OUT_OF_RANGE),
                          static_cast<uint8_t>(INA3221::INA3221::encodeShuntMicroVolts(
                              163840, raw).code));
  TEST_ASSERT_TRUE(INA3221::INA3221::encodeBusMilliVolts(26000, raw).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OUT_OF_RANGE),
                          static_cast<uint8_t>(INA3221::INA3221::validatePowerValidWindow(
                              10000, 10000).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OUT_OF_RANGE),
                          static_cast<uint8_t>(INA3221::INA3221::validatePowerValidWindow(
                              9000, 26001).code));

  ShuntCalibration calibration{};
  calibration.resistanceMicroOhms = 100000U;
  TEST_ASSERT_TRUE(INA3221::INA3221::calculateCurrentMilliAmps(
      2000, calibration, value).ok());
  TEST_ASSERT_EQUAL_INT32(20, value);
  calibration.direction = CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT;
  TEST_ASSERT_TRUE(INA3221::INA3221::calculateCurrentMilliAmps(
      2000, calibration, value).ok());
  TEST_ASSERT_EQUAL_INT32(-20, value);
  calibration.resistanceMicroOhms = 0U;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(INA3221::INA3221::calculateCurrentMilliAmps(
                              1, calibration, value).code));
  calibration.resistanceMicroOhms = 1U;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::ARITHMETIC_OVERFLOW),
                          static_cast<uint8_t>(INA3221::INA3221::calculateCurrentMilliAmps(
                              INT32_MAX, calibration, value).code));
  TEST_ASSERT_TRUE(INA3221::INA3221::calculatePowerMilliWatts(6000, -20, value).ok());
  TEST_ASSERT_EQUAL_INT32(-120, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::ARITHMETIC_OVERFLOW),
                          static_cast<uint8_t>(INA3221::INA3221::calculatePowerMilliWatts(
                              INT32_MAX, INT32_MAX, value).code));
}

void test_owner_channel_helpers_reject_invalid_enum_and_mask() {
  uint8_t index = 0xFFU;
  ChannelMask bit = 0U;
  bool present = false;
  uint8_t count = 0U;

  TEST_ASSERT_TRUE(INA3221::INA3221::channelIndex(Channel::CH1, index).ok());
  TEST_ASSERT_EQUAL_UINT8(0U, index);
  TEST_ASSERT_TRUE(INA3221::INA3221::channelIndex(Channel::CH3, index).ok());
  TEST_ASSERT_EQUAL_UINT8(2U, index);
  TEST_ASSERT_TRUE(INA3221::INA3221::channelBit(Channel::CH2, bit).ok());
  TEST_ASSERT_EQUAL_HEX8(CHANNEL_2, bit);
  TEST_ASSERT_TRUE(INA3221::INA3221::contains(
      static_cast<ChannelMask>(CHANNEL_1 | CHANNEL_3), Channel::CH3, present).ok());
  TEST_ASSERT_TRUE(present);
  TEST_ASSERT_TRUE(INA3221::INA3221::enabledChannelCount(ALL_CHANNELS, count).ok());
  TEST_ASSERT_EQUAL_UINT8(3U, count);

  const Channel invalidChannel = static_cast<Channel>(3U);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::channelIndex(
                              invalidChannel, index).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::channelBit(
                              invalidChannel, bit).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::contains(
                              ALL_CHANNELS, invalidChannel, present).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::contains(
                              static_cast<ChannelMask>(ALL_CHANNELS | 0x80U),
                              Channel::CH1, present).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::enabledChannelCount(
                              static_cast<ChannelMask>(ALL_CHANNELS | 0x08U), count).code));
}

void test_owner_convert_raw_channel_tracks_quantity_validity() {
  const QuantityMask shunt = static_cast<QuantityMask>(Quantity::SHUNT);
  const QuantityMask bus = static_cast<QuantityMask>(Quantity::BUS);
  const QuantityMask current = static_cast<QuantityMask>(Quantity::CURRENT);
  const QuantityMask power = static_cast<QuantityMask>(Quantity::POWER);
  ShuntCalibration noCalibration{};
  FixedChannelReading reading{};

  TEST_ASSERT_TRUE(INA3221::INA3221::convertRawChannel(
      0xFFFFU, 0x1770U, bus, noCalibration, reading).ok());
  TEST_ASSERT_EQUAL_INT32(6000, reading.busMilliVolts);
  TEST_ASSERT_EQUAL_HEX8(bus, reading.validQuantities);

  TEST_ASSERT_TRUE(INA3221::INA3221::convertRawChannel(
      0U, 0xFFF8U, bus, noCalibration, reading).ok());
  TEST_ASSERT_EQUAL_INT32(-8, reading.busMilliVolts);
  TEST_ASSERT_EQUAL_HEX8(0U, reading.validQuantities);

  TEST_ASSERT_TRUE(INA3221::INA3221::convertRawChannel(
      0U, 0x6598U, bus, noCalibration, reading).ok());
  TEST_ASSERT_EQUAL_INT32(26008, reading.busMilliVolts);
  TEST_ASSERT_EQUAL_HEX8(0U, reading.validQuantities);

  ShuntCalibration calibration{};
  calibration.resistanceMicroOhms = 100000U;
  TEST_ASSERT_TRUE(INA3221::INA3221::convertRawChannel(
      0x0190U, 0x1770U, static_cast<QuantityMask>(shunt | bus),
      calibration, reading).ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<QuantityMask>(shunt | bus | current | power),
                         reading.validQuantities);
  TEST_ASSERT_EQUAL_INT32(20, reading.currentMilliAmps);
  TEST_ASSERT_EQUAL_INT32(120, reading.powerMilliWatts);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(INA3221::INA3221::convertRawChannel(
                              0U, 0U, current, calibration, reading).code));
  TEST_ASSERT_EQUAL_HEX8(0U, reading.validQuantities);
}

void test_owner_register_access_classification_is_exact() {
  for (uint16_t reg = 0; reg <= 0xFFU; ++reg) {
    const bool readable = reg <= cmd::REG_PV_LOWER_LIMIT ||
                          reg == cmd::REG_MANUFACTURER_ID || reg == cmd::REG_DIE_ID;
    const bool writable = reg == cmd::REG_CONFIG ||
                          (reg >= cmd::REG_CH1_CRIT_LIMIT &&
                           reg <= cmd::REG_CH3_WARN_LIMIT) ||
                          reg == cmd::REG_SHUNT_SUM_LIMIT ||
                          reg == cmd::REG_MASK_ENABLE ||
                          reg == cmd::REG_PV_UPPER_LIMIT ||
                          reg == cmd::REG_PV_LOWER_LIMIT;
    TEST_ASSERT_EQUAL(readable,
                      INA3221::INA3221::isReadableRegister(static_cast<uint8_t>(reg)));
    TEST_ASSERT_EQUAL(writable,
                      INA3221::INA3221::isWritableRegister(static_cast<uint8_t>(reg)));
  }
}

void test_scripted_transport_rejects_wrong_address_timeout_and_shape() {
  ScriptedTransport bus(0x42, 7U);
  uint8_t readReg = cmd::REG_CONFIG;
  uint8_t rx[2] = {};
  bus.expectRead(cmd::REG_CONFIG);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(ScriptedTransport::writeReadCallback(
                              0x41, &readReg, 1, rx, 2, 7, &bus).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ScriptedTransport::Violation::WRONG_ADDRESS),
      static_cast<uint8_t>(bus.violation()));

  bus.resetHarness();
  bus.expectRead(cmd::REG_CONFIG);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(ScriptedTransport::writeReadCallback(
                              0x42, &readReg, 1, rx, 2, 8, &bus).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ScriptedTransport::Violation::WRONG_TIMEOUT),
      static_cast<uint8_t>(bus.violation()));

  bus.resetHarness();
  bus.expectRead(cmd::REG_CONFIG);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(ScriptedTransport::writeReadCallback(
                              0x42, &readReg, 2, rx, 2, 7, &bus).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ScriptedTransport::Violation::WRONG_READ_TX_LENGTH),
      static_cast<uint8_t>(bus.violation()));

  bus.resetHarness();
  bus.expectWrite(cmd::REG_CONFIG, 0x1234U);
  uint8_t tx[3] = {cmd::REG_CONFIG, 0x12, 0x35};
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(ScriptedTransport::writeCallback(
                              0x42, tx, 3, 7, &bus).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ScriptedTransport::Violation::WRONG_WRITE_VALUE),
      static_cast<uint8_t>(bus.violation()));
}

void test_owner_compile_time_contracts() {
  static_assert(!std::is_copy_constructible<INA3221::INA3221>::value,
                "driver must not be copied");
  static_assert(!std::is_copy_assignable<INA3221::INA3221>::value,
                "driver must not be copy-assigned");
  static_assert(!std::is_move_constructible<INA3221::INA3221>::value,
                "driver must not be moved");
  static_assert(!std::is_move_assignable<INA3221::INA3221>::value,
                "driver must not be move-assigned");
  static_assert(std::is_trivially_copyable<SampleBatch>::value,
                "sample is fixed-record copyable");
  static_assert(std::is_trivially_copyable<JobResult>::value,
                "result is fixed-record copyable");
  static_assert(std::is_trivially_copyable<JobProgress>::value,
                "progress is fixed-record copyable");
  static_assert(std::is_standard_layout<SampleBatch>::value,
                "sample has portable layout");
  static_assert(sizeof(FixedChannelReading) <= 20U, "channel record grew unexpectedly");
  static_assert(sizeof(AlertSnapshot) <= 16U, "alert record grew unexpectedly");
  static_assert(sizeof(SampleBatch) <= 128U, "sample record grew unexpectedly");
  static_assert(sizeof(JobResult) <= 192U, "job result grew unexpectedly");
  static_assert(sizeof(JobProgress) <= 64U, "job progress grew unexpectedly");
  TEST_PASS();
}

}  // namespace

void runOwnerOperationTests() {
  RUN_TEST(test_owner_bind_unbind_and_invalid_rebind_are_bus_silent);
  RUN_TEST(test_owner_bind_requires_explicit_calibration_only_for_enabled_channels);
  RUN_TEST(test_compatibility_profile_setters_reject_invalid_candidates_bus_silent);
  RUN_TEST(test_owner_trigger_admission_and_post_callback_time_origin_are_safe);
  RUN_TEST(test_owner_initialize_full_profile_exact_sequence_and_verification);
  RUN_TEST(test_owner_initialize_failure_at_every_transfer_stage);
  RUN_TEST(test_owner_apply_profile_failure_at_every_transfer_stage);
  RUN_TEST(test_owner_budget_zero_one_and_many_are_exact);
  RUN_TEST(test_owner_timeout_clamps_to_remaining_deadline_and_exact_deadline_expires);
  RUN_TEST(test_owner_interior_deadline_is_bus_silent_and_terminalizes_timed_out);
  RUN_TEST(test_owner_triggered_interior_deadline_is_partial_but_keeps_config_applied);
  RUN_TEST(test_owner_cancel_is_bus_silent_before_and_after_hardware_effects);
  RUN_TEST(test_owner_cancel_is_bus_silent_across_profile_transfer_stages);
  RUN_TEST(test_owner_permanently_low_cvrf_waits_until_owner_deadline);
  RUN_TEST(test_owner_triggered_sample_retains_alerts_across_low_cvrf_recheck);
  RUN_TEST(test_owner_result_identity_pending_take_once_and_stale_prevention);
  RUN_TEST(test_owner_commit_before_timeout_is_indeterminate_and_not_retried);
  RUN_TEST(test_owner_every_managed_write_ambiguity_is_indeterminate_without_retry);
  RUN_TEST(test_owner_every_managed_readback_mismatch_is_partial_and_dirty);
  RUN_TEST(test_failed_partial_mask_read_exposes_alert_evidence_uncertainty);
  RUN_TEST(test_owner_reconcile_reads_first_and_verification_mismatch_is_partial);
  RUN_TEST(test_owner_power_down_mismatch_retains_full_register_evidence);
  RUN_TEST(test_owner_active_job_excludes_legacy_hardware_io_without_callbacks);
  RUN_TEST(test_legacy_facade_rejected_starts_preserve_and_cannot_cross_drive_jobs);
  RUN_TEST(test_legacy_sample_snapshot_retains_cvrf_before_and_after_channel_reads);
  RUN_TEST(test_legacy_single_shot_snapshot_keeps_initial_start_during_cvrf_recheck);
  RUN_TEST(test_raw_reset_updates_both_profile_certainty_families);
  RUN_TEST(test_owner_health_diagnostics_never_suppress_owner_requested_io);
  RUN_TEST(test_owner_alert_events_are_retained_and_taken_exactly_by_owner);
  RUN_TEST(test_owner_triggered_sample_is_atomic_and_preserves_last_good_on_failures);
  RUN_TEST(test_owner_continuous_sample_reports_mixed_age_after_register_mutation);
  RUN_TEST(test_owner_timing_tables_and_cycle_cartesian_are_exact);
  RUN_TEST(test_owner_wait_uses_maximum_timing_and_crosses_uint32_boundary_without_io);
  RUN_TEST(test_owner_fixed_unit_helpers_cover_boundaries_reverse_and_overflow);
  RUN_TEST(test_owner_channel_helpers_reject_invalid_enum_and_mask);
  RUN_TEST(test_owner_convert_raw_channel_tracks_quantity_validity);
  RUN_TEST(test_owner_register_access_classification_is_exact);
  RUN_TEST(test_scripted_transport_rejects_wrong_address_timeout_and_shape);
  RUN_TEST(test_owner_compile_time_contracts);
}
