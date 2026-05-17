/// @file test_basic.cpp
/// @brief Native contract tests for INA3221 lifecycle and health behavior.

#include <unity.h>

#include "Arduino.h"
#include "Wire.h"

#include <limits>

SerialClass Serial;
TwoWire Wire;

#define private public
#include "INA3221/INA3221.h"
#undef private

using namespace INA3221;

namespace {

struct FakeBus {
  Status writeStatus = Status::Ok();
  Status readStatus = Status::Ok();
  uint32_t nowMs = 1234;
  uint32_t writeCalls = 0;
  uint32_t readCalls = 0;
  uint32_t yieldCalls = 0;
  uint8_t lastWriteReg = 0;
  uint16_t lastWriteValue = 0;

  // Register values returned on read (keyed by register address)
  // Default: manufacturer ID, die ID, config register
  uint8_t registerData[256][2] = {};

  FakeBus() {
    // Manufacturer ID (0xFE) = 0x5449
    registerData[0xFE][0] = 0x54;
    registerData[0xFE][1] = 0x49;
    // Die ID (0xFF) = 0x3220
    registerData[0xFF][0] = 0x32;
    registerData[0xFF][1] = 0x20;
    // Config register (0x00) = 0x7127 (POR default)
    registerData[0x00][0] = 0x71;
    registerData[0x00][1] = 0x27;
    // Mask/Enable with CVRF set = 0x0001
    registerData[0x0F][0] = 0x00;
    registerData[0x0F][1] = 0x01;
  }
};

Status fakeWrite(uint8_t, const uint8_t* data, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  if (!bus->writeStatus.ok()) {
    return bus->writeStatus;
  }
  if (data != nullptr && len >= 3) {
    bus->lastWriteReg = data[0];
    bus->lastWriteValue = (static_cast<uint16_t>(data[1]) << 8) | data[2];
    bus->registerData[data[0]][0] = data[1];
    bus->registerData[data[0]][1] = data[2];
  }
  return Status::Ok();
}

Status fakeWriteRead(uint8_t, const uint8_t* txData, size_t txLen, uint8_t* rxData,
                     size_t rxLen, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->readCalls++;
  if (!bus->readStatus.ok()) {
    return bus->readStatus;
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C buffers");
  }
  // Return register-specific data based on register address
  uint8_t reg = txData[0];
  if (rxLen >= 1) {
    rxData[0] = bus->registerData[reg][0];
  }
  if (rxLen >= 2) {
    rxData[1] = bus->registerData[reg][1];
  }
  for (size_t i = 2; i < rxLen; ++i) {
    rxData[i] = 0;
  }
  return Status::Ok();
}

uint32_t fakeNowMs(void* user) {
  return static_cast<FakeBus*>(user)->nowMs;
}

void fakeYield(void* user) {
  static_cast<FakeBus*>(user)->yieldCalls++;
}

Config makeConfig(FakeBus& bus) {
  Config cfg;
  cfg.i2cWrite = fakeWrite;
  cfg.i2cWriteRead = fakeWriteRead;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
  cfg.cooperativeYield = fakeYield;
  cfg.timeUser = &bus;
  cfg.offlineThreshold = 3;
  cfg.i2cTimeoutMs = 10;
  return cfg;
}

}  // namespace

void setUp() {}
void tearDown() {}

// ============================================================================
// Status Tests
// ============================================================================

void test_status_ok() {
  Status st = Status::Ok();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OK), static_cast<uint8_t>(st.code));
}

void test_status_error() {
  Status st = Status::Error(Err::I2C_ERROR, "Test error", 42);
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(42, st.detail);
}

void test_status_in_progress() {
  Status st{Err::IN_PROGRESS, 0, "In progress"};
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_TRUE(st.inProgress());
}

// ============================================================================
// Config Defaults Tests
// ============================================================================

void test_config_defaults() {
  Config cfg;
  TEST_ASSERT_NULL(cfg.i2cWrite);
  TEST_ASSERT_NULL(cfg.i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x40, cfg.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(50, cfg.i2cTimeoutMs);
  TEST_ASSERT_TRUE(cfg.ch1Enable);
  TEST_ASSERT_TRUE(cfg.ch2Enable);
  TEST_ASSERT_TRUE(cfg.ch3Enable);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Averaging::AVG_1), static_cast<uint8_t>(cfg.averaging));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConvTime::CT_1100US), static_cast<uint8_t>(cfg.vBusCt));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConvTime::CT_1100US), static_cast<uint8_t>(cfg.vShCt));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_BUS_CONT), static_cast<uint8_t>(cfg.mode));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, cfg.shuntResistance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, cfg.shuntResistance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, cfg.shuntResistance[2]);
  TEST_ASSERT_EQUAL_UINT8(5, cfg.offlineThreshold);
}

void test_get_settings_snapshot() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x43;
  cfg.ch2Enable = false;
  cfg.averaging = Averaging::AVG_64;
  cfg.vBusCt = ConvTime::CT_204US;
  cfg.vShCt = ConvTime::CT_4156US;
  cfg.mode = Mode::SHUNT_BUS_TRIG;
  cfg.shuntResistance[0] = 0.05f;
  cfg.shuntResistance[1] = 0.10f;
  cfg.shuntResistance[2] = 0.20f;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  SettingsSnapshot snap;
  Status st = dev.getSettings(snap);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(snap.state));
  TEST_ASSERT_EQUAL_HEX8(0x43, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(10u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_TRUE(snap.hasCooperativeYieldHook);
  TEST_ASSERT_TRUE(snap.ch1Enable);
  TEST_ASSERT_FALSE(snap.ch2Enable);
  TEST_ASSERT_TRUE(snap.ch3Enable);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Averaging::AVG_64),
                          static_cast<uint8_t>(snap.averaging));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConvTime::CT_204US),
                          static_cast<uint8_t>(snap.vBusCt));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConvTime::CT_4156US),
                          static_cast<uint8_t>(snap.vShCt));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_BUS_TRIG),
                          static_cast<uint8_t>(snap.mode));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, snap.shuntResistance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.10f, snap.shuntResistance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.20f, snap.shuntResistance[2]);
  TEST_ASSERT_TRUE(snap.conversionStarted);
  TEST_ASSERT_FALSE(snap.conversionReady);
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, snap.conversionStartMs);
  TEST_ASSERT_EQUAL_HEX16(0u, snap.maskEnableWritableCache);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

void test_begin_rejects_missing_callbacks() {
  INA3221::INA3221 dev;
  Config cfg;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_begin_rejects_invalid_address() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x50;  // invalid for INA3221
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
}

void test_begin_rejects_zero_shunt_resistance() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.shuntResistance[1] = 0.0f;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
}

void test_begin_rejects_nan_shunt_resistance() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.shuntResistance[0] = std::numeric_limits<float>::quiet_NaN();
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
}

void test_begin_rejects_active_mode_with_all_channels_disabled() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.ch1Enable = false;
  cfg.ch2Enable = false;
  cfg.ch3Enable = false;
  cfg.mode = Mode::SHUNT_BUS_CONT;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
}

void test_invalid_begin_resets_runtime_and_default_config() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config good = makeConfig(bus);
  good.i2cAddress = 0x43;
  TEST_ASSERT_TRUE(dev.begin(good).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced recover timeout", -9);
  (void)dev.recover();
  TEST_ASSERT_GREATER_THAN_UINT32(0u, dev.totalFailures());

  Config bad = makeConfig(bus);
  bad.i2cTimeoutMs = 0;
  Status st = dev.begin(bad);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_NULL(dev.getConfig().i2cWrite);
  TEST_ASSERT_NULL(dev.getConfig().i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x40, dev.getConfig().i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(5u, dev.getConfig().offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastErrorMs());
}

void test_failed_begin_probe_resets_cached_config() {
  FakeBus bus;
  bus.registerData[0xFE][0] = 0x00;
  bus.registerData[0xFE][1] = 0x00;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x43;
  cfg.mode = Mode::SHUNT_BUS_TRIG;
  cfg.offlineThreshold = 1;

  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::MANUFACTURER_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_NULL(dev.getConfig().i2cWrite);
  TEST_ASSERT_NULL(dev.getConfig().i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x40, dev.getConfig().i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_BUS_CONT),
                          static_cast<uint8_t>(dev.getConfig().mode));
  TEST_ASSERT_EQUAL_UINT8(5u, dev.getConfig().offlineThreshold);
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
}

void test_begin_normalizes_offline_threshold_on_stored_copy() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 0;

  Status st = dev.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, cfg.offlineThreshold);
  TEST_ASSERT_EQUAL_UINT8(1u, dev.getConfig().offlineThreshold);
}

void test_begin_success_sets_ready_and_counters() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.isOnline());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
}

// ============================================================================
// Probe / Recover Tests
// ============================================================================

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = dev.totalSuccess();
  const uint32_t beforeFailures = dev.totalFailures();
  const uint8_t beforeConsecutive = dev.consecutiveFailures();
  const DriverState beforeState = dev.state();

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced probe timeout", -7);
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(beforeConsecutive, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeState),
                          static_cast<uint8_t>(dev.state()));
}

void test_probe_detects_wrong_manufacturer_id() {
  FakeBus bus;
  bus.registerData[0xFE][0] = 0x00;  // wrong manufacturer ID
  bus.registerData[0xFE][1] = 0x00;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::MANUFACTURER_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
}

void test_probe_detects_wrong_die_id() {
  FakeBus bus;
  bus.registerData[0xFF][0] = 0x00;  // wrong die ID
  bus.registerData[0xFF][1] = 0x00;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DIE_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
}

void test_recover_failure_updates_health() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced recover timeout", -9);
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastErrorMs());
}

void test_recover_wrong_die_id_updates_health() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.registerData[0xFF][0] = 0x00;
  bus.registerData[0xFF][1] = 0x00;
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DIE_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
}

void test_recover_success_returns_ready() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced recover timeout", -9);
  (void)dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));

  bus.nowMs = 4321;
  bus.readStatus = Status::Ok();
  Status st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(4321u, dev.lastOkMs());
}

void test_recover_reaches_offline_when_threshold_is_one() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -11);
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_FALSE(dev.isOnline());
}

void test_offline_latches_normal_read_without_i2c_until_recover() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -11);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.recover().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t readsBefore = bus.readCalls;
  int16_t raw = 0;
  Status st = dev.readShuntRaw(Channel::CH1, raw);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  TEST_ASSERT_TRUE(dev.recover().ok());
  TEST_ASSERT_GREATER_THAN_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
}

void test_failed_recover_from_offline_preserves_latch_after_partial_success() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 3;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -12);
  uint16_t reg = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    Status st = dev.readRegister16(cmd::REG_CONFIG, reg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                            static_cast<uint8_t>(st.code));
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(3u, dev.consecutiveFailures());

  bus.readStatus = Status::Ok();
  bus.registerData[cmd::REG_DIE_ID][0] = 0x00;
  bus.registerData[cmd::REG_DIE_ID][1] = 0x00;
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DIE_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.consecutiveFailures() >= 3u);
  TEST_ASSERT_FALSE(dev._allowOfflineI2c);

  const uint32_t readsBefore = bus.readCalls;
  int16_t raw = 0;
  st = dev.readShuntRaw(Channel::CH1, raw);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

// ============================================================================
// Measurement Tests
// ============================================================================

void test_read_shunt_raw() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  // Set CH1 shunt register (0x01) to 0x0190 = 400 decimal
  // Data in bits [15:3]: 400 >> 3 = 50 -> shuntRawToMv = 50 * 0.04 = 2.0 mV
  bus.registerData[0x01][0] = 0x01;
  bus.registerData[0x01][1] = 0x90;

  int16_t raw = 0;
  Status st = dev.readShuntRaw(Channel::CH1, raw);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(0x0190, raw);
}

void test_read_bus_voltage() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  // Set CH2 bus register (0x04) to 0x1770 = 6000 decimal
  // Data in bits [15:3]: 6000 >> 3 = 750 -> busRawToVolts = 750 * 0.008 = 6.0 V
  bus.registerData[0x04][0] = 0x17;
  bus.registerData[0x04][1] = 0x70;

  float volts = 0.0f;
  Status st = dev.readBusVoltage(Channel::CH2, volts);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, volts);
}

void test_read_current() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.shuntResistance[0] = 0.1f;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  // CH1 shunt (0x01): 0x0190 -> shuntMv = 2.0 mV -> current = 2.0 / 0.1 = 20.0 mA
  bus.registerData[0x01][0] = 0x01;
  bus.registerData[0x01][1] = 0x90;

  float mA = 0.0f;
  Status st = dev.readCurrent(Channel::CH1, mA);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, mA);
}

void test_read_channel_full() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.shuntResistance[2] = 0.05f;  // CH3 = 50 mOhm
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  // CH3 shunt (0x05): 0x00C8 = 200 -> 200>>3 = 25 -> 25*0.04 = 1.0 mV
  bus.registerData[0x05][0] = 0x00;
  bus.registerData[0x05][1] = 0xC8;
  // CH3 bus (0x06): 0x0BB8 = 3000 -> 3000>>3 = 375 -> 375*0.008 = 3.0 V
  bus.registerData[0x06][0] = 0x0B;
  bus.registerData[0x06][1] = 0xB8;

  ChannelMeasurement m;
  Status st = dev.readChannel(Channel::CH3, m);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, m.shuntVoltage_mV);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, m.busVoltage_V);
  // Current = 1.0 mV / 0.05 ohm = 20.0 mA
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, m.current_mA);
  // Power = 3.0 V * 20.0 mA = 60.0 mW
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, m.power_mW);
}

void test_triggered_read_is_gated_until_conversion_ready() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SHUNT_BUS_TRIG;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev._conversionStarted);

  ChannelMeasurement m;
  Status st = dev.readChannel(Channel::CH1, m);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CONVERSION_NOT_READY),
                          static_cast<uint8_t>(st.code));

  bus.registerData[0x01][0] = 0x01;
  bus.registerData[0x01][1] = 0x90;
  bus.registerData[0x02][0] = 0x17;
  bus.registerData[0x02][1] = 0x70;
  bus.registerData[0x0F][0] = 0x00;
  bus.registerData[0x0F][1] = 0x01;
  bus.nowMs += 10;

  st = dev.readChannel(Channel::CH1, m);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev._conversionReady);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, m.shuntVoltage_mV);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, m.busVoltage_V);
}

void test_read_not_initialized() {
  INA3221::INA3221 dev;
  int16_t raw = 0;
  Status st = dev.readShuntRaw(Channel::CH1, raw);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED), static_cast<uint8_t>(st.code));
}

// ============================================================================
// Configuration Tests
// ============================================================================

void test_set_mode() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setMode(Mode::SHUNT_TRIG);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_TRIG),
                          static_cast<uint8_t>(dev.getMode()));
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
}

void test_set_mode_rolls_back_cached_config_on_write_failure() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.writeStatus = Status::Error(Err::I2C_BUS, "forced write failure", -2);
  Status st = dev.setMode(Mode::BUS_CONT);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_BUS_CONT),
                          static_cast<uint8_t>(dev.getMode()));
  TEST_ASSERT_FALSE(dev._conversionStarted);
}

void test_set_averaging() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setAveraging(Averaging::AVG_64);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Averaging::AVG_64),
                          static_cast<uint8_t>(dev.getAveraging()));
}

void test_set_averaging_rolls_back_cached_config_on_write_failure() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.writeStatus = Status::Error(Err::I2C_TIMEOUT, "forced write timeout", -3);
  Status st = dev.setAveraging(Averaging::AVG_1024);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Averaging::AVG_1),
                          static_cast<uint8_t>(dev.getAveraging()));
}

void test_set_channel_enable() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setChannelEnable(Channel::CH2, false);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.getChannelEnable(Channel::CH2));
  TEST_ASSERT_TRUE(dev.getChannelEnable(Channel::CH1));
  TEST_ASSERT_TRUE(dev.getChannelEnable(Channel::CH3));
}

void test_set_channel_enable_rejects_disabling_last_active_channel() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.ch2Enable = false;
  cfg.ch3Enable = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.setChannelEnable(Channel::CH1, false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.getChannelEnable(Channel::CH1));
}

void test_set_shunt_resistance() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setShuntResistance(Channel::CH1, 0.05f);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, dev.getShuntResistance(Channel::CH1));
}

void test_set_shunt_resistance_rejects_zero() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setShuntResistance(Channel::CH1, 0.0f);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));
}

void test_set_shunt_resistance_rejects_nan() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setShuntResistance(Channel::CH1,
                                     std::numeric_limits<float>::quiet_NaN());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
}

void test_mask_enable_cache_survives_config_writes() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.setSummationChannels(true, false, true).ok());
  TEST_ASSERT_EQUAL_HEX16(cmd::MASK_SCC1 | cmd::MASK_SCC3, dev._maskEnableWritableCache);

  TEST_ASSERT_TRUE(dev.setAveraging(Averaging::AVG_4).ok());
  TEST_ASSERT_EQUAL_HEX16(cmd::MASK_SCC1 | cmd::MASK_SCC3, dev._maskEnableWritableCache);

  TEST_ASSERT_TRUE(dev.setAlertLatchEnable(true, false).ok());
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_MASK_ENABLE, bus.lastWriteReg);
  TEST_ASSERT_EQUAL_HEX16(cmd::MASK_SCC1 | cmd::MASK_SCC3 | cmd::MASK_WEN,
                          bus.lastWriteValue);
  TEST_ASSERT_EQUAL_HEX16(cmd::MASK_SCC1 | cmd::MASK_SCC3 | cmd::MASK_WEN,
                          dev._maskEnableWritableCache);
}

void test_write_config_with_reset_bit_syncs_cached_defaults() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setAveraging(Averaging::AVG_64).ok());
  TEST_ASSERT_TRUE(dev.setChannelEnable(Channel::CH2, false).ok());
  TEST_ASSERT_TRUE(dev.setSummationChannels(true, false, false).ok());

  Status st = dev.writeConfig(cmd::MASK_RST);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_CONFIG, bus.lastWriteReg);
  TEST_ASSERT_EQUAL_HEX16(cmd::MASK_RST, bus.lastWriteValue);
  TEST_ASSERT_TRUE(dev.getChannelEnable(Channel::CH1));
  TEST_ASSERT_TRUE(dev.getChannelEnable(Channel::CH2));
  TEST_ASSERT_TRUE(dev.getChannelEnable(Channel::CH3));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Averaging::AVG_1),
                          static_cast<uint8_t>(dev.getAveraging()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConvTime::CT_1100US),
                          static_cast<uint8_t>(dev.getVBusConvTime()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConvTime::CT_1100US),
                          static_cast<uint8_t>(dev.getVShuntConvTime()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_BUS_CONT),
                          static_cast<uint8_t>(dev.getMode()));
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_EQUAL_HEX16(0u, dev._maskEnableWritableCache);
}

void test_alert_limit_writes_clear_reserved_bits() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.setCriticalAlertLimit(Channel::CH1, static_cast<int16_t>(0x1237)).ok());
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_CH1_CRIT_LIMIT, bus.lastWriteReg);
  TEST_ASSERT_EQUAL_HEX16(0x1230, bus.lastWriteValue);

  TEST_ASSERT_TRUE(dev.setWarningAlertLimit(Channel::CH2, static_cast<int16_t>(0xC187)).ok());
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_CH2_WARN_LIMIT, bus.lastWriteReg);
  TEST_ASSERT_EQUAL_HEX16(0xC180, bus.lastWriteValue);

  TEST_ASSERT_TRUE(dev.setShuntSumLimit(static_cast<int16_t>(0x1235)).ok());
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_SHUNT_SUM_LIMIT, bus.lastWriteReg);
  TEST_ASSERT_EQUAL_HEX16(0x1234, bus.lastWriteValue);

  TEST_ASSERT_TRUE(dev.setPowerValidUpperLimit(static_cast<int16_t>(0xFFFF)).ok());
  TEST_ASSERT_EQUAL_HEX8(cmd::REG_PV_UPPER_LIMIT, bus.lastWriteReg);
  TEST_ASSERT_EQUAL_HEX16(0x7FF8, bus.lastWriteValue);
}

// ============================================================================
// Utility Tests
// ============================================================================

void test_shunt_raw_to_mv() {
  // 0x0190 = 400 -> 400 >> 3 = 50 -> 50 * 0.04 = 2.0 mV
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, INA3221::INA3221::shuntRawToMv(0x0190));
}

void test_bus_raw_to_volts() {
  // 0x1770 = 6000 -> 6000 >> 3 = 750 -> 750 * 0.008 = 6.0 V
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, INA3221::INA3221::busRawToVolts(0x1770));
}

void test_mv_to_shunt_raw() {
  // 2.0 mV -> 2.0 / 0.04 = 50 -> 50 << 3 = 400 = 0x0190
  TEST_ASSERT_EQUAL_INT16(0x0190, INA3221::INA3221::mvToShuntRaw(2.0f));
}

void test_volts_to_bus_raw() {
  // 6.0 V -> 6.0 / 0.008 = 750 -> 750 << 3 = 6000 = 0x1770
  TEST_ASSERT_EQUAL_INT16(0x1770, INA3221::INA3221::voltsToBusRaw(6.0f));
}

void test_conversion_time_us() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  // Default: SHUNT_BUS_CONT, vBusCt=CT_1100US, vShCt=CT_1100US
  // convTimeUs = 1100 + 1100 = 2200
  TEST_ASSERT_EQUAL_UINT32(2200u, dev.getConversionTimeUs());
}

void test_cycle_time_us() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  // All 3 channels enabled, each takes 2200 us
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_EQUAL_UINT32(6600u, dev.getCycleTimeUs());
}

void test_cycle_time_includes_averaging() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.averaging = Averaging::AVG_1024;
  cfg.mode = Mode::SHUNT_BUS_TRIG;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  TEST_ASSERT_EQUAL_UINT32(2200u, dev.getConversionTimeUs());
  TEST_ASSERT_EQUAL_UINT32(6758400u, dev.getCycleTimeUs());
}

void test_start_conversion_rejects_power_down_mode() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::POWER_DOWN;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.startConversion();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev._conversionStarted);
}

void test_read_conversion_ready_propagates_i2c_error() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SHUNT_BUS_TRIG;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.nowMs += 10;
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced ready timeout", -4);
  bool ready = true;
  Status st = dev.readConversionReady(ready);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(ready);
}

void test_read_blocking_times_out_with_stalled_clock() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SHUNT_BUS_TRIG;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  ChannelMeasurement m;
  Status st = dev.readBlocking(&m, nullptr, nullptr, 5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(bus.yieldCalls > 0);
}

// ============================================================================
// Transport Wrapper Tests
// ============================================================================

void test_raw_transport_rejects_invalid_buffers() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  uint8_t byte = 0;
  uint8_t rx = 0;

  Status st = dev._i2cWriteRaw(nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteRaw(&byte, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(nullptr, 1, &rx, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 0, &rx, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 1, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 1, &rx, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));
}

void test_register_access_after_end_does_not_touch_bus() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t writesAfterBegin = bus.writeCalls;
  const uint32_t readsAfterBegin = bus.readCalls;

  dev.end();
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin + 1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);

  uint16_t value = 0;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);

  st = dev.writeRegister16(cmd::REG_CONFIG, cmd::CONFIG_DEFAULT);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin + 1u, bus.writeCalls);
}

void test_end_while_offline_does_not_touch_bus() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -13);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.recover().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  const uint32_t writesBefore = bus.writeCalls;
  dev.end();
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_invalid_raw_register_address_does_not_touch_bus() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  uint16_t value = 0;
  Status st = dev.readRegister16(0x12, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);

  st = dev.writeRegister16(0x12, 0x1234);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

// ============================================================================
// Build Config Register Test
// ============================================================================

void test_build_config_register() {
  FakeBus bus;
  INA3221::INA3221 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  // Default config: CH1|CH2|CH3 EN, AVG_1, CT_1100US, CT_1100US, SHUNT_BUS_CONT
  // = 0x4000 | 0x2000 | 0x1000 | 0x0000 | 0x0100 | 0x0020 | 0x0007
  // = 0x7127
  uint16_t reg = dev._buildConfigRegister();
  TEST_ASSERT_EQUAL_HEX16(0x7127, reg);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  UNITY_BEGIN();

  // Status
  RUN_TEST(test_status_ok);
  RUN_TEST(test_status_error);
  RUN_TEST(test_status_in_progress);

  // Config
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_get_settings_snapshot);

  // Lifecycle
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_begin_rejects_invalid_address);
  RUN_TEST(test_begin_rejects_zero_shunt_resistance);
  RUN_TEST(test_begin_rejects_nan_shunt_resistance);
  RUN_TEST(test_begin_rejects_active_mode_with_all_channels_disabled);
  RUN_TEST(test_invalid_begin_resets_runtime_and_default_config);
  RUN_TEST(test_failed_begin_probe_resets_cached_config);
  RUN_TEST(test_begin_normalizes_offline_threshold_on_stored_copy);
  RUN_TEST(test_begin_success_sets_ready_and_counters);

  // Probe / Recover
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_detects_wrong_manufacturer_id);
  RUN_TEST(test_probe_detects_wrong_die_id);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_wrong_die_id_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);
  RUN_TEST(test_offline_latches_normal_read_without_i2c_until_recover);
  RUN_TEST(test_failed_recover_from_offline_preserves_latch_after_partial_success);

  // Measurements
  RUN_TEST(test_read_shunt_raw);
  RUN_TEST(test_read_bus_voltage);
  RUN_TEST(test_read_current);
  RUN_TEST(test_read_channel_full);
  RUN_TEST(test_triggered_read_is_gated_until_conversion_ready);
  RUN_TEST(test_read_not_initialized);

  // Configuration
  RUN_TEST(test_set_mode);
  RUN_TEST(test_set_mode_rolls_back_cached_config_on_write_failure);
  RUN_TEST(test_set_averaging);
  RUN_TEST(test_set_averaging_rolls_back_cached_config_on_write_failure);
  RUN_TEST(test_set_channel_enable);
  RUN_TEST(test_set_channel_enable_rejects_disabling_last_active_channel);
  RUN_TEST(test_set_shunt_resistance);
  RUN_TEST(test_set_shunt_resistance_rejects_zero);
  RUN_TEST(test_set_shunt_resistance_rejects_nan);
  RUN_TEST(test_mask_enable_cache_survives_config_writes);
  RUN_TEST(test_write_config_with_reset_bit_syncs_cached_defaults);
  RUN_TEST(test_alert_limit_writes_clear_reserved_bits);

  // Utility
  RUN_TEST(test_shunt_raw_to_mv);
  RUN_TEST(test_bus_raw_to_volts);
  RUN_TEST(test_mv_to_shunt_raw);
  RUN_TEST(test_volts_to_bus_raw);
  RUN_TEST(test_conversion_time_us);
  RUN_TEST(test_cycle_time_us);
  RUN_TEST(test_cycle_time_includes_averaging);
  RUN_TEST(test_start_conversion_rejects_power_down_mode);
  RUN_TEST(test_read_conversion_ready_propagates_i2c_error);
  RUN_TEST(test_read_blocking_times_out_with_stalled_clock);

  // Transport
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  RUN_TEST(test_register_access_after_end_does_not_touch_bus);
  RUN_TEST(test_end_while_offline_does_not_touch_bus);
  RUN_TEST(test_invalid_raw_register_address_does_not_touch_bus);

  // Config register
  RUN_TEST(test_build_config_register);

  return UNITY_END();
}
