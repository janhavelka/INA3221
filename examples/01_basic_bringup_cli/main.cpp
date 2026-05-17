/// @file main.cpp
/// @brief INA3221 basic bringup example
/// @note This is an EXAMPLE, not part of the library

#include <Arduino.h>
#include <cstdlib>
#include <limits>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/CliStyle.h"
#include "examples/common/Log.h"

#include "INA3221/INA3221.h"

// ============================================================================
// Globals
// ============================================================================

INA3221::INA3221 device;
bool verboseMode = false;

struct ChannelStressStats {
  bool enabled = false;
  bool hasSample = false;
  float minVshuntMv = 0.0f;
  float maxVshuntMv = 0.0f;
  float minVbusV = 0.0f;
  float maxVbusV = 0.0f;
  float minCurrentMa = 0.0f;
  float maxCurrentMa = 0.0f;
  float minPowerMw = 0.0f;
  float maxPowerMw = 0.0f;
  double sumVshuntMv = 0.0;
  double sumVbusV = 0.0;
  double sumCurrentMa = 0.0;
  double sumPowerMw = 0.0;
};

struct StressStats {
  bool active = false;
  uint32_t startMs = 0;
  uint32_t endMs = 0;
  int target = 0;
  int attempts = 0;
  int success = 0;
  uint32_t errors = 0;
  ChannelStressStats channels[3];
  INA3221::Status lastError = INA3221::Status::Ok();
};

StressStats stressStats;

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
    case Err::I2C_NACK_ADDR:            return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA:            return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT:              return "I2C_TIMEOUT";
    case Err::I2C_BUS:                  return "I2C_BUS";
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

void printPrompt() {
  cli::printPrompt();
  Serial.flush();
}

void resetStressStats(int target) {
  stressStats.active = true;
  stressStats.startMs = millis();
  stressStats.endMs = 0;
  stressStats.target = target;
  stressStats.attempts = 0;
  stressStats.success = 0;
  stressStats.errors = 0;
  stressStats.lastError = INA3221::Status::Ok();

  for (int ch = 0; ch < 3; ++ch) {
    ChannelStressStats& stats = stressStats.channels[ch];
    stats.enabled = device.getChannelEnable(static_cast<INA3221::Channel>(ch));
    stats.hasSample = false;
    stats.minVshuntMv = std::numeric_limits<float>::max();
    stats.maxVshuntMv = std::numeric_limits<float>::lowest();
    stats.minVbusV = std::numeric_limits<float>::max();
    stats.maxVbusV = std::numeric_limits<float>::lowest();
    stats.minCurrentMa = std::numeric_limits<float>::max();
    stats.maxCurrentMa = std::numeric_limits<float>::lowest();
    stats.minPowerMw = std::numeric_limits<float>::max();
    stats.maxPowerMw = std::numeric_limits<float>::lowest();
    stats.sumVshuntMv = 0.0;
    stats.sumVbusV = 0.0;
    stats.sumCurrentMa = 0.0;
    stats.sumPowerMw = 0.0;
  }
}

void noteStressError(const INA3221::Status& st) {
  stressStats.errors++;
  stressStats.lastError = st;
}

void updateChannelStressStats(ChannelStressStats& stats,
                              const INA3221::ChannelMeasurement& measurement) {
  if (!stats.hasSample) {
    stats.minVshuntMv = measurement.shuntVoltage_mV;
    stats.maxVshuntMv = measurement.shuntVoltage_mV;
    stats.minVbusV = measurement.busVoltage_V;
    stats.maxVbusV = measurement.busVoltage_V;
    stats.minCurrentMa = measurement.current_mA;
    stats.maxCurrentMa = measurement.current_mA;
    stats.minPowerMw = measurement.power_mW;
    stats.maxPowerMw = measurement.power_mW;
    stats.hasSample = true;
  } else {
    if (measurement.shuntVoltage_mV < stats.minVshuntMv) stats.minVshuntMv = measurement.shuntVoltage_mV;
    if (measurement.shuntVoltage_mV > stats.maxVshuntMv) stats.maxVshuntMv = measurement.shuntVoltage_mV;
    if (measurement.busVoltage_V < stats.minVbusV) stats.minVbusV = measurement.busVoltage_V;
    if (measurement.busVoltage_V > stats.maxVbusV) stats.maxVbusV = measurement.busVoltage_V;
    if (measurement.current_mA < stats.minCurrentMa) stats.minCurrentMa = measurement.current_mA;
    if (measurement.current_mA > stats.maxCurrentMa) stats.maxCurrentMa = measurement.current_mA;
    if (measurement.power_mW < stats.minPowerMw) stats.minPowerMw = measurement.power_mW;
    if (measurement.power_mW > stats.maxPowerMw) stats.maxPowerMw = measurement.power_mW;
  }

  stats.sumVshuntMv += measurement.shuntVoltage_mV;
  stats.sumVbusV += measurement.busVoltage_V;
  stats.sumCurrentMa += measurement.current_mA;
  stats.sumPowerMw += measurement.power_mW;
}

void updateStressStats(const INA3221::ChannelMeasurement* ch1,
                       const INA3221::ChannelMeasurement* ch2,
                       const INA3221::ChannelMeasurement* ch3) {
  const INA3221::ChannelMeasurement* measurements[3] = {ch1, ch2, ch3};
  for (int ch = 0; ch < 3; ++ch) {
    if (measurements[ch] == nullptr) {
      continue;
    }
    updateChannelStressStats(stressStats.channels[ch], *measurements[ch]);
  }
  stressStats.success++;
}

void printStressChannelSummary(int chNum, const ChannelStressStats& stats, int successCount) {
  if (!stats.enabled) {
    Serial.printf("  CH%d: disabled\n", chNum);
    return;
  }
  if (!stats.hasSample || successCount <= 0) {
    Serial.printf("  CH%d: no valid samples\n", chNum);
    return;
  }

  const float avgVshunt = static_cast<float>(stats.sumVshuntMv / successCount);
  const float avgVbus = static_cast<float>(stats.sumVbusV / successCount);
  const float avgCurrent = static_cast<float>(stats.sumCurrentMa / successCount);
  const float avgPower = static_cast<float>(stats.sumPowerMw / successCount);

  Serial.printf("  CH%d Vshunt mV: min=%.3f avg=%.3f max=%.3f\n",
                chNum, stats.minVshuntMv, avgVshunt, stats.maxVshuntMv);
  Serial.printf("  CH%d Vbus V:    min=%.3f avg=%.3f max=%.3f\n",
                chNum, stats.minVbusV, avgVbus, stats.maxVbusV);
  Serial.printf("  CH%d Current mA:min=%.3f avg=%.3f max=%.3f\n",
                chNum, stats.minCurrentMa, avgCurrent, stats.maxCurrentMa);
  Serial.printf("  CH%d Power mW:  min=%.3f avg=%.3f max=%.3f\n",
                chNum, stats.minPowerMw, avgPower, stats.maxPowerMw);
}

void finishStressStats() {
  stressStats.active = false;
  stressStats.endMs = millis();
  const uint32_t durationMs = stressStats.endMs - stressStats.startMs;

  Serial.println("=== Stress Summary ===");
  Serial.printf("  Target: %d\n", stressStats.target);
  Serial.printf("  Attempts: %d\n", stressStats.attempts);
  Serial.printf("  Success: %s%d%s\n",
                goodIfNonZeroColor(static_cast<uint32_t>(stressStats.success)),
                stressStats.success,
                LOG_COLOR_RESET);
  Serial.printf("  Errors: %s%lu%s\n",
                goodIfZeroColor(stressStats.errors),
                static_cast<unsigned long>(stressStats.errors),
                LOG_COLOR_RESET);
  Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(durationMs));
  if (durationMs > 0U) {
    const float rate = 1000.0f * static_cast<float>(stressStats.attempts) /
                       static_cast<float>(durationMs);
    Serial.printf("  Rate: %.2f samples/s\n", rate);
  }

  for (int ch = 0; ch < 3; ++ch) {
    printStressChannelSummary(ch + 1, stressStats.channels[ch], stressStats.success);
  }

  if (!stressStats.lastError.ok()) {
    Serial.printf("  Last error: %s\n", errToStr(stressStats.lastError.code));
    if (stressStats.lastError.msg && stressStats.lastError.msg[0]) {
      Serial.printf("  Message: %s\n", stressStats.lastError.msg);
    }
  }
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
  Serial.println();
  cli::printHelpHeader("INA3221 CLI Help");
  cli::printHelpSection("Common");
  cli::printHelpItem("help / ?", "Show this help");
  cli::printHelpItem("version / ver", "Print firmware and library version info");
  cli::printHelpItem("scan", "Scan I2C bus");
  cli::printHelpItem("read", "Read all enabled channels (blocking)");
  cli::printHelpItem("read N", "Read N measurements");
  cli::printHelpItem("ch <1|2|3>", "Read single channel");
  cli::printHelpItem("shunt <1|2|3>", "Read shunt voltage (mV)");
  cli::printHelpItem("bus <1|2|3>", "Read bus voltage (V)");
  cli::printHelpItem("current <1|2|3>", "Read current (mA)");
  cli::printHelpItem("power <1|2|3>", "Read power (mW)");
  cli::printHelpItem("sum", "Read shunt-voltage sum (mV)");
  cli::printHelpItem("shuntraw <1|2|3>", "Read raw shunt register value");
  cli::printHelpItem("busraw <1|2|3>", "Read raw bus register value");
  cli::printHelpItem("sumraw", "Read raw shunt sum register value");
  cli::printHelpItem("ids", "Read manufacturer and die IDs");
  cli::printHelpItem("timing", "Show conversion time and cycle timing");
  cli::printHelpItem("start", "Start single-shot conversion");
  cli::printHelpItem("start <mode>", "Start with triggered mode (strig/btrig/sbtrig)");
  cli::printHelpItem("poll", "Check if conversion is ready");

  cli::printHelpSection("Configuration");
  cli::printHelpItem("mode [pd|strig|btrig|sbtrig|sc|bc|sbc]", "Set/show operating mode");
  cli::printHelpItem("avg [0..7]", "Set/show averaging (0=1,...,7=1024)");
  cli::printHelpItem("vbusct [0..7]", "Set/show bus voltage conv time");
  cli::printHelpItem("vshct [0..7]", "Set/show shunt voltage conv time");
  cli::printHelpItem("chen [<1|2|3> <0|1>]", "Show or set channel enable");
  cli::printHelpItem("rshunt [<1|2|3> <ohms>]", "Show or set shunt resistance");
  cli::printHelpItem("config", "Dump config register");
  cli::printHelpItem("config write <hex>", "Write full config register");
  cli::printHelpItem("reset", "Software reset");

  cli::printHelpSection("Registers");
  cli::printHelpItem("reg <addr>", "Read 16-bit register (hex address)");
  cli::printHelpItem("wreg <addr> <val>", "Write 16-bit register (diagnostic only; may desync cached config)");

  cli::printHelpSection("Alerts");
  cli::printHelpItem("alerts", "Read alert flags");
  cli::printHelpItem("mask", "Read/decode Mask/Enable register");
  cli::printHelpItem("crit [<1|2|3> [raw]]", "Show or set critical alert limit");
  cli::printHelpItem("warn [<1|2|3> [raw]]", "Show or set warning alert limit");
  cli::printHelpItem("sumlim [raw]", "Get/set shunt sum limit");
  cli::printHelpItem("pvhi [raw]", "Get/set power valid upper limit");
  cli::printHelpItem("pvlo [raw]", "Get/set power valid lower limit");
  cli::printHelpItem("sumch [<1|2|3> <0|1>]", "Show or set summation channels");
  cli::printHelpItem("latch [<warn> <crit>]", "Show or set alert latch enable");

  cli::printHelpSection("Diagnostics");
  cli::printHelpItem("drv", "Show driver state and health");
  cli::printHelpItem("probe", "Probe device (no health tracking)");
  cli::printHelpItem("recover", "Manual recovery attempt");
  cli::printHelpItem("online", "Show online state");
  cli::printHelpItem("cfg / settings", "Print active configuration snapshot");
  cli::printHelpItem("verbose [0|1]", "Enable/disable verbose output");
  cli::printHelpItem("stress [N]", "Run N measurement cycles");
  cli::printHelpItem("stress_mix [N]", "Run N mixed-operation stress cycles");
  cli::printHelpItem("selftest", "Run safe command self-test report");
  cli::printHelpItem("convert shunt <raw>", "Convert shunt raw to mV");
  cli::printHelpItem("convert bus <raw>", "Convert bus raw to V");
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

bool parseBool01(const String& token, bool& out) {
  String t = token;
  t.trim();
  if (t == "0") {
    out = false;
    return true;
  }
  if (t == "1") {
    out = true;
    return true;
  }
  return false;
}

int parseChannel(const String& token) {
  int32_t ch = 0;
  if (!parseI32(token, ch)) {
    return -1;
  }
  if (ch < 1 || ch > 3) {
    return -1;
  }
  return static_cast<int>(ch - 1);  // Convert to 0-based
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
  Serial.flush();
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

void printChannelEnable() {
  Serial.printf("  Channels: CH1=%s  CH2=%s  CH3=%s\n",
                device.getChannelEnable(INA3221::Channel::CH1) ? "ON" : "OFF",
                device.getChannelEnable(INA3221::Channel::CH2) ? "ON" : "OFF",
                device.getChannelEnable(INA3221::Channel::CH3) ? "ON" : "OFF");
}

void printShuntResistance() {
  Serial.printf("  Rshunt: CH1=%.4f  CH2=%.4f  CH3=%.4f ohm\n",
                static_cast<double>(device.getShuntResistance(INA3221::Channel::CH1)),
                static_cast<double>(device.getShuntResistance(INA3221::Channel::CH2)),
                static_cast<double>(device.getShuntResistance(INA3221::Channel::CH3)));
}

void printCriticalAlertLimit(INA3221::Channel ch, int chNum) {
  int16_t raw = 0;
  INA3221::Status st = device.getCriticalAlertLimit(ch, raw);
  if (st.ok()) {
    Serial.printf("  CH%d critical limit: %d (%.3f mV)\n",
                  chNum, raw,
                  static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)));
  } else {
    printStatus(st);
  }
}

void printWarningAlertLimit(INA3221::Channel ch, int chNum) {
  int16_t raw = 0;
  INA3221::Status st = device.getWarningAlertLimit(ch, raw);
  if (st.ok()) {
    Serial.printf("  CH%d warning limit: %d (%.3f mV)\n",
                  chNum, raw,
                  static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)));
  } else {
    printStatus(st);
  }
}

void printCriticalAlertLimits() {
  printCriticalAlertLimit(INA3221::Channel::CH1, 1);
  printCriticalAlertLimit(INA3221::Channel::CH2, 2);
  printCriticalAlertLimit(INA3221::Channel::CH3, 3);
}

void printWarningAlertLimits() {
  printWarningAlertLimit(INA3221::Channel::CH1, 1);
  printWarningAlertLimit(INA3221::Channel::CH2, 2);
  printWarningAlertLimit(INA3221::Channel::CH3, 3);
}

void printSettingsSnapshot() {
  INA3221::SettingsSnapshot snap;
  INA3221::Status st = device.getSettings(snap);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== Cached Settings ===");
  Serial.printf("  Initialized: %s\n", snap.initialized ? "YES" : "NO");
  Serial.printf("  State: %s\n", stateToStr(snap.state));
  Serial.printf("  Address: 0x%02X\n", snap.i2cAddress);
  Serial.printf("  I2C timeout: %lu ms\n", static_cast<unsigned long>(snap.i2cTimeoutMs));
  Serial.printf("  Offline threshold: %u\n", static_cast<unsigned>(snap.offlineThreshold));
  Serial.printf("  Hooks: nowMs=%s yield=%s\n",
                snap.hasNowMsHook ? "YES" : "NO",
                snap.hasCooperativeYieldHook ? "YES" : "NO");
  Serial.printf("  Mode: %s\n", modeToStr(snap.mode));
  Serial.printf("  Averaging: %s samples\n", avgToStr(snap.averaging));
  Serial.printf("  VbusCT: %s\n", ctToStr(snap.vBusCt));
  Serial.printf("  VshCT: %s\n", ctToStr(snap.vShCt));
  Serial.printf("  Channels: CH1=%s  CH2=%s  CH3=%s\n",
                snap.ch1Enable ? "ON" : "OFF",
                snap.ch2Enable ? "ON" : "OFF",
                snap.ch3Enable ? "ON" : "OFF");
  Serial.printf("  Rshunt: CH1=%.4f  CH2=%.4f  CH3=%.4f ohm\n",
                static_cast<double>(snap.shuntResistance[0]),
                static_cast<double>(snap.shuntResistance[1]),
                static_cast<double>(snap.shuntResistance[2]));
  Serial.printf("  Conversion: started=%s ready=%s start=%lu ms\n",
                snap.conversionStarted ? "YES" : "NO",
                snap.conversionReady ? "YES" : "NO",
                static_cast<unsigned long>(snap.conversionStartMs));
  Serial.printf("  Mask/Enable writable cache: 0x%04X\n", snap.maskEnableWritableCache);
  Serial.printf("  Cycle time: %lu us\n", static_cast<unsigned long>(device.getCycleTimeUs()));
}

void printMaskEnable() {
  uint16_t raw = 0;
  INA3221::Status st = device.readRegister16(INA3221::cmd::REG_MASK_ENABLE, raw);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== Mask/Enable Register ===");
  Serial.printf("  Raw: 0x%04X\n", raw);
  Serial.printf("  Sum channels: CH1=%s  CH2=%s  CH3=%s\n",
                (raw & INA3221::cmd::MASK_SCC1) ? "ON" : "OFF",
                (raw & INA3221::cmd::MASK_SCC2) ? "ON" : "OFF",
                (raw & INA3221::cmd::MASK_SCC3) ? "ON" : "OFF");
  Serial.printf("  Latch: warning=%s  critical=%s\n",
                (raw & INA3221::cmd::MASK_WEN) ? "ON" : "OFF",
                (raw & INA3221::cmd::MASK_CEN) ? "ON" : "OFF");
  Serial.printf("  Critical flags: CH1=%d  CH2=%d  CH3=%d\n",
                (raw & INA3221::cmd::MASK_CF1) != 0,
                (raw & INA3221::cmd::MASK_CF2) != 0,
                (raw & INA3221::cmd::MASK_CF3) != 0);
  Serial.printf("  Warning flags:  CH1=%d  CH2=%d  CH3=%d\n",
                (raw & INA3221::cmd::MASK_WF1) != 0,
                (raw & INA3221::cmd::MASK_WF2) != 0,
                (raw & INA3221::cmd::MASK_WF3) != 0);
  Serial.printf("  SF=%d  PVF=%d  TCF=%d  CVRF=%d\n",
                (raw & INA3221::cmd::MASK_SF) != 0,
                (raw & INA3221::cmd::MASK_PVF) != 0,
                (raw & INA3221::cmd::MASK_TCF) != 0,
                (raw & INA3221::cmd::MASK_CVRF) != 0);
  Serial.println("  Note: reading this register clears latched alert and conversion-ready flags.");
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
  struct OpStats {
    const char* name;
    uint32_t ok;
    uint32_t fail;
  };
  OpStats stats[] = {
      {"readBlocking", 0, 0},
      {"readConfig",   0, 0},
      {"mfgId",        0, 0},
      {"shuntCh1",     0, 0},
      {"busCh2",       0, 0},
      {"alerts",       0, 0},
  };
  const int opCount = static_cast<int>(sizeof(stats) / sizeof(stats[0]));
  const uint32_t successBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint32_t startMs = millis();

  for (int i = 0; i < count; ++i) {
    // readBlocking all channels
    INA3221::ChannelMeasurement ch1, ch2, ch3;
    INA3221::ChannelMeasurement* p1 = device.getChannelEnable(INA3221::Channel::CH1) ? &ch1 : nullptr;
    INA3221::ChannelMeasurement* p2 = device.getChannelEnable(INA3221::Channel::CH2) ? &ch2 : nullptr;
    INA3221::ChannelMeasurement* p3 = device.getChannelEnable(INA3221::Channel::CH3) ? &ch3 : nullptr;

    INA3221::Status st = device.readBlocking(p1, p2, p3);
    if (st.ok()) {
      stats[0].ok++;
    } else {
      stats[0].fail++;
      if (verboseMode) {
        Serial.printf("  [%d] %s failed: %s\n", i, stats[0].name, errToStr(st.code));
      }
    }

    // Read config register
    uint16_t cfg = 0;
    st = device.readConfig(cfg);
    if (st.ok()) {
      stats[1].ok++;
    } else {
      stats[1].fail++;
      if (verboseMode) {
        Serial.printf("  [%d] %s failed: %s\n", i, stats[1].name, errToStr(st.code));
      }
    }

    // Read manufacturer ID
    uint16_t mfgId = 0;
    st = device.readManufacturerId(mfgId);
    if (st.ok()) {
      stats[2].ok++;
    } else {
      stats[2].fail++;
      if (verboseMode) {
        Serial.printf("  [%d] %s failed: %s\n", i, stats[2].name, errToStr(st.code));
      }
    }

    // Read individual channel shunt voltage
    float shuntMv = 0.0f;
    st = device.readShuntVoltage(INA3221::Channel::CH1, shuntMv);
    if (st.ok()) {
      stats[3].ok++;
    } else {
      stats[3].fail++;
      if (verboseMode) {
        Serial.printf("  [%d] %s failed: %s\n", i, stats[3].name, errToStr(st.code));
      }
    }

    // Read individual channel bus voltage
    float busV = 0.0f;
    st = device.readBusVoltage(INA3221::Channel::CH2, busV);
    if (st.ok()) {
      stats[4].ok++;
    } else {
      stats[4].fail++;
      if (verboseMode) {
        Serial.printf("  [%d] %s failed: %s\n", i, stats[4].name, errToStr(st.code));
      }
    }

    // Read alert flags
    INA3221::AlertFlags flags;
    st = device.readAlertFlags(flags);
    if (st.ok()) {
      stats[5].ok++;
    } else {
      stats[5].fail++;
      if (verboseMode) {
        Serial.printf("  [%d] %s failed: %s\n", i, stats[5].name, errToStr(st.code));
      }
    }
  }

  const uint32_t elapsed = millis() - startMs;
  uint32_t okTotal = 0;
  uint32_t failTotal = 0;
  for (int i = 0; i < opCount; ++i) {
    okTotal += stats[i].ok;
    failTotal += stats[i].fail;
  }

  Serial.println("=== stress_mix summary ===");
  const float pct =
      ((okTotal + failTotal) > 0U)
          ? (100.0f * static_cast<float>(okTotal) /
             static_cast<float>(okTotal + failTotal))
          : 0.0f;
  Serial.printf("  Total: %sok=%lu%s %sfail=%lu%s (%s%.2f%%%s)\n",
                goodIfNonZeroColor(okTotal),
                static_cast<unsigned long>(okTotal),
                LOG_COLOR_RESET,
                goodIfZeroColor(failTotal),
                static_cast<unsigned long>(failTotal),
                LOG_COLOR_RESET,
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET);
  Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  if (elapsed > 0U) {
    const uint32_t totalOps = okTotal + failTotal;
    Serial.printf("  Rate: %.2f ops/s\n",
                  (1000.0f * static_cast<float>(totalOps)) /
                      static_cast<float>(elapsed));
  }
  for (int i = 0; i < opCount; ++i) {
    Serial.printf("  %-12s %sok=%lu%s %sfail=%lu%s\n",
                  stats[i].name,
                  goodIfNonZeroColor(stats[i].ok),
                  static_cast<unsigned long>(stats[i].ok),
                  LOG_COLOR_RESET,
                  goodIfZeroColor(stats[i].fail),
                  static_cast<unsigned long>(stats[i].fail),
                  LOG_COLOR_RESET);
  }
  const uint32_t successDelta = device.totalSuccess() - successBefore;
  const uint32_t failDelta = device.totalFailures() - failBefore;
  Serial.printf("  Health delta (tracked I2C): %ssuccess +%lu%s, %sfailures +%lu%s\n",
                goodIfNonZeroColor(successDelta),
                static_cast<unsigned long>(successDelta),
                LOG_COLOR_RESET,
                goodIfZeroColor(failDelta),
                static_cast<unsigned long>(failDelta),
                LOG_COLOR_RESET);
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
    bool val = false;
    if (!parseBool01(cmd.substring(8), val)) {
      LOGW("Invalid verbose value (0|1)");
      return;
    }
    verboseMode = val;
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
  } else if (cmd == "read") {
    readAllChannels();
  } else if (cmd.startsWith("read ")) {
    int32_t parsedCount = 0;
    if (!parseI32(cmd.substring(5), parsedCount)) {
      LOGW("Invalid count (1-10000)");
      return;
    }
    if (parsedCount <= 0 || parsedCount > 10000) {
      LOGW("Invalid count (1-10000)");
      return;
    }
    const int count = static_cast<int>(parsedCount);
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
    bool ready = false;
    auto st = device.readConversionReady(ready);
    if (st.ok()) {
      LOGI("Conversion ready: %s%s%s", yesNoColor(ready), ready ? "YES" : "NO", LOG_COLOR_RESET);
    } else {
      printStatus(st);
    }
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
    int32_t val = 0;
    if (!parseI32(cmd.substring(4), val)) {
      LOGW("Invalid avg (0-7)");
      return;
    }
    if (val < 0 || val > 7) {
      LOGW("Invalid avg (0-7)");
      return;
    }
    printStatus(device.setAveraging(static_cast<INA3221::Averaging>(val)));
  } else if (cmd == "vbusct") {
    Serial.printf("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  } else if (cmd.startsWith("vbusct ")) {
    int32_t val = 0;
    if (!parseI32(cmd.substring(7), val)) {
      LOGW("Invalid conv time (0-7)");
      return;
    }
    if (val < 0 || val > 7) {
      LOGW("Invalid conv time (0-7)");
      return;
    }
    printStatus(device.setVBusConvTime(static_cast<INA3221::ConvTime>(val)));
  } else if (cmd == "vshct") {
    Serial.printf("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  } else if (cmd.startsWith("vshct ")) {
    int32_t val = 0;
    if (!parseI32(cmd.substring(6), val)) {
      LOGW("Invalid conv time (0-7)");
      return;
    }
    if (val < 0 || val > 7) {
      LOGW("Invalid conv time (0-7)");
      return;
    }
    printStatus(device.setVShuntConvTime(static_cast<INA3221::ConvTime>(val)));
  } else if (cmd == "chen") {
    printChannelEnable();
  } else if (cmd.startsWith("chen ")) {
    String args = cmd.substring(5);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: chen <1|2|3> <0|1>");
      return;
    }
    int ch = parseChannel(args.substring(0, split));
    bool en = false;
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    if (!parseBool01(args.substring(split + 1), en)) {
      LOGW("Invalid enable value (0|1)");
      return;
    }
    printStatus(device.setChannelEnable(static_cast<INA3221::Channel>(ch), en));
  } else if (cmd == "rshunt") {
    printShuntResistance();
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
  } else if (cmd == "config") {
    printConfig();
  } else if (cmd == "cfg" || cmd == "settings") {
    printSettingsSnapshot();
  } else if (cmd == "reset") {
    LOGI("Performing software reset...");
    auto st = device.softReset();
    printStatus(st);
  } else if (cmd.startsWith("wreg ")) {
    String args = cmd.substring(5);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: wreg <addr> <val>");
      return;
    }
    uint32_t addr = 0;
    uint32_t value = 0;
    if (!parseU32(args.substring(0, split), addr) ||
        !parseU32(args.substring(split + 1), value) ||
        addr > 0xFFu || value > 0xFFFFu) {
      LOGW("Usage: wreg <addr> <val>");
      return;
    }
    printStatus(device.writeRegister16(static_cast<uint8_t>(addr), static_cast<uint16_t>(value)));
  } else if (cmd.startsWith("reg ")) {
    uint32_t addr = 0;
    if (!parseU32(cmd.substring(4), addr) || addr > 0xFFu) {
      LOGW("Usage: reg <addr>");
      return;
    }
    uint16_t value = 0;
    auto st = device.readRegister16(static_cast<uint8_t>(addr), value);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    Serial.printf("  Reg 0x%02lX = 0x%04X (%u)\n",
                  static_cast<unsigned long>(addr),
                  value,
                  value);
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
  } else if (cmd == "mask") {
    printMaskEnable();
  } else if (cmd == "crit") {
    printCriticalAlertLimits();
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
      printCriticalAlertLimit(static_cast<INA3221::Channel>(ch), ch + 1);
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
  } else if (cmd == "warn") {
    printWarningAlertLimits();
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
      printWarningAlertLimit(static_cast<INA3221::Channel>(ch), ch + 1);
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
  } else if (cmd == "sumch") {
    printMaskEnable();
  } else if (cmd.startsWith("sumch ")) {
    String args = cmd.substring(6);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: sumch <1|2|3> <0|1>");
      return;
    }
    int ch = parseChannel(args.substring(0, split));
    bool en = false;
    if (ch < 0) {
      LOGW("Invalid channel (1-3)");
      return;
    }
    if (!parseBool01(args.substring(split + 1), en)) {
      LOGW("Invalid enable value (0|1)");
      return;
    }
    uint16_t mask = 0;
    auto st = device.readRegister16(INA3221::cmd::REG_MASK_ENABLE, mask);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    bool ch1en = (mask & INA3221::cmd::MASK_SCC1) != 0;
    bool ch2en = (mask & INA3221::cmd::MASK_SCC2) != 0;
    bool ch3en = (mask & INA3221::cmd::MASK_SCC3) != 0;
    if (ch == 0) ch1en = en;
    if (ch == 1) ch2en = en;
    if (ch == 2) ch3en = en;
    printStatus(device.setSummationChannels(ch1en, ch2en, ch3en));
  } else if (cmd == "latch") {
    printMaskEnable();
  } else if (cmd.startsWith("latch ")) {
    String args = cmd.substring(6);
    args.trim();
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: latch <warn 0|1> <crit 0|1>");
      return;
    }
    bool warn = false;
    bool crit = false;
    if (!parseBool01(args.substring(0, split), warn) ||
        !parseBool01(args.substring(split + 1), crit)) {
      LOGW("Invalid latch value (0|1 0|1)");
      return;
    }
    printStatus(device.setAlertLatchEnable(warn, crit));
  } else if (cmd == "online") {
    bool online = device.isOnline();
    LOGI("Online: %s%s%s", yesNoColor(online), online ? "YES" : "NO", LOG_COLOR_RESET);
  } else if (cmd == "selftest") {
    runSelfTest();
  } else if (cmd == "stress_mix") {
    runStressMix(50);
  } else if (cmd.startsWith("stress_mix ")) {
    int32_t parsedCount = 0;
    if (!parseI32(cmd.substring(11), parsedCount)) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    if (parsedCount <= 0 || parsedCount > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    const int count = static_cast<int>(parsedCount);
    runStressMix(count);
  } else if (cmd.startsWith("stress")) {
    int32_t parsedCount = 10;
    if (cmd.length() > 6) {
      if (cmd.charAt(6) != ' ' || !parseI32(cmd.substring(7), parsedCount)) {
        LOGW("Usage: stress [N]");
        return;
      }
    }
    if (parsedCount <= 0 || parsedCount > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    const int count = static_cast<int>(parsedCount);
    resetStressStats(count);
    for (int i = 0; i < count; ++i) {
      INA3221::ChannelMeasurement ch1, ch2, ch3;
      INA3221::ChannelMeasurement* p1 = device.getChannelEnable(INA3221::Channel::CH1) ? &ch1 : nullptr;
      INA3221::ChannelMeasurement* p2 = device.getChannelEnable(INA3221::Channel::CH2) ? &ch2 : nullptr;
      INA3221::ChannelMeasurement* p3 = device.getChannelEnable(INA3221::Channel::CH3) ? &ch3 : nullptr;
      auto st = device.readBlocking(p1, p2, p3);
      stressStats.attempts++;
      if (st.ok()) {
        updateStressStats(p1, p2, p3);
        if (verboseMode) {
          Serial.printf("  [%d]", i + 1);
          if (p1) {
            Serial.printf(" CH1=%.1fmA", static_cast<double>(ch1.current_mA));
          }
          if (p2) {
            Serial.printf(" CH2=%.1fmA", static_cast<double>(ch2.current_mA));
          }
          if (p3) {
            Serial.printf(" CH3=%.1fmA", static_cast<double>(ch3.current_mA));
          }
          Serial.println();
        }
      } else {
        noteStressError(st);
        if (verboseMode) {
          printStatus(st);
        }
      }
    }
    finishStressStats();
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
  printPrompt();
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
        printPrompt();
      }
    } else if (inputBuffer.length() < kMaxInputLen) {
      inputBuffer += c;
    }
  }
}
