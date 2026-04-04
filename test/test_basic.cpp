/// @file test_basic.cpp
/// @brief Native contract tests for INA3221 lifecycle and health behavior.

#include <unity.h>

#include "Arduino.h"
#include "Wire.h"

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

Status fakeWrite(uint8_t, const uint8_t*, size_t, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  return bus->writeStatus;
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

Config makeConfig(FakeBus& bus) {
  Config cfg;
  cfg.i2cWrite = fakeWrite;
  cfg.i2cWriteRead = fakeWriteRead;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
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

void test_begin_success_sets_ready_and_counters() {
  FakeBus bus;
  INA3221::INA3221 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.isOnline());
  // begin() does: probe (2 raw reads, no tracking) + applyConfig (1 tracked write)
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastOkMs());
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
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SHUNT_TRIG),
                          static_cast<uint8_t>(dev.getMode()));
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

  // Lifecycle
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_begin_rejects_invalid_address);
  RUN_TEST(test_begin_rejects_zero_shunt_resistance);
  RUN_TEST(test_begin_success_sets_ready_and_counters);

  // Probe / Recover
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_detects_wrong_manufacturer_id);
  RUN_TEST(test_probe_detects_wrong_die_id);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);

  // Measurements
  RUN_TEST(test_read_shunt_raw);
  RUN_TEST(test_read_bus_voltage);
  RUN_TEST(test_read_current);
  RUN_TEST(test_read_channel_full);
  RUN_TEST(test_read_not_initialized);

  // Configuration
  RUN_TEST(test_set_mode);
  RUN_TEST(test_set_averaging);
  RUN_TEST(test_set_channel_enable);
  RUN_TEST(test_set_shunt_resistance);
  RUN_TEST(test_set_shunt_resistance_rejects_zero);

  // Utility
  RUN_TEST(test_shunt_raw_to_mv);
  RUN_TEST(test_bus_raw_to_volts);
  RUN_TEST(test_mv_to_shunt_raw);
  RUN_TEST(test_volts_to_bus_raw);
  RUN_TEST(test_conversion_time_us);
  RUN_TEST(test_cycle_time_us);

  // Transport
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);

  // Config register
  RUN_TEST(test_build_config_register);

  return UNITY_END();
}
