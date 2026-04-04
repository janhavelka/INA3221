/// @file main.cpp
/// @brief INA3221 basic bringup example
/// @note This is an EXAMPLE, not part of the library

#include <Arduino.h>
#include <cstdlib>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

#include "INA3221/INA3221.h"

// ============================================================================
// Globals
// ============================================================================

INA3221::INA3221 device;
bool verboseMode = false;

// ============================================================================
// Helper Functions
// ============================================================================

const char* errToStr(INA3221::Err err) {
  using INA3221::Err;
  switch (err) {
    case Err::OK:                       return "OK";
    case Err::NOT_INITIALIZED:          return "NOT_INITIALIZED";
    case Err::INVALID_CONFIG:           return "INVALID_CONFIG";
    case Err::I2C_ERROR:                return "I2C_ERROR";
    case Err::TIMEOUT:                  return "TIMEOUT";
    case Err::INVALID_PARAM:            return "INVALID_PARAM";
    case Err::DEVICE_NOT_FOUND:         return "DEVICE_NOT_FOUND";
    case Err::MANUFACTURER_ID_MISMATCH: return "MANUFACTURER_ID_MISMATCH";
    case Err::DIE_ID_MISMATCH:          return "DIE_ID_MISMATCH";
    case Err::CONVERSION_NOT_READY:     return "CONVERSION_NOT_READY";
    case Err::BUSY:                     return "BUSY";
    case Err::IN_PROGRESS:              return "IN_PROGRESS";
    default:                            return "UNKNOWN";
  }
}

const char* stateToStr(INA3221::DriverState st) {
  using INA3221::DriverState;
  switch (st) {
    case DriverState::UNINIT:   return "UNINIT";
    case DriverState::READY:    return "READY";
    case DriverState::DEGRADED: return "DEGRADED";
    case DriverState::OFFLINE:  return "OFFLINE";
    default:                    return "UNKNOWN";
  }
}

const char* stateColor(INA3221::DriverState st, bool online, uint8_t consecutiveFailures) {
  if (st == INA3221::DriverState::UNINIT) {
    return LOG_COLOR_YELLOW;
  }
  return LOG_COLOR_STATE(online, consecutiveFailures);
}

const char* goodIfZeroColor(uint32_t value) {
  return (value == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

const char* goodIfNonZeroColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* onOffColor(bool enabled) {
  return enabled ? LOG_COLOR_GREEN : LOG_COLOR_RESET;
}

const char* yesNoColor(bool value) {
  return value ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* successRateColor(float pct) {
  if (pct >= 99.9f) return LOG_COLOR_GREEN;
  if (pct >= 80.0f) return LOG_COLOR_YELLOW;
  return LOG_COLOR_RED;
}

const char* staleTimeColor(bool isErrorTimestamp) {
  return isErrorTimestamp ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

void printStatus(const INA3221::Status& st) {
  Serial.printf("  Status: %s%s%s (code=%u, detail=%ld)\n",
                LOG_COLOR_RESULT(st.ok()),
                errToStr(st.code),
                LOG_COLOR_RESET,
                static_cast<unsigned>(st.code),
                static_cast<long>(st.detail));
  if (st.msg && st.msg[0]) {
    Serial.printf("  Message: %s%s%s\n", LOG_COLOR_YELLOW, st.msg, LOG_COLOR_RESET);
  }
}

void printDriverHealth() {
  const uint32_t now = millis();
  const uint32_t totalOk = device.totalSuccess();
  const uint32_t totalFail = device.totalFailures();
  const uint32_t total = totalOk + totalFail;
  const float successRate = (total > 0U)
                                ? (100.0f * static_cast<float>(totalOk) / static_cast<float>(total))
                                : 0.0f;
  const INA3221::Status lastErr = device.lastError();
  const INA3221::DriverState st = device.state();
  const bool online = device.isOnline();

  Serial.println("=== Driver Health ===");
  Serial.printf("  State: %s%s%s\n",
                stateColor(st, online, device.consecutiveFailures()),
                stateToStr(st),
                LOG_COLOR_RESET);
  Serial.printf("  Online: %s%s%s\n",
                online ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(online),
                LOG_COLOR_RESET);
  Serial.printf("  Consecutive failures: %s%u%s\n",
                goodIfZeroColor(device.consecutiveFailures()),
                device.consecutiveFailures(),
                LOG_COLOR_RESET);
  Serial.printf("  Total success: %s%lu%s\n",
                goodIfNonZeroColor(totalOk),
                static_cast<unsigned long>(totalOk),
                LOG_COLOR_RESET);
  Serial.printf("  Total failures: %s%lu%s\n",
                goodIfZeroColor(totalFail),
                static_cast<unsigned long>(totalFail),
                LOG_COLOR_RESET);
  Serial.printf("  Success rate: %s%.1f%%%s\n",
                successRateColor(successRate),
                successRate,
                LOG_COLOR_RESET);

  const uint32_t lastOkMs = device.lastOkMs();
  if (lastOkMs > 0U) {
    Serial.printf("  Last OK: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_GREEN,
                  static_cast<unsigned long>(now - lastOkMs),
                  static_cast<unsigned long>(lastOkMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last OK: %snever%s\n", staleTimeColor(false), LOG_COLOR_RESET);
  }

  const uint32_t lastErrorMs = device.lastErrorMs();
  if (lastErrorMs > 0U) {
    Serial.printf("  Last error: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_RED,
                  static_cast<unsigned long>(now - lastErrorMs),
                  static_cast<unsigned long>(lastErrorMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last error: %snever%s\n", staleTimeColor(true), LOG_COLOR_RESET);
  }

  if (!lastErr.ok()) {
    Serial.printf("  Error code: %s%s%s\n",
                  LOG_COLOR_RED,
                  errToStr(lastErr.code),
                  LOG_COLOR_RESET);
    Serial.printf("  Error detail: %ld\n", static_cast<long>(lastErr.detail));
    if (lastErr.msg && lastErr.msg[0]) {
      Serial.printf("  Error msg: %s%s%s\n", LOG_COLOR_YELLOW, lastErr.msg, LOG_COLOR_RESET);
    }
  }
}

void printHelp() {
  auto helpSection = [](const char* title) {
    Serial.printf("\n%s[%s]%s\n", LOG_COLOR_GREEN, title, LOG_COLOR_RESET);
  };
  auto helpItem = [](const char* cmd, const char* desc) {
    Serial.printf("  %s%-32s%s - %s\n", LOG_COLOR_CYAN, cmd, LOG_COLOR_RESET, desc);
  };

  Serial.println();
  Serial.printf("%s=== INA3221 CLI Help ===%s\n", LOG_COLOR_CYAN, LOG_COLOR_RESET);
  helpSection("Common");
  helpItem("help / ?", "Show this help");
  helpItem("version / ver", "Print firmware and library version info");
  helpItem("scan", "Scan I2C bus");
  helpItem("read", "Read all enabled channels (blocking)");
  helpItem("read N", "Read N measurements");
  helpItem("ch <1|2|3>", "Read single channel");
  helpItem("shunt <1|2|3>", "Read shunt voltage (mV)");
  helpItem("bus <1|2|3>", "Read bus voltage (V)");
  helpItem("current <1|2|3>", "Read current (mA)");
  helpItem("power <1|2|3>", "Read power (mW)");
  helpItem("sum", "Read shunt-voltage sum (mV)");
  helpItem("shuntraw <1|2|3>", "Read raw shunt register value");
  helpItem("busraw <1|2|3>", "Read raw bus register value");
  helpItem("sumraw", "Read raw shunt sum register value");
  helpItem("ids", "Read manufacturer and die IDs");
  helpItem("timing", "Show conversion time and cycle timing");
  helpItem("start", "Start single-shot conversion");
  helpItem("start <mode>", "Start with triggered mode (strig/btrig/sbtrig)");
  helpItem("poll", "Check if conversion is ready");

  helpSection("Configuration");
  helpItem("mode [pd|strig|btrig|sbtrig|sc|bc|sbc]", "Set/show operating mode");
  helpItem("avg [0..7]", "Set/show averaging (0=1,...,7=1024)");
  helpItem("vbusct [0..7]", "Set/show bus voltage conv time");
  helpItem("vshct [0..7]", "Set/show shunt voltage conv time");
  helpItem("chen <1|2|3> <0|1>", "Enable/disable channel");
  helpItem("rshunt <1|2|3> <ohms>", "Set shunt resistance");
  helpItem("config", "Dump config register");
  helpItem("config write <hex>", "Write full config register");
  helpItem("reset", "Software reset");

  helpSection("Alerts");
  helpItem("alerts", "Read alert flags");
  helpItem("crit <1|2|3> [raw]", "Get/set critical alert limit");
  helpItem("warn <1|2|3> [raw]", "Get/set warning alert limit");
  helpItem("sumlim [raw]", "Get/set shunt sum limit");
  helpItem("pvhi [raw]", "Get/set power valid upper limit");
  helpItem("pvlo [raw]", "Get/set power valid lower limit");
  helpItem("sumch <1|2|3> <0|1>", "Enable/disable channel in summation");
  helpItem("latch <warn> <crit>", "Set alert latch enable (0|1 0|1)");

  helpSection("Diagnostics");
  helpItem("drv", "Show driver state and health");
  helpItem("probe", "Probe device (no health tracking)");
  helpItem("recover", "Manual recovery attempt");
  helpItem("online", "Show online state");
  helpItem("cfg / settings", "Print active configuration snapshot");
  helpItem("verbose [0|1]", "Enable/disable verbose output");
  helpItem("stress [N]", "Run N measurement cycles");
  helpItem("stress_mix [N]", "Run N mixed-operation stress cycles");
  helpItem("selftest", "Run safe command self-test report");
  helpItem("convert shunt <raw>", "Convert shunt raw to mV");
  helpItem("convert bus <raw>", "Convert bus raw to V");
}

void printVersionInfo() {
  Serial.println("=== Version Info ===");
  Serial.printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("  INA3221 library version: %s\n", INA3221::VERSION);
  Serial.printf("  INA3221 version code: %d (major=%d minor=%d patch=%d)\n",
                INA3221::VERSION_INT,
                INA3221::VERSION_MAJOR,
                INA3221::VERSION_MINOR,
                INA3221::VERSION_PATCH);
}

const char* modeToStr(INA3221::Mode mode) {
  using INA3221::Mode;
  switch (mode) {
    case Mode::POWER_DOWN:      return "POWER_DOWN";
    case Mode::SHUNT_TRIG:      return "SHUNT_TRIG";
    case Mode::BUS_TRIG:        return "BUS_TRIG";
    case Mode::SHUNT_BUS_TRIG:  return "SHUNT_BUS_TRIG";
    case Mode::POWER_DOWN_ALT:  return "POWER_DOWN_ALT";
    case Mode::SHUNT_CONT:      return "SHUNT_CONT";
    case Mode::BUS_CONT:        return "BUS_CONT";
    case Mode::SHUNT_BUS_CONT:  return "SHUNT_BUS_CONT";
    default:                    return "UNKNOWN";
  }
}

const char* avgToStr(INA3221::Averaging avg) {
  using INA3221::Averaging;
  switch (avg) {
    case Averaging::AVG_1:    return "1";
    case Averaging::AVG_4:    return "4";
    case Averaging::AVG_16:   return "16";
    case Averaging::AVG_64:   return "64";
    case Averaging::AVG_128:  return "128";
    case Averaging::AVG_256:  return "256";
    case Averaging::AVG_512:  return "512";
    case Averaging::AVG_1024: return "1024";
    default:                  return "UNKNOWN";
  }
}

const char* ctToStr(INA3221::ConvTime ct) {
  using INA3221::ConvTime;
  switch (ct) {
    case ConvTime::CT_140US:  return "140us";
    case ConvTime::CT_204US:  return "204us";
    case ConvTime::CT_332US:  return "332us";
    case ConvTime::CT_588US:  return "588us";
    case ConvTime::CT_1100US: return "1100us";
    case ConvTime::CT_2116US: return "2116us";
    case ConvTime::CT_4156US: return "4156us";
    case ConvTime::CT_8244US: return "8244us";
    default:                  return "UNKNOWN";
  }
}

bool parseI32(const String& token, int32_t& out) {
  char* end = nullptr;
  const long value = strtol(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<int32_t>(value);
  return true;
}

bool parseU32(const String& token, uint32_t& out) {
  char* end = nullptr;
  const unsigned long value = strtoul(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseFloat(const String& token, float& out) {
  char* end = nullptr;
  const float value = strtof(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = value;
  return true;
}

int parseChannel(const String& token) {
  int ch = token.toInt();
  if (ch < 1 || ch > 3) {
    return -1;
  }
  return ch - 1;  // Convert to 0-based
}

void printChannelMeasurement(int chNum, const INA3221::ChannelMeasurement& m) {
  Serial.printf("  CH%d: Vshunt=%.3f mV  Vbus=%.3f V  I=%.3f mA  P=%.3f mW\n",
                chNum,
                static_cast<double>(m.shuntVoltage_mV),
                static_cast<double>(m.busVoltage_V),
                static_cast<double>(m.current_mA),
                static_cast<double>(m.power_mW));
}

void readAllChannels() {
  INA3221::ChannelMeasurement ch1, ch2, ch3;
  INA3221::ChannelMeasurement* p1 = device.getChannelEnable(INA3221::Channel::CH1) ? &ch1 : nullptr;
  INA3221::ChannelMeasurement* p2 = device.getChannelEnable(INA3221::Channel::CH2) ? &ch2 : nullptr;
  INA3221::ChannelMeasurement* p3 = device.getChannelEnable(INA3221::Channel::CH3) ? &ch3 : nullptr;

  auto st = device.readBlocking(p1, p2, p3);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  if (p1) printChannelMeasurement(1, ch1);
  if (p2) printChannelMeasurement(2, ch2);
  if (p3) printChannelMeasurement(3, ch3);
}

void printConfig() {
  uint16_t config = 0;
  INA3221::Status st = device.readConfig(config);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  Serial.printf("  Config: 0x%04X\n", config);
  Serial.printf("  Mode: %s\n", modeToStr(device.getMode()));
  Serial.printf("  Averaging: %s samples\n", avgToStr(device.getAveraging()));
  Serial.printf("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  Serial.printf("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  Serial.printf("  CH1: %s  CH2: %s  CH3: %s\n",
                device.getChannelEnable(INA3221::Channel::CH1) ? "ON" : "OFF",
                device.getChannelEnable(INA3221::Channel::CH2) ? "ON" : "OFF",
                device.getChannelEnable(INA3221::Channel::CH3) ? "ON" : "OFF");
  Serial.printf("  Rshunt: CH1=%.4f  CH2=%.4f  CH3=%.4f ohm\n",
                static_cast<double>(device.getShuntResistance(INA3221::Channel::CH1)),
                static_cast<double>(device.getShuntResistance(INA3221::Channel::CH2)),
                static_cast<double>(device.getShuntResistance(INA3221::Channel::CH3)));
  Serial.printf("  Cycle time: %lu us\n", static_cast<unsigned long>(device.getCycleTimeUs()));
}

void printTimingInfo() {
  Serial.println("=== Timing Info ===");
  Serial.printf("  Conversion time: %lu us\n", static_cast<unsigned long>(device.getConversionTimeUs()));
  Serial.printf("  Cycle time: %lu us\n", static_cast<unsigned long>(device.getCycleTimeUs()));
  Serial.printf("  Averaging: %s samples\n", avgToStr(device.getAveraging()));
  Serial.printf("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  Serial.printf("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  Serial.printf("  Shunt LSB: 0.04 mV (40 uV)\n");
  Serial.printf("  Bus LSB: 8 mV\n");
  Serial.printf("  Data shift: 3 bits (data in [15:3])\n");
}

void runSelfTest() {
  struct TestStats {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
  } stats;

  enum class SelftestOutcome : uint8_t { PASS, FAIL, SKIP };
  auto report = [&](const char* name, SelftestOutcome outcome, const char* note) {
    const bool passed = (outcome == SelftestOutcome::PASS);
    const bool skipped = (outcome == SelftestOutcome::SKIP);
    const char* color = skipped ? LOG_COLOR_YELLOW : LOG_COLOR_RESULT(passed);
    const char* tag = skipped ? "SKIP" : (passed ? "PASS" : "FAIL");
    Serial.printf("  [%s%s%s] %s", color, tag, LOG_COLOR_RESET, name);
    if (note && note[0]) {
      Serial.printf(" - %s", note);
    }
    Serial.println();
    if (skipped) {
      stats.skip++;
    } else if (passed) {
      stats.pass++;
    } else {
      stats.fail++;
    }
  };
  auto reportCheck = [&](const char* name, bool passed, const char* note) {
    report(name, passed ? SelftestOutcome::PASS : SelftestOutcome::FAIL, note);
  };
  auto reportSkip = [&](const char* name, const char* note) {
    report(name, SelftestOutcome::SKIP, note);
  };

  Serial.println("=== INA3221 selftest (safe commands) ===");

  const uint32_t succBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint8_t consBefore = device.consecutiveFailures();

  const INA3221::Status pst = device.probe();
  if (pst.code == INA3221::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                  goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                  goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                  LOG_COLOR_YELLOW, static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
    return;
  }
  const bool probeHealthUnchanged =
      device.totalSuccess() == succBefore &&
      device.totalFailures() == failBefore &&
      device.consecutiveFailures() == consBefore;
  reportCheck("probe responds", pst.ok(), pst.ok() ? "" : errToStr(pst.code));
  reportCheck("probe no-health-side-effects", probeHealthUnchanged, "");

  uint16_t cfg = 0;
  INA3221::Status st = device.readConfig(cfg);
  reportCheck("readConfig", st.ok(), st.ok() ? "" : errToStr(st.code));

  // Read manufacturer ID
  uint16_t mfgId = 0;
  st = device.readManufacturerId(mfgId);
  reportCheck("readManufacturerId", st.ok() && mfgId == 0x5449,
              st.ok() ? "" : errToStr(st.code));

  // Read die ID
  uint16_t dieId = 0;
  st = device.readDieId(dieId);
  reportCheck("readDieId", st.ok() && dieId == 0x3220,
              st.ok() ? "" : errToStr(st.code));

  // Read a channel
  INA3221::ChannelMeasurement m;
  st = device.readChannel(INA3221::Channel::CH1, m);
  reportCheck("readChannel(CH1)", st.ok(), st.ok() ? "" : errToStr(st.code));

  // Test mode changes
  st = device.setMode(INA3221::Mode::SHUNT_BUS_CONT);
  reportCheck("setMode(SHUNT_BUS_CONT)", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.setAveraging(INA3221::Averaging::AVG_1);
  reportCheck("setAveraging(1)", st.ok(), st.ok() ? "" : errToStr(st.code));

  // Test recovery
  st = device.recover();
  reportCheck("recover", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportCheck("isOnline", device.isOnline(), "");

  Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                LOG_COLOR_YELLOW, static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
}

void runStressMix(int count) {
  LOGI("Starting stress_mix test: %d cycles", count);
  int ok = 0;
  int fail = 0;
  bool hasFailure = false;
  INA3221::Status firstFailure = INA3221::Status::Ok();
  INA3221::Status lastFailure = INA3221::Status::Ok();

  auto note = [&](const INA3221::Status& st) {
    if (st.ok()) {
      ok++;
    } else {
      fail++;
      if (!hasFailure) {
        firstFailure = st;
        hasFailure = true;
      }
      lastFailure = st;
      if (verboseMode) {
        printStatus(st);
      }
    }
  };

  for (int i = 0; i < count; ++i) {
    // readBlocking all channels
    INA3221::ChannelMeasurement ch1, ch2, ch3;
    note(device.readBlocking(&ch1, &ch2, &ch3));

    // Read config register
    uint16_t cfg = 0;
    note(device.readConfig(cfg));

    // Read manufacturer ID
    uint16_t mfgId = 0;
    note(device.readManufacturerId(mfgId));

    // Read individual channel shunt voltage
    float shuntMv = 0.0f;
    note(device.readShuntVoltage(INA3221::Channel::CH1, shuntMv));

    // Read individual channel bus voltage
    float busV = 0.0f;
    note(device.readBusVoltage(INA3221::Channel::CH2, busV));

    // Read alert flags
    INA3221::AlertFlags flags;
    note(device.readAlertFlags(flags));

    LOGV(verboseMode, "  %d/%d: ok=%d fail=%d", i + 1, count, ok, fail);
  }

  const int total = ok + fail;
  const float pct = (total > 0) ? (100.0f * static_cast<float>(ok) / static_cast<float>(total)) : 0.0f;
  Serial.printf("  Stress_mix results: %s%d ok%s, %s%d failed%s (%s%.2f%%%s) [%d ops in %d cycles]\n",
                goodIfNonZeroColor(static_cast<uint32_t>(ok)),
                ok,
                LOG_COLOR_RESET,
                goodIfZeroColor(static_cast<uint32_t>(fail)),
                fail,
                LOG_COLOR_RESET,
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET,
                total,
                count);
  if (hasFailure) {
    Serial.println("  Failure details:");
    Serial.println("  First failure:");
    printStatus(firstFailure);
    if (fail > 1) {
      Serial.println("  Last failure:");
      printStatus(lastFailure);
    }
  }
}

// ============================================================================
// Command Processing
// ============================================================================

void processCommand(const String& cmdLine) {
  String cmd = cmdLine;
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "version" || cmd == "ver") {
    printVersionInfo();
  } else if (cmd == "scan") {
    bus_diag::scan();
  } else if (cmd == "probe") {
    LOGI("Probing device (no health tracking)...");
    auto st = device.probe();
    printStatus(st);
  } else if (cmd == "drv") {
    printDriverHealth();
  } else if (cmd == "recover") {
    LOGI("Attempting recovery...");
    auto st = device.recover();
    printStatus(st);
    printDriverHealth();
  } else if (cmd == "verbose") {
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
  } else if (cmd.startsWith("verbose ")) {
    int val = cmd.substring(8).toInt();
    verboseMode = (val != 0);
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
  } else if (cmd == "read") {
    readAllChannels();
  } else if (cmd.startsWith("read ")) {
    int count = cmd.substring(5).toInt();
    if (count <= 0 || count > 10000) {
      LOGW("Invalid count (1-10000)");
      return;
    }
    for (int i = 0; i < count; ++i) {
      Serial.printf("--- Reading %d/%d ---\n", i + 1, count);
      readAllChannels();
    }
  } else if (cmd.startsWith("ch ")) {
    int ch = parseChannel(cmd.substring(3));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    INA3221::ChannelMeasurement m;
    auto st = device.readChannel(static_cast<INA3221::Channel>(ch), m);
    if (st.ok()) {
      printChannelMeasurement(ch + 1, m);
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("shuntraw ")) {
    int ch = parseChannel(cmd.substring(9));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    int16_t raw = 0;
    auto st = device.readShuntRaw(static_cast<INA3221::Channel>(ch), raw);
    if (st.ok()) {
      Serial.printf("  CH%d shunt raw: %d (%.3f mV)\n", ch + 1, raw,
                    static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("shunt ")) {
    int ch = parseChannel(cmd.substring(6));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    float mV = 0.0f;
    auto st = device.readShuntVoltage(static_cast<INA3221::Channel>(ch), mV);
    if (st.ok()) {
      Serial.printf("  CH%d shunt: %.3f mV\n", ch + 1, static_cast<double>(mV));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("busraw ")) {
    int ch = parseChannel(cmd.substring(7));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    int16_t raw = 0;
    auto st = device.readBusRaw(static_cast<INA3221::Channel>(ch), raw);
    if (st.ok()) {
      Serial.printf("  CH%d bus raw: %d (%.3f V)\n", ch + 1, raw,
                    static_cast<double>(INA3221::INA3221::busRawToVolts(raw)));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("bus ")) {
    int ch = parseChannel(cmd.substring(4));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    float volts = 0.0f;
    auto st = device.readBusVoltage(static_cast<INA3221::Channel>(ch), volts);
    if (st.ok()) {
      Serial.printf("  CH%d bus: %.3f V\n", ch + 1, static_cast<double>(volts));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("current ")) {
    int ch = parseChannel(cmd.substring(8));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    float mA = 0.0f;
    auto st = device.readCurrent(static_cast<INA3221::Channel>(ch), mA);
    if (st.ok()) {
      Serial.printf("  CH%d current: %.3f mA\n", ch + 1, static_cast<double>(mA));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("power ")) {
    int ch = parseChannel(cmd.substring(6));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    float mW = 0.0f;
    auto st = device.readPower(static_cast<INA3221::Channel>(ch), mW);
    if (st.ok()) {
      Serial.printf("  CH%d power: %.3f mW\n", ch + 1, static_cast<double>(mW));
    } else {
      printStatus(st);
    }
  } else if (cmd == "sum") {
    float mV = 0.0f;
    auto st = device.readShuntSumVoltage(mV);
    if (st.ok()) {
      Serial.printf("  Shunt sum: %.3f mV\n", static_cast<double>(mV));
    } else {
      printStatus(st);
    }
  } else if (cmd == "sumraw") {
    int16_t raw = 0;
    auto st = device.readShuntSumRaw(raw);
    if (st.ok()) {
      Serial.printf("  Shunt sum raw: %d\n", raw);
    } else {
      printStatus(st);
    }
  } else if (cmd == "timing") {
    printTimingInfo();
  } else if (cmd == "start") {
    auto st = device.startConversion();
    printStatus(st);
  } else if (cmd.startsWith("start ")) {
    String token = cmd.substring(6);
    token.trim();
    INA3221::Mode mode = INA3221::Mode::SHUNT_BUS_TRIG;
    if (token == "strig") {
      mode = INA3221::Mode::SHUNT_TRIG;
    } else if (token == "btrig") {
      mode = INA3221::Mode::BUS_TRIG;
    } else if (token == "sbtrig") {
      mode = INA3221::Mode::SHUNT_BUS_TRIG;
    } else {
      LOGW("Invalid mode (strig/btrig/sbtrig)");
      return;
    }
    auto st = device.startConversion(mode);
    printStatus(st);
  } else if (cmd == "poll") {
    bool ready = device.conversionReady();
    LOGI("Conversion ready: %s%s%s", yesNoColor(ready), ready ? "YES" : "NO", LOG_COLOR_RESET);
  } else if (cmd == "ids") {
    uint16_t mfgId = 0;
    uint16_t dieId = 0;
    auto st1 = device.readManufacturerId(mfgId);
    auto st2 = device.readDieId(dieId);
    if (st1.ok() && st2.ok()) {
      Serial.printf("  Manufacturer ID: 0x%04X (%s)\n", mfgId,
                    (mfgId == 0x5449) ? "OK - TI" : "UNEXPECTED");
      Serial.printf("  Die ID: 0x%04X (%s)\n", dieId,
                    (dieId == 0x3220) ? "OK - INA3221" : "UNEXPECTED");
    } else {
      if (!st1.ok()) printStatus(st1);
      if (!st2.ok()) printStatus(st2);
    }
  } else if (cmd == "mode") {
    Serial.printf("  Mode: %s\n", modeToStr(device.getMode()));
  } else if (cmd.startsWith("mode ")) {
    String token = cmd.substring(5);
    token.trim();
    INA3221::Mode mode = INA3221::Mode::SHUNT_BUS_CONT;
    if (token == "pd") {
      mode = INA3221::Mode::POWER_DOWN;
    } else if (token == "strig") {
      mode = INA3221::Mode::SHUNT_TRIG;
    } else if (token == "btrig") {
      mode = INA3221::Mode::BUS_TRIG;
    } else if (token == "sbtrig") {
      mode = INA3221::Mode::SHUNT_BUS_TRIG;
    } else if (token == "sc") {
      mode = INA3221::Mode::SHUNT_CONT;
    } else if (token == "bc") {
      mode = INA3221::Mode::BUS_CONT;
    } else if (token == "sbc") {
      mode = INA3221::Mode::SHUNT_BUS_CONT;
    } else {
      LOGW("Invalid mode (pd/strig/btrig/sbtrig/sc/bc/sbc)");
      return;
    }
    printStatus(device.setMode(mode));
  } else if (cmd == "avg") {
    Serial.printf("  Averaging: %s samples\n", avgToStr(device.getAveraging()));
  } else if (cmd.startsWith("avg ")) {
    int val = cmd.substring(4).toInt();
    if (val < 0 || val > 7) {
      LOGW("Invalid avg (0-7)");
      return;
    }
    printStatus(device.setAveraging(static_cast<INA3221::Averaging>(val)));
  } else if (cmd == "vbusct") {
    Serial.printf("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  } else if (cmd.startsWith("vbusct ")) {
    int val = cmd.substring(7).toInt();
    if (val < 0 || val > 7) {
      LOGW("Invalid conv time (0-7)");
      return;
    }
    printStatus(device.setVBusConvTime(static_cast<INA3221::ConvTime>(val)));
  } else if (cmd == "vshct") {
    Serial.printf("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  } else if (cmd.startsWith("vshct ")) {
    int val = cmd.substring(6).toInt();
    if (val < 0 || val > 7) {
      LOGW("Invalid conv time (0-7)");
      return;
    }
    printStatus(device.setVShuntConvTime(static_cast<INA3221::ConvTime>(val)));
  } else if (cmd.startsWith("chen ")) {
    String args = cmd.substring(5);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: chen <1|2|3> <0|1>");
      return;
    }
    int ch = parseChannel(args.substring(0, split));
    int en = args.substring(split + 1).toInt();
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    printStatus(device.setChannelEnable(static_cast<INA3221::Channel>(ch), en != 0));
  } else if (cmd.startsWith("rshunt ")) {
    String args = cmd.substring(7);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: rshunt <1|2|3> <ohms>");
      return;
    }
    int ch = parseChannel(args.substring(0, split));
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    float ohms = 0.0f;
    String ohmStr = args.substring(split + 1);
    ohmStr.trim();
    if (!parseFloat(ohmStr, ohms) || ohms <= 0.0f) {
      LOGW("Invalid resistance (must be > 0)");
      return;
    }
    printStatus(device.setShuntResistance(static_cast<INA3221::Channel>(ch), ohms));
  } else if (cmd.startsWith("config write ")) {
    uint32_t value = 0;
    String token = cmd.substring(13);
    token.trim();
    if (!parseU32(token, value) || value > 0xFFFFu) {
      LOGW("Usage: config write <0..0xFFFF>");
      return;
    }
    INA3221::Status st = device.writeConfig(static_cast<uint16_t>(value));
    printStatus(st);
    if (st.ok()) {
      printConfig();
    }
  } else if (cmd == "config" || cmd == "cfg" || cmd == "settings") {
    printConfig();
  } else if (cmd == "reset") {
    LOGI("Performing software reset...");
    auto st = device.softReset();
    printStatus(st);
  } else if (cmd == "alerts") {
    INA3221::AlertFlags flags;
    auto st = device.readAlertFlags(flags);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    Serial.println("=== Alert Flags ===");
    Serial.printf("  Critical: CH1=%d  CH2=%d  CH3=%d\n",
                  flags.criticalCh1, flags.criticalCh2, flags.criticalCh3);
    Serial.printf("  Warning:  CH1=%d  CH2=%d  CH3=%d\n",
                  flags.warningCh1, flags.warningCh2, flags.warningCh3);
    Serial.printf("  Summation=%d  PowerValid=%d  TimingCtl=%d  ConvReady=%d\n",
                  flags.summation, flags.powerValid, flags.timingControl, flags.conversionReady);
  } else if (cmd.startsWith("crit ")) {
    String args = cmd.substring(5);
    args.trim();
    int split = args.indexOf(' ');
    int ch;
    if (split < 0) {
      ch = parseChannel(args);
      if (ch < 0) {
        LOGW("Invalid channel (1-3)");
        return;
      }
      int16_t raw = 0;
      auto st = device.getCriticalAlertLimit(static_cast<INA3221::Channel>(ch), raw);
      if (st.ok()) {
        Serial.printf("  CH%d critical limit: %d (%.3f mV)\n",
                      ch + 1, raw,
                      static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)));
      } else {
        printStatus(st);
      }
    } else {
      ch = parseChannel(args.substring(0, split));
      if (ch < 0) {
        LOGW("Invalid channel (1-3)");
        return;
      }
      int32_t raw = 0;
      if (!parseI32(args.substring(split + 1), raw) || raw < -32768 || raw > 32767) {
        LOGW("Invalid raw value (int16 range)");
        return;
      }
      printStatus(device.setCriticalAlertLimit(static_cast<INA3221::Channel>(ch), static_cast<int16_t>(raw)));
    }
  } else if (cmd.startsWith("warn ")) {
    String args = cmd.substring(5);
    args.trim();
    int split = args.indexOf(' ');
    int ch;
    if (split < 0) {
      ch = parseChannel(args);
      if (ch < 0) {
        LOGW("Invalid channel (1-3)");
        return;
      }
      int16_t raw = 0;
      auto st = device.getWarningAlertLimit(static_cast<INA3221::Channel>(ch), raw);
      if (st.ok()) {
        Serial.printf("  CH%d warning limit: %d (%.3f mV)\n",
                      ch + 1, raw,
                      static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)));
      } else {
        printStatus(st);
      }
    } else {
      ch = parseChannel(args.substring(0, split));
      if (ch < 0) {
        LOGW("Invalid channel (1-3)");
        return;
      }
      int32_t raw = 0;
      if (!parseI32(args.substring(split + 1), raw) || raw < -32768 || raw > 32767) {
        LOGW("Invalid raw value (int16 range)");
        return;
      }
      printStatus(device.setWarningAlertLimit(static_cast<INA3221::Channel>(ch), static_cast<int16_t>(raw)));
    }
  } else if (cmd == "sumlim") {
    int16_t raw = 0;
    auto st = device.getShuntSumLimit(raw);
    if (st.ok()) {
      Serial.printf("  Shunt sum limit: %d\n", raw);
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("sumlim ")) {
    int32_t raw = 0;
    if (!parseI32(cmd.substring(7), raw) || raw < -32768 || raw > 32767) {
      LOGW("Invalid raw value (int16 range)");
      return;
    }
    printStatus(device.setShuntSumLimit(static_cast<int16_t>(raw)));
  } else if (cmd == "pvhi") {
    int16_t raw = 0;
    auto st = device.getPowerValidUpperLimit(raw);
    if (st.ok()) {
      Serial.printf("  Power valid upper limit: %d (%.3f V)\n", raw,
                    static_cast<double>(INA3221::INA3221::busRawToVolts(raw)));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("pvhi ")) {
    int32_t raw = 0;
    if (!parseI32(cmd.substring(5), raw) || raw < -32768 || raw > 32767) {
      LOGW("Invalid raw value (int16 range)");
      return;
    }
    printStatus(device.setPowerValidUpperLimit(static_cast<int16_t>(raw)));
  } else if (cmd == "pvlo") {
    int16_t raw = 0;
    auto st = device.getPowerValidLowerLimit(raw);
    if (st.ok()) {
      Serial.printf("  Power valid lower limit: %d (%.3f V)\n", raw,
                    static_cast<double>(INA3221::INA3221::busRawToVolts(raw)));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("pvlo ")) {
    int32_t raw = 0;
    if (!parseI32(cmd.substring(5), raw) || raw < -32768 || raw > 32767) {
      LOGW("Invalid raw value (int16 range)");
      return;
    }
    printStatus(device.setPowerValidLowerLimit(static_cast<int16_t>(raw)));
  } else if (cmd.startsWith("sumch ")) {
    String args = cmd.substring(6);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: sumch <1|2|3> <0|1>");
      return;
    }
    int ch = parseChannel(args.substring(0, split));
    int en = args.substring(split + 1).toInt();
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    // Read-modify: enable/disable one channel in summation
    // setSummationChannels expects all three at once, so we keep a simple approach
    // For simplicity, set the requested channel and keep others as-is via a single call pattern.
    // NOTE: this always sets all three; we pass the requested change.
    bool ch1en = (ch == 0) ? (en != 0) : true;
    bool ch2en = (ch == 1) ? (en != 0) : true;
    bool ch3en = (ch == 2) ? (en != 0) : true;
    printStatus(device.setSummationChannels(ch1en, ch2en, ch3en));
  } else if (cmd.startsWith("latch ")) {
    String args = cmd.substring(6);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: latch <warn 0|1> <crit 0|1>");
      return;
    }
    int warn = args.substring(0, split).toInt();
    int crit = args.substring(split + 1).toInt();
    printStatus(device.setAlertLatchEnable(warn != 0, crit != 0));
  } else if (cmd == "online") {
    bool online = device.isOnline();
    LOGI("Online: %s%s%s", yesNoColor(online), online ? "YES" : "NO", LOG_COLOR_RESET);
  } else if (cmd == "selftest") {
    runSelfTest();
  } else if (cmd == "stress_mix") {
    runStressMix(50);
  } else if (cmd.startsWith("stress_mix ")) {
    int count = cmd.substring(11).toInt();
    if (count <= 0 || count > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    runStressMix(count);
  } else if (cmd.startsWith("stress")) {
    int count = 10;
    if (cmd.length() > 6) {
      count = cmd.substring(7).toInt();
    }
    if (count <= 0 || count > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    int ok = 0;
    int fail = 0;
    bool hasFailure = false;
    INA3221::Status firstFailure = INA3221::Status::Ok();
    INA3221::Status lastFailure = INA3221::Status::Ok();
    for (int i = 0; i < count; ++i) {
      INA3221::ChannelMeasurement ch1, ch2, ch3;
      auto st = device.readBlocking(&ch1, &ch2, &ch3);
      if (st.ok()) {
        ok++;
        LOGV(verboseMode, "  %d: CH1=%.1fmA CH2=%.1fmA CH3=%.1fmA",
             i + 1,
             static_cast<double>(ch1.current_mA),
             static_cast<double>(ch2.current_mA),
             static_cast<double>(ch3.current_mA));
      } else {
        fail++;
        if (!hasFailure) {
          firstFailure = st;
          hasFailure = true;
        }
        lastFailure = st;
        if (verboseMode) {
          printStatus(st);
        }
      }
    }
    const float pct = (count > 0) ? (100.0f * static_cast<float>(ok) / static_cast<float>(count)) : 0.0f;
    Serial.printf("  Stress results: %s%d ok%s, %s%d failed%s (%s%.2f%%%s)\n",
                  goodIfNonZeroColor(static_cast<uint32_t>(ok)),
                  ok,
                  LOG_COLOR_RESET,
                  goodIfZeroColor(static_cast<uint32_t>(fail)),
                  fail,
                  LOG_COLOR_RESET,
                  successRateColor(pct),
                  pct,
                  LOG_COLOR_RESET);
    if (hasFailure) {
      Serial.println("  Failure details:");
      Serial.println("  First failure:");
      printStatus(firstFailure);
      if (fail > 1) {
        Serial.println("  Last failure:");
        printStatus(lastFailure);
      }
    }
  } else if (cmd.startsWith("convert shunt ")) {
    int32_t raw = 0;
    if (!parseI32(cmd.substring(14), raw) || raw < -32768 || raw > 32767) {
      LOGW("Invalid raw value (int16 range)");
      return;
    }
    Serial.printf("  Shunt raw %d = %.3f mV\n", static_cast<int>(raw),
                  static_cast<double>(INA3221::INA3221::shuntRawToMv(static_cast<int16_t>(raw))));
  } else if (cmd.startsWith("convert bus ")) {
    int32_t raw = 0;
    if (!parseI32(cmd.substring(12), raw) || raw < -32768 || raw > 32767) {
      LOGW("Invalid raw value (int16 range)");
      return;
    }
    Serial.printf("  Bus raw %d = %.3f V\n", static_cast<int>(raw),
                  static_cast<double>(INA3221::INA3221::busRawToVolts(static_cast<int16_t>(raw))));
  } else {
    LOGW("Unknown command: %s", cmd.c_str());
  }
}

// ============================================================================
// Setup and Loop
// ============================================================================

void setup() {
  board::initSerial();
  delay(100);

  LOGI("=== INA3221 Bringup Example ===");

  if (!board::initI2c()) {
    LOGE("Failed to initialize I2C");
    return;
  }
  LOGI("I2C initialized (SDA=%d, SCL=%d)", board::I2C_SDA, board::I2C_SCL);

  bus_diag::scan();

  INA3221::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.i2cAddress = 0x40;
  cfg.i2cTimeoutMs = board::I2C_TIMEOUT_MS;
  cfg.offlineThreshold = 5;
  cfg.shuntResistance[0] = 0.1f;
  cfg.shuntResistance[1] = 0.1f;
  cfg.shuntResistance[2] = 0.1f;

  auto st = device.begin(cfg);
  if (!st.ok()) {
    LOGE("Failed to initialize device");
    printStatus(st);
    return;
  }

  LOGI("Device initialized successfully");
  printDriverHealth();

  Serial.println("\nType 'help' for commands");
  Serial.print("> ");
}

void loop() {
  device.tick(millis());

  static String inputBuffer;
  static constexpr size_t kMaxInputLen = 128;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
        Serial.print("> ");
      }
    } else if (inputBuffer.length() < kMaxInputLen) {
      inputBuffer += c;
    }
  }
}
