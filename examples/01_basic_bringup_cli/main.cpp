/// @file main.cpp
/// @brief INA3221 basic bringup example
/// @note This is an EXAMPLE, not part of the library

#include <cstdlib>
#include <cstring>
#include <limits>

#include <Arduino.h>
#include <esp_timer.h>

#include "examples/common/BoardConfig.h"
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
INA3221::Err hilCommandStatus = INA3221::Err::OK;

// Every CLI warning is a rejected command contract. Keep the framed HIL status
// authoritative even when the legacy human-readable branch only prints usage.
#undef LOGW
#define LOGW(fmt, ...) \
  do { \
    if (hilCommandStatus == INA3221::Err::OK) { \
      hilCommandStatus = INA3221::Err::INVALID_PARAM; \
    } \
    if (LOG_LEVEL >= 2) { \
      LOG_PRINT_WITH_TAG(LOG_COLOR_YELLOW, "W", fmt, ##__VA_ARGS__); \
    } \
  } while (0)

static constexpr uint8_t INA3221_ADDR_MIN = 0x40U;
static constexpr uint8_t INA3221_ADDR_MAX = 0x43U;
static constexpr uint32_t I2C_FREQ_MIN_HZ = 10000U;
static constexpr uint32_t I2C_FREQ_MAX_HZ = 400000U;
uint8_t selectedAddress = board::INA3221_I2C_ADDR;
INA3221::DeviceProfile retainedProfile{};
bool retainedProfileValid = false;
INA3221::JobResult lastOwnerResult{};
bool lastOwnerResultValid = false;

enum class OwnerDemoPhase : uint8_t {
  IDLE,
  ACTIVE
};

OwnerDemoPhase ownerDemoPhase = OwnerDemoPhase::IDLE;
bool startupSamplePending = false;
bool ownerAutoService = true;
uint32_t ownerRequestId = 1000U;
static constexpr uint64_t OWNER_JOB_TIMEOUT_MS = 1000U;

struct RegisterEvidence {
  uint8_t reg = 0;
  uint16_t expected = 0;
  uint16_t actual = 0;
  uint16_t compareMask = 0xFFFFU;
  bool readOk = false;
  INA3221::Status status = INA3221::Status::Ok();
};

struct VerificationEvidence {
  bool valid = false;
  uint32_t capturedAtMs = 0;
  uint8_t i2cAddress = 0;
  uint32_t profileGeneration = 0;
  uint8_t count = 0;
  uint8_t mismatches = 0;
  uint8_t readFailures = 0;
  RegisterEvidence registers[11]{};
};

VerificationEvidence lastVerification;

void printAlertSnapshot(const INA3221::AlertSnapshot& alerts, const char* title);

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
    case Err::JOB_BUSY:                 return "JOB_BUSY";
    case Err::RESULT_PENDING:           return "RESULT_PENDING";
    case Err::NO_RESULT:                return "NO_RESULT";
    case Err::CANCELLED:                return "CANCELLED";
    case Err::DEADLINE_EXPIRED:         return "DEADLINE_EXPIRED";
    case Err::CONFIG_UNKNOWN:           return "CONFIG_UNKNOWN";
    case Err::PROFILE_MISMATCH:         return "PROFILE_MISMATCH";
    case Err::READ_ONLY_REGISTER:       return "READ_ONLY_REGISTER";
    case Err::ARITHMETIC_OVERFLOW:      return "ARITHMETIC_OVERFLOW";
    case Err::OUT_OF_RANGE:             return "OUT_OF_RANGE";
    case Err::NO_ACTIVE_JOB:            return "NO_ACTIVE_JOB";
    case Err::JOB_KIND_MISMATCH:        return "JOB_KIND_MISMATCH";
    case Err::CONVERSION_BUSY:          return "CONVERSION_BUSY";
    case Err::DEVICE_OFFLINE:           return "DEVICE_OFFLINE";
    default:                            return "UNKNOWN";
  }
}

uint64_t ownerNowMs() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000U;
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

const char* appliedStateToStr(INA3221::AppliedConfigState state) {
  switch (state) {
    case INA3221::AppliedConfigState::UNKNOWN: return "UNKNOWN";
    case INA3221::AppliedConfigState::APPLIED: return "APPLIED";
    case INA3221::AppliedConfigState::DIRTY: return "DIRTY";
    default: return "INVALID";
  }
}

const char* jobKindToStr(INA3221::JobKind kind) {
  switch (kind) {
    case INA3221::JobKind::NONE: return "NONE";
    case INA3221::JobKind::INITIALIZE: return "INITIALIZE";
    case INA3221::JobKind::APPLY_PROFILE: return "APPLY_PROFILE";
    case INA3221::JobKind::RECONCILE: return "RECONCILE";
    case INA3221::JobKind::TRIGGERED_SAMPLE: return "TRIGGERED_SAMPLE";
    case INA3221::JobKind::CONTINUOUS_SAMPLE: return "CONTINUOUS_SAMPLE";
    case INA3221::JobKind::POWER_DOWN: return "POWER_DOWN";
    default: return "INVALID";
  }
}

const char* jobStageToStr(INA3221::JobStage stage) {
  switch (stage) {
    case INA3221::JobStage::IDLE: return "IDLE";
    case INA3221::JobStage::READ_IDENTITY: return "READ_IDENTITY";
    case INA3221::JobStage::READ_PROFILE: return "READ_PROFILE";
    case INA3221::JobStage::WRITE_PROFILE: return "WRITE_PROFILE";
    case INA3221::JobStage::VERIFY_PROFILE: return "VERIFY_PROFILE";
    case INA3221::JobStage::TRIGGER_SAMPLE: return "TRIGGER_SAMPLE";
    case INA3221::JobStage::WAIT_CONVERSION: return "WAIT_CONVERSION";
    case INA3221::JobStage::READ_ALERTS: return "READ_ALERTS";
    case INA3221::JobStage::READ_CHANNELS: return "READ_CHANNELS";
    case INA3221::JobStage::READ_POWER_STATE: return "READ_POWER_STATE";
    case INA3221::JobStage::WRITE_POWER_STATE: return "WRITE_POWER_STATE";
    case INA3221::JobStage::VERIFY_POWER_STATE: return "VERIFY_POWER_STATE";
    case INA3221::JobStage::TERMINAL: return "TERMINAL";
    default: return "INVALID";
  }
}

const char* jobStateToStr(INA3221::JobTerminalState state) {
  switch (state) {
    case INA3221::JobTerminalState::IDLE: return "IDLE";
    case INA3221::JobTerminalState::ACTIVE: return "ACTIVE";
    case INA3221::JobTerminalState::SUCCEEDED: return "SUCCEEDED";
    case INA3221::JobTerminalState::FAILED: return "FAILED";
    case INA3221::JobTerminalState::CANCELLED: return "CANCELLED";
    case INA3221::JobTerminalState::TIMED_OUT: return "TIMED_OUT";
    case INA3221::JobTerminalState::PARTIAL: return "PARTIAL";
    case INA3221::JobTerminalState::INDETERMINATE: return "INDETERMINATE";
    default: return "INVALID";
  }
}

const char* directionToStr(INA3221::CurrentDirection direction) {
  return direction == INA3221::CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT
             ? "INVERTED"
             : "NORMAL";
}

bool isValidIna3221Address(uint8_t address) {
  return address >= INA3221_ADDR_MIN && address <= INA3221_ADDR_MAX;
}

const char* stateColor(INA3221::DriverState st, bool online, uint8_t consecutiveFailures) {
  if (st == INA3221::DriverState::UNINIT) {
    return LOG_COLOR_YELLOW;
  }
  return LOG_COLOR_STATE(online, consecutiveFailures);
}

const char* staleTimeColor(bool isErrorTimestamp) {
  return isErrorTimestamp ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

void printPrompt() {
  cli::printPrompt();
#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE == 0
  Serial.flush();
#else
  // HWCDC::flush() drops its queued TX ring when its SOF-based connection
  // check transiently reports disconnected. Let the USB Serial/JTAG TX ISR
  // drain this small prompt instead of risking a rare missing prompt packet.
  yield();
#endif
}

void serviceCliOutput() {
  // Let USB CDC drain during multi-line diagnostic responses.
  Serial.flush();
  yield();
  delay(1);
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
                cli::nonZeroGoodColor(static_cast<uint32_t>(stressStats.success)),
                stressStats.success,
                LOG_COLOR_RESET);
  Serial.printf("  Errors: %s%lu%s\n",
                cli::zeroGoodColor(stressStats.errors),
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
    if (hilCommandStatus == INA3221::Err::OK) {
      hilCommandStatus = stressStats.lastError.code;
    }
    Serial.printf("  Last error: %s\n", errToStr(stressStats.lastError.code));
    Serial.printf("  Detail: %ld\n", static_cast<long>(stressStats.lastError.detail));
    if (stressStats.lastError.msg && stressStats.lastError.msg[0]) {
      Serial.printf("  Message: %s\n", stressStats.lastError.msg);
    }
  }
}

void printStatus(const INA3221::Status& st) {
  if (!st.ok() && hilCommandStatus == INA3221::Err::OK) {
    hilCommandStatus = st.code;
  }
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
                cli::zeroGoodColor(device.consecutiveFailures()),
                device.consecutiveFailures(),
                LOG_COLOR_RESET);
  Serial.printf("  Total success: %s%lu%s\n",
                cli::nonZeroGoodColor(totalOk),
                static_cast<unsigned long>(totalOk),
                LOG_COLOR_RESET);
  Serial.printf("  Total failures: %s%lu%s\n",
                cli::zeroGoodColor(totalFail),
                static_cast<unsigned long>(totalFail),
                LOG_COLOR_RESET);
  Serial.printf("  Success rate: %s%.1f%%%s\n",
                cli::successRateColor(successRate),
                successRate,
                LOG_COLOR_RESET);
  serviceCliOutput();

  const uint32_t lastOkMs = device.lastOkMs();
  if (totalOk > 0U) {
    Serial.printf("  Last OK: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_GREEN,
                  static_cast<unsigned long>(now - lastOkMs),
                  static_cast<unsigned long>(lastOkMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last OK: %snever%s\n", staleTimeColor(false), LOG_COLOR_RESET);
  }

  const uint32_t lastErrorMs = device.lastErrorMs();
  if (!lastErr.ok()) {
    Serial.printf("  Last error: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_RED,
                  static_cast<unsigned long>(now - lastErrorMs),
                  static_cast<unsigned long>(lastErrorMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last error: %snever%s\n", staleTimeColor(true), LOG_COLOR_RESET);
  }

  if (!lastErr.ok()) {
    Serial.printf("  Last error code: %s%s%s\n",
                  LOG_COLOR_RED,
                  errToStr(lastErr.code),
                  LOG_COLOR_RESET);
    Serial.printf("  Last error detail: %ld\n", static_cast<long>(lastErr.detail));
    if (lastErr.msg && lastErr.msg[0]) {
      Serial.printf("  Last error msg: %s%s%s\n", LOG_COLOR_YELLOW, lastErr.msg, LOG_COLOR_RESET);
    }
  }
  serviceCliOutput();
}

void printHelp() {
  Serial.println();
  cli::printHelpHeader("INA3221 CLI Help");
  cli::printHelpSection("Common");
  cli::printHelpItem("help / ?", "Show this help");
  cli::printHelpItem("version / ver", "Print firmware and library version info");
  cli::printHelpItem("scan", "Scan bus and probe 0x40-0x43 for INA3221 IDs");
  cli::printHelpItem("scanina", "Probe only valid INA3221 addresses and IDs");
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
  cli::printHelpItem("job / job progress", "Show cooperative owner-job progress");
  cli::printHelpItem("job init", "Start identity/profile initialization");
  cli::printHelpItem("job apply", "Apply and verify the active desired profile");
  cli::printHelpItem("job reconcile", "Reapply and verify the active profile");
  cli::printHelpItem("job sample", "Start triggered sample using active mode");
  cli::printHelpItem("job continuous [0|1]", "Start continuous sample; optionally consume alerts");
  cli::printHelpItem("job powerdown", "Start verified power-down operation");
  cli::printHelpItem("job cancel", "Cancel active job without I2C");
  cli::printHelpItem("job auto [0|1]", "Show/set automatic one-transfer polling");
  cli::printHelpItem("job step <0..255>", "Poll once with an explicit transfer budget");
  cli::printHelpItem("job result", "Show the last consumed terminal result");
  cli::printHelpItem("job lastsample", "Show the last committed fixed-unit sample");
  cli::printHelpItem("job alerts [take]", "Peek or consume retained alert evidence");

  cli::printHelpSection("Configuration");
  cli::printHelpItem("mode [pd|pda|strig|btrig|sbtrig|sc|bc|sbc]",
                     "Set/show operating mode");
  cli::printHelpItem("avg [0..7]", "Set/show averaging (0=1,...,7=1024)");
  cli::printHelpItem("vbusct [0..7]", "Set/show bus voltage conv time");
  cli::printHelpItem("vshct [0..7]", "Set/show shunt voltage conv time");
  cli::printHelpItem("chen [<1|2|3> <0|1>]", "Show or set channel enable");
  cli::printHelpItem("rshunt [<1|2|3> <ohms>]", "Show or set shunt resistance");
  cli::printHelpItem("direction [<1|2|3> <0|1>]", "Show/set current sign (0=normal, 1=inverted)");
  cli::printHelpItem("profile", "Show complete desired profile and certainty");
  cli::printHelpItem("addr [0x40..0x43]", "Show or select target INA3221 address");
  cli::printHelpItem("init [0x40..0x43]", "Bind and initialize at selected/given address");
  cli::printHelpItem("end", "Unbind driver without touching the bus");
  cli::printHelpItem("freq [10000..400000]", "Show or set I2C frequency in Hz");
  cli::printHelpItem("config", "Dump config register");
  cli::printHelpItem("config write <hex>", "Write full config register");
  cli::printHelpItem("reset", "Software reset");

  cli::printHelpSection("Registers");
  cli::printHelpItem("reg <addr>", "Read 16-bit register (hex address)");
  cli::printHelpItem("wreg <addr> <val>", "Write 16-bit register (diagnostic only; may desync cached config)");

  cli::printHelpSection("Alerts");
  cli::printHelpItem("alerts", "Read alert flags");
  cli::printHelpItem("alertsnap [take]", "Peek or consume retained alert evidence");
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
  cli::printHelpItem("diag", "Show full cache-only diagnostics and prior mismatch evidence");
  cli::printHelpItem("verify", "Read/compare every managed register (Mask read clears flags)");
  cli::printHelpItem("mismatch", "Show cached register verification evidence");
  cli::printHelpItem("probe", "Probe device (no health tracking)");
  cli::printHelpItem("recover", "Manual recovery attempt");
  cli::printHelpItem("online", "Show online state");
  cli::printHelpItem("cfg / settings", "Print active configuration snapshot");
  cli::printHelpItem("verbose [0|1]", "Enable/disable verbose output");
  cli::printHelpItem("stress [N]", "Run N measurement cycles");
  cli::printHelpItem("stress_mix [N]", "Run N mixed-operation stress cycles");
  cli::printHelpItem("stress_owner [N]", "Run N cooperative sample jobs");
  cli::printHelpItem("stress_freq [N]", "Alternate 100/400 kHz with identity reads");
  cli::printHelpItem("hilrun <token> <seq> <cmd>", "Run one framed HIL command");
  cli::printHelpItem("hilmark <token>", "Print an automation synchronization marker");
  cli::printHelpItem("xfer_reset", "Reset example transport counters");
  cli::printHelpItem("xfer_stats", "Show example transport counters");
  cli::printHelpItem("xfer_assert <r> <w> <t>", "Assert transport read/write/total counts");
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
  Serial.printf("  Arduino-ESP32 version: %s\n", ESP.getCoreVersion());
  Serial.printf("  ESP-IDF version: %s\n", ESP.getSdkVersion());
  Serial.printf("  Flash size: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()));
  Serial.printf("  PSRAM size: %lu bytes\n",
                static_cast<unsigned long>(ESP.getPsramSize()));
}

INA3221::TransportConfig makeOwnerTransport() {
  INA3221::TransportConfig transportConfig{};
  transportConfig.i2cWrite = transport::wireWrite;
  transportConfig.i2cWriteRead = transport::wireWriteRead;
  transportConfig.i2cUser = transport::configUser();
  transportConfig.nowMs = transport::arduinoNowMs;
  transportConfig.cooperativeYield = transport::arduinoYield;
  transportConfig.defaultTransferTimeoutMs = board::I2C_TIMEOUT_MS;
  transportConfig.offlineThreshold = 5U;
  return transportConfig;
}

INA3221::DeviceProfile makeOwnerProfile() {
  INA3221::DeviceProfile profile{};
  profile.i2cAddress = selectedAddress;
  profile.enabledChannels = INA3221::ALL_CHANNELS;
  profile.averaging = INA3221::Averaging::AVG_1;
  profile.vBusCt = INA3221::ConvTime::CT_1100US;
  profile.vShCt = INA3221::ConvTime::CT_1100US;
  profile.mode = INA3221::Mode::SHUNT_BUS_TRIG;
  for (INA3221::ShuntCalibration& shunt : profile.shunts) {
    shunt.resistanceMicroOhms = 100000U;
  }
  return profile;
}

void printOwnerSample(const INA3221::SampleBatch& sample) {
  Serial.printf("Owner sample request=%lu profile=%lu enabled=0x%02X valid=0x%02X "
                "coherence=%u capture=%llu ms alerts_valid=%u\n",
                static_cast<unsigned long>(sample.requestId),
                static_cast<unsigned long>(sample.profileGeneration),
                static_cast<unsigned>(sample.enabledChannels),
                static_cast<unsigned>(sample.validChannels),
                static_cast<unsigned>(sample.coherence),
                static_cast<unsigned long long>(sample.captureUptimeMs),
                sample.alertSnapshotValid ? 1U : 0U);
  for (uint8_t i = 0; i < 3U; ++i) {
    const INA3221::ChannelMask bit = static_cast<INA3221::ChannelMask>(1U << i);
    if ((sample.validChannels & bit) == 0U) continue;
    const INA3221::FixedChannelReading& reading = sample.channels[i];
    Serial.printf("  CH%u: shunt=%lduV bus=%ldmV current=%ldmA power=%ldmW quantities=0x%02X\n",
                  static_cast<unsigned>(i + 1U),
                  static_cast<long>(reading.shuntMicroVolts),
                  static_cast<long>(reading.busMilliVolts),
                  static_cast<long>(reading.currentMilliAmps),
                  static_cast<long>(reading.powerMilliWatts),
                  static_cast<unsigned>(reading.validQuantities));
  }
  if (sample.alertSnapshotValid) {
    printAlertSnapshot(sample.alerts, "Sample Alert Evidence");
  }
}

uint64_t ownerDeadlineFor(INA3221::JobKind kind,
                          const INA3221::DeviceProfile& profile,
                          INA3221::Mode sampleMode = INA3221::Mode::SHUNT_BUS_TRIG) {
  const uint64_t now = ownerNowMs();
  uint16_t transfers = 0;
  uint64_t durationMs = OWNER_JOB_TIMEOUT_MS;
  INA3221::Status st = INA3221::INA3221::maximumJobTransfers(kind, profile, transfers);
  if (st.ok()) {
    durationMs = static_cast<uint64_t>(transfers) * board::I2C_TIMEOUT_MS + 250U;
  }
  if (kind == INA3221::JobKind::TRIGGERED_SAMPLE) {
    uint64_t cycleUs = 0;
    st = INA3221::INA3221::maximumCycleTimeUs(profile, sampleMode, cycleUs);
    if (st.ok()) durationMs += (cycleUs + 999U) / 1000U + 100U;
  }
  if (durationMs < OWNER_JOB_TIMEOUT_MS) durationMs = OWNER_JOB_TIMEOUT_MS;
  return now + durationMs;
}

INA3221::Status startOwnerSample() {
  ++ownerRequestId;
  if (ownerRequestId == 0U) ++ownerRequestId;
  const INA3221::Mode mode = device.deviceProfile().mode;
  INA3221::Status st;
  if (mode == INA3221::Mode::SHUNT_TRIG ||
      mode == INA3221::Mode::BUS_TRIG ||
      mode == INA3221::Mode::SHUNT_BUS_TRIG) {
    st = device.startTriggeredSample(
        mode, ownerRequestId,
        ownerDeadlineFor(INA3221::JobKind::TRIGGERED_SAMPLE,
                         device.deviceProfile(), mode));
  } else if (mode == INA3221::Mode::SHUNT_CONT ||
             mode == INA3221::Mode::BUS_CONT ||
             mode == INA3221::Mode::SHUNT_BUS_CONT) {
    st = device.startContinuousSample(
        ownerRequestId,
        ownerDeadlineFor(INA3221::JobKind::CONTINUOUS_SAMPLE,
                         device.deviceProfile()),
        false);
  } else {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM,
                                  "Active profile is powered down");
  }
  if (st.inProgress()) ownerDemoPhase = OwnerDemoPhase::ACTIVE;
  return st;
}

INA3221::Status trackOwnerStart(const INA3221::Status& st) {
  if (st.inProgress()) {
    ownerDemoPhase = OwnerDemoPhase::ACTIVE;
    startupSamplePending = false;
    ownerAutoService = false;
  }
  return st;
}

uint32_t nextOwnerRequestId() {
  ++ownerRequestId;
  if (ownerRequestId == 0U) ++ownerRequestId;
  return ownerRequestId;
}

void printAlertSnapshot(const INA3221::AlertSnapshot& alerts, const char* title) {
  Serial.printf("=== %s ===\n", title);
  Serial.printf("  Raw: 0x%04X  events=0x%04X  writable=0x%04X\n",
                alerts.raw, alerts.events, alerts.writableBits);
  Serial.printf("  PowerValid=%u TimingControl=%u ConversionReady=%u EvidenceUncertain=%u\n",
                alerts.powerValid ? 1U : 0U,
                alerts.timingControl ? 1U : 0U,
                alerts.conversionReady ? 1U : 0U,
                alerts.evidenceUncertain ? 1U : 0U);
}

void printOwnerResult(const INA3221::JobResult& result) {
  Serial.printf("=== Owner Job Result ===\n");
  Serial.printf("  Kind: %s (%u)  State: %s (%u)\n",
                jobKindToStr(result.kind), static_cast<unsigned>(result.kind),
                jobStateToStr(result.state), static_cast<unsigned>(result.state));
  Serial.printf("  Request: %lu  Transfers: %u  Profile generation: %lu\n",
                static_cast<unsigned long>(result.requestId),
                static_cast<unsigned>(result.transfers),
                static_cast<unsigned long>(result.profileGeneration));
  Serial.printf("  Hardware effect: %u  Sample valid: %u\n",
                static_cast<unsigned>(result.hardwareEffect),
                result.sampleValid ? 1U : 0U);
  printStatus(result.status);
  if (result.mismatchValid) {
    const uint16_t delta = static_cast<uint16_t>(
        (result.mismatchActual ^ result.mismatchExpected) & result.mismatchMask);
    Serial.printf("  Register mismatch: reg=0x%02X actual=0x%04X expected=0x%04X "
                  "mask=0x%04X delta=0x%04X\n",
                  result.mismatchRegister, result.mismatchActual,
                  result.mismatchExpected, result.mismatchMask, delta);
  }
  if (result.sampleValid) printOwnerSample(result.sample);
}

void serviceOwnerJob() {
  if (ownerDemoPhase == OwnerDemoPhase::IDLE) return;

  INA3221::JobProgress progress{};
  (void)device.getJobProgress(progress);
  if (progress.state == INA3221::JobTerminalState::ACTIVE && ownerAutoService) {
    const uint64_t now = ownerNowMs();
    INA3221::PollContext context{};
    context.nowMs = now;
    context.deadlineMs = progress.deadlineMs;
    context.transferTimeoutMs = board::I2C_TIMEOUT_MS;
    context.maxTransfers = 1U;  // Normal owner-loop budget: at most one callback.
    (void)device.pollJob(context);
    (void)device.getJobProgress(progress);
  }

  if (!progress.resultPending) return;

  INA3221::JobResult result{};
  const INA3221::Status takeStatus = device.takeJobResult(result);
  if (!takeStatus.ok()) {
    printStatus(takeStatus);
    ownerDemoPhase = OwnerDemoPhase::IDLE;
    return;
  }

  lastOwnerResult = result;
  lastOwnerResultValid = true;
  printOwnerResult(result);
  if (result.status.ok() && device.isBound()) {
    retainedProfile = device.deviceProfile();
    retainedProfileValid = true;
    selectedAddress = retainedProfile.i2cAddress;
  }

  if (startupSamplePending && result.kind == INA3221::JobKind::INITIALIZE &&
      result.status.ok()) {
    startupSamplePending = false;
    const INA3221::Status sampleStatus = startOwnerSample();
    if (!sampleStatus.inProgress()) {
      printStatus(sampleStatus);
      ownerDemoPhase = OwnerDemoPhase::IDLE;
    }
    return;
  }
  startupSamplePending = false;
  ownerDemoPhase = OwnerDemoPhase::IDLE;
}

void printOwnerJobProgress() {
  INA3221::JobProgress progress{};
  const INA3221::Status st = device.getJobProgress(progress);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  const uint64_t now = ownerNowMs();
  const uint64_t remaining = progress.deadlineMs > now ? progress.deadlineMs - now : 0U;
  Serial.printf("=== Owner Job Progress ===\n");
  Serial.printf("  Kind: %s (%u)  Stage: %s (%u)  State: %s (%u)\n",
                jobKindToStr(progress.kind), static_cast<unsigned>(progress.kind),
                jobStageToStr(progress.stage), static_cast<unsigned>(progress.stage),
                jobStateToStr(progress.state), static_cast<unsigned>(progress.state));
  Serial.printf("  Request: %lu  Transfers: %u  Last poll: %u  Result pending: %u\n",
                static_cast<unsigned long>(progress.requestId),
                static_cast<unsigned>(progress.totalTransfers),
                static_cast<unsigned>(progress.lastPollTransfers),
                progress.resultPending ? 1U : 0U);
  Serial.printf("  Now: %llu  Deadline: %llu  Remaining: %llu ms  Ready at: %llu\n",
                static_cast<unsigned long long>(now),
                static_cast<unsigned long long>(progress.deadlineMs),
                static_cast<unsigned long long>(remaining),
                static_cast<unsigned long long>(progress.readyAtMs));
}

INA3221::Status stepOwnerJob(uint8_t maxTransfers) {
  INA3221::JobProgress progress{};
  INA3221::Status st = device.getJobProgress(progress);
  if (!st.ok()) return st;
  if (progress.state != INA3221::JobTerminalState::ACTIVE) {
    return INA3221::Status::Error(INA3221::Err::NO_ACTIVE_JOB,
                                  "No active owner job");
  }
  INA3221::PollContext context{};
  context.nowMs = ownerNowMs();
  context.deadlineMs = progress.deadlineMs;
  context.transferTimeoutMs = board::I2C_TIMEOUT_MS;
  context.maxTransfers = maxTransfers;
  st = device.pollJob(context);
  ownerAutoService = false;
  serviceOwnerJob();
  return st;
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

bool parseThreeU32(const String& args, uint32_t& first, uint32_t& second,
                   uint32_t& third) {
  String tail = args;
  tail.trim();
  const int firstSplit = tail.indexOf(' ');
  if (firstSplit < 0) return false;
  String secondAndThird = tail.substring(firstSplit + 1);
  secondAndThird.trim();
  const int secondSplit = secondAndThird.indexOf(' ');
  if (secondSplit < 0) return false;
  String firstToken = tail.substring(0, firstSplit);
  String secondToken = secondAndThird.substring(0, secondSplit);
  String thirdToken = secondAndThird.substring(secondSplit + 1);
  thirdToken.trim();
  return parseU32(firstToken, first) && parseU32(secondToken, second) &&
         parseU32(thirdToken, third);
}

bool parseAddress(const String& token, uint8_t& address) {
  uint32_t value = 0;
  String normalized = token;
  normalized.trim();
  if (!parseU32(normalized, value) || value > 0xFFU ||
      !isValidIna3221Address(static_cast<uint8_t>(value))) {
    return false;
  }
  address = static_cast<uint8_t>(value);
  return true;
}

const char* addressStrap(uint8_t address) {
  switch (address) {
    case 0x40U: return "A0=GND";
    case 0x41U: return "A0=VS";
    case 0x42U: return "A0=SDA";
    case 0x43U: return "A0=SCL";
    default: return "invalid";
  }
}

INA3221::Status readRegisterAt(uint8_t address, uint8_t reg, uint16_t& value) {
  uint8_t rx[2] = {0, 0};
  const INA3221::Status st = transport::wireWriteReadAt(
      address, &reg, 1U, rx, sizeof(rx), board::I2C_TIMEOUT_MS);
  if (!st.ok()) return st;
  value = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8U) | rx[1]);
  return INA3221::Status::Ok();
}

void scanIna3221Addresses() {
  Serial.println("=== INA3221 Address Probe (raw, no driver-health tracking) ===");
  uint8_t healthy = 0;
  for (uint8_t address = INA3221_ADDR_MIN; address <= INA3221_ADDR_MAX; ++address) {
    Serial.printf("  0x%02X (%s): ", address, addressStrap(address));
    INA3221::Status st = transport::probeAddress(address, board::I2C_TIMEOUT_MS);
    if (!st.ok()) {
      Serial.printf("NO_ACK code=%s detail=%ld\n", errToStr(st.code),
                    static_cast<long>(st.detail));
      continue;
    }
    uint16_t manufacturer = 0;
    uint16_t die = 0;
    const INA3221::Status manufacturerStatus = readRegisterAt(
        address, INA3221::cmd::REG_MANUFACTURER_ID, manufacturer);
    const INA3221::Status dieStatus = readRegisterAt(
        address, INA3221::cmd::REG_DIE_ID, die);
    if (!manufacturerStatus.ok() || !dieStatus.ok()) {
      const INA3221::Status failed = manufacturerStatus.ok() ? dieStatus : manufacturerStatus;
      Serial.printf("ACK ID_READ_ERROR code=%s detail=%ld msg=%s\n",
                    errToStr(failed.code), static_cast<long>(failed.detail),
                    failed.msg ? failed.msg : "");
      continue;
    }
    const bool match = manufacturer == INA3221::cmd::MANUFACTURER_ID_VALUE &&
                       die == INA3221::cmd::DIE_ID_VALUE;
    Serial.printf("ACK manufacturer=0x%04X die=0x%04X %s\n",
                  manufacturer, die, match ? "HEALTHY_INA3221" : "ID_MISMATCH");
    if (match) ++healthy;
  }
  Serial.printf("  Healthy INA3221 devices: %u\n", static_cast<unsigned>(healthy));
  if (healthy == 0U) {
    printStatus(INA3221::Status::Error(
        INA3221::Err::DEVICE_NOT_FOUND, "No healthy INA3221 found"));
  }
}

INA3221::Status requireIdleOwnerJob() {
  INA3221::JobProgress progress{};
  const INA3221::Status st = device.getJobProgress(progress);
  if (!st.ok()) return st;
  if (progress.state == INA3221::JobTerminalState::ACTIVE || progress.resultPending) {
    return INA3221::Status::Error(INA3221::Err::JOB_BUSY,
                                  "Owner job must be idle");
  }
  return INA3221::Status::Ok();
}

void retainCurrentProfile() {
  if (device.isBound()) {
    retainedProfile = device.deviceProfile();
    retainedProfileValid = true;
  }
}

INA3221::Status initializeDeviceAt(uint8_t address, bool automaticService) {
  if (!isValidIna3221Address(address)) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM,
                                  "Address must be 0x40-0x43", address);
  }
  INA3221::Status st = requireIdleOwnerJob();
  if (!st.ok()) return st;
  retainCurrentProfile();
  INA3221::DeviceProfile profile = retainedProfileValid
                                       ? retainedProfile
                                       : makeOwnerProfile();
  profile.i2cAddress = address;
  device.unbind();
  ownerDemoPhase = OwnerDemoPhase::IDLE;
  selectedAddress = address;
  retainedProfile = profile;
  retainedProfileValid = true;
  st = device.bind(makeOwnerTransport(), profile);
  if (!st.ok()) return st;
  const uint32_t requestId = nextOwnerRequestId();
  st = device.startInitialize(
      requestId,
      ownerDeadlineFor(INA3221::JobKind::INITIALIZE, profile));
  if (st.inProgress()) {
    ownerDemoPhase = OwnerDemoPhase::ACTIVE;
    ownerAutoService = automaticService;
    startupSamplePending = false;
  }
  return st;
}

INA3221::Status setExampleFrequency(uint32_t frequencyHz) {
  if (frequencyHz < I2C_FREQ_MIN_HZ || frequencyHz > I2C_FREQ_MAX_HZ) {
    return INA3221::Status::Error(INA3221::Err::OUT_OF_RANGE,
                                  "I2C frequency must be 10000-400000 Hz",
                                  static_cast<int32_t>(frequencyHz));
  }
  INA3221::Status st = requireIdleOwnerJob();
  if (!st.ok()) return st;
  const uint32_t previous = transport::frequencyHz();
  st = transport::setFrequency(frequencyHz);
  if (!st.ok()) return st;
  if (!device.isInitialized()) return INA3221::Status::Ok();

  const uint8_t address = device.deviceProfile().i2cAddress;
  uint16_t manufacturer = 0;
  uint16_t die = 0;
  st = readRegisterAt(address, INA3221::cmd::REG_MANUFACTURER_ID, manufacturer);
  if (st.ok()) st = readRegisterAt(address, INA3221::cmd::REG_DIE_ID, die);
  if (st.ok() && manufacturer != INA3221::cmd::MANUFACTURER_ID_VALUE) {
    st = INA3221::Status::Error(INA3221::Err::MANUFACTURER_ID_MISMATCH,
                                "Manufacturer ID mismatch", manufacturer);
  }
  if (st.ok() && die != INA3221::cmd::DIE_ID_VALUE) {
    st = INA3221::Status::Error(INA3221::Err::DIE_ID_MISMATCH,
                                "Die ID mismatch", die);
  }
  if (!st.ok()) {
    const INA3221::Status restore = transport::setFrequency(previous);
    if (!restore.ok()) {
      return INA3221::Status::Error(INA3221::Err::I2C_BUS,
                                    "Frequency verification and rollback failed",
                                    restore.detail);
    }
    return st;
  }
  return INA3221::Status::Ok();
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
  serviceCliOutput();
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
  serviceCliOutput();
  Serial.printf("  Conversion: started=%s ready=%s start=%lu ms\n",
                snap.conversionStarted ? "YES" : "NO",
                snap.conversionReady ? "YES" : "NO",
                static_cast<unsigned long>(snap.conversionStartMs));
  Serial.printf("  Mask/Enable writable cache: 0x%04X\n", snap.maskEnableWritableCache);
  Serial.printf("  Hardware config dirty: %s\n", snap.hardwareConfigDirty ? "YES" : "NO");
  serviceCliOutput();
  if (snap.hardwareConfigDirty) {
    Serial.printf("  Dirty reason: %s (code=%u detail=%ld)\n",
                  errToStr(snap.hardwareConfigDirtyStatus.code),
                  static_cast<unsigned>(snap.hardwareConfigDirtyStatus.code),
                  static_cast<long>(snap.hardwareConfigDirtyStatus.detail));
    if (snap.hardwareConfigDirtyStatus.msg && snap.hardwareConfigDirtyStatus.msg[0]) {
      Serial.printf("  Dirty message: %s\n", snap.hardwareConfigDirtyStatus.msg);
    }
    serviceCliOutput();
  }
  Serial.printf("  Cycle time: %lu us\n", static_cast<unsigned long>(device.getCycleTimeUs()));
  serviceCliOutput();
}

void printDesiredProfile() {
  if (!device.isBound() && !retainedProfileValid) {
    Serial.println("  No retained profile");
    return;
  }
  const INA3221::DeviceProfile& profile =
      device.isBound() ? device.deviceProfile() : retainedProfile;
  Serial.println("=== Desired Device Profile ===");
  Serial.printf("  Bound: %s  Initialized: %s  Address: 0x%02X\n",
                device.isBound() ? "YES" : "NO",
                device.isInitialized() ? "YES" : "NO",
                profile.i2cAddress);
  Serial.printf("  Generation: %lu  Measurement state: %s  Alert state: %s\n",
                static_cast<unsigned long>(device.profileGeneration()),
                appliedStateToStr(device.measurementConfigState()),
                appliedStateToStr(device.alertConfigState()));
  Serial.printf("  Mode: %s  AVG: %s  VBUSCT: %s  VSHCT: %s  Channels=0x%02X\n",
                modeToStr(profile.mode), avgToStr(profile.averaging),
                ctToStr(profile.vBusCt), ctToStr(profile.vShCt),
                static_cast<unsigned>(profile.enabledChannels));
  for (uint8_t i = 0; i < 3U; ++i) {
    Serial.printf("  CH%u calibration: %lu uohm (%.6f ohm), direction=%s\n",
                  static_cast<unsigned>(i + 1U),
                  static_cast<unsigned long>(profile.shunts[i].resistanceMicroOhms),
                  static_cast<double>(profile.shunts[i].resistanceMicroOhms) / 1000000.0,
                  directionToStr(profile.shunts[i].direction));
    Serial.printf("  CH%u limits: critical=%ld uV warning=%ld uV\n",
                  static_cast<unsigned>(i + 1U),
                  static_cast<long>(profile.alerts.criticalLimitMicroVolts[i]),
                  static_cast<long>(profile.alerts.warningLimitMicroVolts[i]));
  }
  Serial.printf("  Sum channels=0x%02X  Sum limit=%ld uV\n",
                static_cast<unsigned>(profile.alerts.summationChannels),
                static_cast<long>(profile.alerts.shuntSumLimitMicroVolts));
  Serial.printf("  Power-valid window: %lu..%lu mV  Latch: warning=%u critical=%u\n",
                static_cast<unsigned long>(profile.alerts.powerValidLowerMilliVolts),
                static_cast<unsigned long>(profile.alerts.powerValidUpperMilliVolts),
                profile.alerts.warningLatch ? 1U : 0U,
                profile.alerts.criticalLatch ? 1U : 0U);
}

uint16_t expectedConfigRegister(const INA3221::DeviceProfile& profile) {
  uint16_t value = 0;
  if ((profile.enabledChannels & INA3221::CHANNEL_1) != 0U) value |= INA3221::cmd::MASK_CH1EN;
  if ((profile.enabledChannels & INA3221::CHANNEL_2) != 0U) value |= INA3221::cmd::MASK_CH2EN;
  if ((profile.enabledChannels & INA3221::CHANNEL_3) != 0U) value |= INA3221::cmd::MASK_CH3EN;
  value |= static_cast<uint16_t>(static_cast<uint8_t>(profile.averaging) << INA3221::cmd::BIT_AVG);
  value |= static_cast<uint16_t>(static_cast<uint8_t>(profile.vBusCt) << INA3221::cmd::BIT_VBUSCT);
  value |= static_cast<uint16_t>(static_cast<uint8_t>(profile.vShCt) << INA3221::cmd::BIT_VSHCT);
  value |= static_cast<uint16_t>(static_cast<uint8_t>(profile.mode) << INA3221::cmd::BIT_MODE);
  return value;
}

INA3221::Status buildRegisterExpectations(const INA3221::DeviceProfile& profile,
                                           RegisterEvidence (&items)[11]) {
  for (RegisterEvidence& item : items) item = RegisterEvidence{};
  items[0].reg = INA3221::cmd::REG_CONFIG;
  items[0].expected = expectedConfigRegister(profile);
  items[0].compareMask = static_cast<uint16_t>(~INA3221::cmd::MASK_RST);
  for (uint8_t i = 0; i < 3U; ++i) {
    const uint8_t criticalIndex = static_cast<uint8_t>(1U + i * 2U);
    const uint8_t warningIndex = static_cast<uint8_t>(criticalIndex + 1U);
    items[criticalIndex].reg = static_cast<uint8_t>(INA3221::cmd::REG_CH1_CRIT_LIMIT + i * 2U);
    items[warningIndex].reg = static_cast<uint8_t>(INA3221::cmd::REG_CH1_WARN_LIMIT + i * 2U);
    items[criticalIndex].compareMask = 0xFFFFU;
    items[warningIndex].compareMask = 0xFFFFU;
    INA3221::Status st = INA3221::INA3221::encodeShuntMicroVolts(
        profile.alerts.criticalLimitMicroVolts[i], items[criticalIndex].expected);
    if (!st.ok()) return st;
    st = INA3221::INA3221::encodeShuntMicroVolts(
        profile.alerts.warningLimitMicroVolts[i], items[warningIndex].expected);
    if (!st.ok()) return st;
  }
  items[7].reg = INA3221::cmd::REG_SHUNT_SUM_LIMIT;
  items[7].compareMask = 0xFFFFU;
  INA3221::Status st = INA3221::INA3221::encodeShuntSumMicroVolts(
      profile.alerts.shuntSumLimitMicroVolts, items[7].expected);
  if (!st.ok()) return st;
  items[8].reg = INA3221::cmd::REG_MASK_ENABLE;
  items[8].compareMask = static_cast<uint16_t>(INA3221::cmd::MASK_SCC1 |
                                               INA3221::cmd::MASK_SCC2 |
                                               INA3221::cmd::MASK_SCC3 |
                                               INA3221::cmd::MASK_WEN |
                                               INA3221::cmd::MASK_CEN);
  if ((profile.alerts.summationChannels & INA3221::CHANNEL_1) != 0U) items[8].expected |= INA3221::cmd::MASK_SCC1;
  if ((profile.alerts.summationChannels & INA3221::CHANNEL_2) != 0U) items[8].expected |= INA3221::cmd::MASK_SCC2;
  if ((profile.alerts.summationChannels & INA3221::CHANNEL_3) != 0U) items[8].expected |= INA3221::cmd::MASK_SCC3;
  if (profile.alerts.warningLatch) items[8].expected |= INA3221::cmd::MASK_WEN;
  if (profile.alerts.criticalLatch) items[8].expected |= INA3221::cmd::MASK_CEN;
  items[9].reg = INA3221::cmd::REG_PV_UPPER_LIMIT;
  items[9].compareMask = 0xFFFFU;
  st = INA3221::INA3221::encodeBusMilliVolts(
      static_cast<int32_t>(profile.alerts.powerValidUpperMilliVolts), items[9].expected);
  if (!st.ok()) return st;
  items[10].reg = INA3221::cmd::REG_PV_LOWER_LIMIT;
  items[10].compareMask = 0xFFFFU;
  return INA3221::INA3221::encodeBusMilliVolts(
      static_cast<int32_t>(profile.alerts.powerValidLowerMilliVolts), items[10].expected);
}

const char* registerName(uint8_t reg) {
  switch (reg) {
    case INA3221::cmd::REG_CONFIG: return "CONFIG";
    case INA3221::cmd::REG_CH1_CRIT_LIMIT: return "CH1_CRIT";
    case INA3221::cmd::REG_CH1_WARN_LIMIT: return "CH1_WARN";
    case INA3221::cmd::REG_CH2_CRIT_LIMIT: return "CH2_CRIT";
    case INA3221::cmd::REG_CH2_WARN_LIMIT: return "CH2_WARN";
    case INA3221::cmd::REG_CH3_CRIT_LIMIT: return "CH3_CRIT";
    case INA3221::cmd::REG_CH3_WARN_LIMIT: return "CH3_WARN";
    case INA3221::cmd::REG_SHUNT_SUM_LIMIT: return "SUM_LIMIT";
    case INA3221::cmd::REG_MASK_ENABLE: return "MASK_ENABLE";
    case INA3221::cmd::REG_PV_UPPER_LIMIT: return "PV_UPPER";
    case INA3221::cmd::REG_PV_LOWER_LIMIT: return "PV_LOWER";
    default: return "UNKNOWN";
  }
}

void printVerificationEvidence() {
  Serial.println("=== Managed Register Verification Evidence ===");
  if (!lastVerification.valid) {
    Serial.println("  No verification captured; run 'verify'");
    return;
  }
  Serial.printf("  Captured at: %lu ms  Address: 0x%02X  Profile generation: %lu\n",
                static_cast<unsigned long>(lastVerification.capturedAtMs),
                lastVerification.i2cAddress,
                static_cast<unsigned long>(lastVerification.profileGeneration));
  Serial.printf("  Registers: %u  Mismatches: %u  Read failures: %u\n",
                static_cast<unsigned>(lastVerification.count),
                static_cast<unsigned>(lastVerification.mismatches),
                static_cast<unsigned>(lastVerification.readFailures));
  for (uint8_t i = 0; i < lastVerification.count; ++i) {
    const RegisterEvidence& item = lastVerification.registers[i];
    if (!item.readOk) {
      Serial.printf("  REG 0x%02X %-11s READ_ERROR code=%s detail=%ld msg=%s\n",
                    item.reg, registerName(item.reg), errToStr(item.status.code),
                    static_cast<long>(item.status.detail),
                    item.status.msg ? item.status.msg : "");
      continue;
    }
    const uint16_t delta = static_cast<uint16_t>((item.actual ^ item.expected) & item.compareMask);
    Serial.printf("  REG 0x%02X %-11s actual=0x%04X expected=0x%04X mask=0x%04X delta=0x%04X %s\n",
                  item.reg, registerName(item.reg), item.actual, item.expected,
                  item.compareMask, delta, delta == 0U ? "MATCH" : "MISMATCH");
  }
  Serial.printf("  VERIFY_RESULT: %s\n",
                (lastVerification.mismatches == 0U && lastVerification.readFailures == 0U)
                    ? "PASS" : "FAIL");
}

INA3221::Status verifyManagedRegisters() {
  lastVerification = VerificationEvidence{};
  lastVerification.valid = true;
  lastVerification.capturedAtMs = millis();
  lastVerification.i2cAddress = device.isBound()
                                    ? device.deviceProfile().i2cAddress
                                    : selectedAddress;
  lastVerification.profileGeneration = device.profileGeneration();
  lastVerification.count = 11U;
  const INA3221::DeviceProfile verificationProfile =
      device.isBound() ? device.deviceProfile()
                       : (retainedProfileValid ? retainedProfile : makeOwnerProfile());
  INA3221::Status st = buildRegisterExpectations(
      verificationProfile, lastVerification.registers);
  if (!st.ok()) {
    lastVerification.readFailures = lastVerification.count;
    for (RegisterEvidence& item : lastVerification.registers) item.status = st;
    return st;
  }
  if (!device.isInitialized()) {
    const INA3221::Status uninitialized = INA3221::Status::Error(
        INA3221::Err::NOT_INITIALIZED, "Driver not initialized");
    lastVerification.readFailures = lastVerification.count;
    for (RegisterEvidence& item : lastVerification.registers) {
      item.status = uninitialized;
    }
    return uninitialized;
  }
  INA3221::Status firstReadError = INA3221::Status::Ok();
  uint8_t firstMismatch = 0xFFU;
  for (uint8_t i = 0; i < lastVerification.count; ++i) {
    RegisterEvidence& item = lastVerification.registers[i];
    item.status = device.readRegister16(item.reg, item.actual);
    item.readOk = item.status.ok();
    if (!item.readOk) {
      if (firstReadError.ok()) firstReadError = item.status;
      ++lastVerification.readFailures;
      continue;
    }
    if (((item.actual ^ item.expected) & item.compareMask) != 0U) {
      if (firstMismatch == 0xFFU) firstMismatch = item.reg;
      ++lastVerification.mismatches;
    }
  }
  if (lastVerification.readFailures != 0U) {
    return firstReadError;
  }
  if (lastVerification.mismatches != 0U) {
    return INA3221::Status::Error(INA3221::Err::PROFILE_MISMATCH,
                                  "Managed register mismatch",
                                  firstMismatch);
  }
  return INA3221::Status::Ok();
}

void printFullDiagnostics() {
  Serial.println("=== Full INA3221 Diagnostics (cache-only) ===");
  Serial.printf("  Selected address: 0x%02X  I2C frequency: %lu Hz\n",
                selectedAddress,
                static_cast<unsigned long>(transport::frequencyHz()));
  printDriverHealth();
  printDesiredProfile();
  printOwnerJobProgress();
  if (lastOwnerResultValid) printOwnerResult(lastOwnerResult);
  else Serial.println("  Last owner result: none");
  printVerificationEvidence();
  const transport::TransferStats stats = transport::transferStats();
  Serial.printf("  Transport transfers: read=%lu write=%lu total=%lu\n",
                static_cast<unsigned long>(stats.read),
                static_cast<unsigned long>(stats.write),
                static_cast<unsigned long>(stats.read + stats.write));
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

  Serial.println("=== INA3221 selftest (configuration-preserving diagnostics) ===");
  Serial.println("  Note: managed-register verification reads Mask/Enable and retains its read-clear alert evidence.");

  const INA3221::Status idle = requireIdleOwnerJob();
  if (!idle.ok()) {
    reportSkip("all checks", "owner job active");
    printStatus(idle);
    Serial.printf("Selftest result: pass=0 fail=0 skip=1\n");
    return;
  }

  const INA3221::DeviceProfile profileBefore =
      device.isBound() ? device.deviceProfile() : INA3221::DeviceProfile{};
  const uint32_t generationBefore = device.profileGeneration();

  const uint32_t succBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint8_t consBefore = device.consecutiveFailures();

  const INA3221::Status pst = device.probe();
  if (pst.code == INA3221::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                  cli::nonZeroGoodColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                  cli::zeroGoodColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
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

  const bool poweredDown = profileBefore.mode == INA3221::Mode::POWER_DOWN ||
                           profileBefore.mode == INA3221::Mode::POWER_DOWN_ALT;
  for (uint8_t i = 0; i < 3U; ++i) {
    const INA3221::ChannelMask bit = static_cast<INA3221::ChannelMask>(1U << i);
    char name[24];
    snprintf(name, sizeof(name), "readChannel(CH%u)", static_cast<unsigned>(i + 1U));
    if ((profileBefore.enabledChannels & bit) == 0U || poweredDown) {
      reportSkip(name, poweredDown ? "profile is powered down" : "channel disabled");
      continue;
    }
    INA3221::ChannelMeasurement measurement{};
    st = device.readChannel(static_cast<INA3221::Channel>(i), measurement);
    reportCheck(name, st.ok(), st.ok() ? "" : errToStr(st.code));
  }

  INA3221::AlertSnapshot alertSnapshot{};
  st = device.peekAlertEvents(alertSnapshot);
  reportCheck("peekAlertEvents(cache-only)", st.ok(), st.ok() ? "" : errToStr(st.code));

  INA3221::SampleBatch cachedSample{};
  st = device.peekLastSample(cachedSample);
  if (st.ok()) reportCheck("peekLastSample(cache-only)", true, "");
  else reportSkip("peekLastSample(cache-only)", "no completed sample retained");

  st = verifyManagedRegisters();
  reportCheck("verify 11 managed registers", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportCheck("register mismatch count zero", lastVerification.mismatches == 0U, "");
  reportCheck("register read failure count zero", lastVerification.readFailures == 0U, "");

  int32_t decoded = 0;
  st = INA3221::INA3221::decodeShuntMicroVolts(0xFFF8U, decoded);
  reportCheck("fixed shunt decode", st.ok() && decoded == -40, "");
  st = INA3221::INA3221::decodeBusMilliVolts(0x0008U, decoded);
  reportCheck("fixed bus decode", st.ok() && decoded == 8, "");

  const bool profileUnchanged =
      device.isBound() &&
      std::memcmp(&profileBefore, &device.deviceProfile(), sizeof(profileBefore)) == 0;
  reportCheck("profile preserved", profileUnchanged, "");
  reportCheck("profile generation preserved",
              generationBefore == device.profileGeneration(), "");
  reportCheck("driver online", device.isOnline(), "");
  reportCheck("consecutive failures zero", device.consecutiveFailures() == 0U, "");

  Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                cli::nonZeroGoodColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                cli::zeroGoodColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                LOG_COLOR_YELLOW, static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
  if (stats.fail != 0U) {
    printStatus(INA3221::Status::Error(INA3221::Err::PROFILE_MISMATCH,
                                      "Selftest failed",
                                      static_cast<int32_t>(stats.fail)));
  }
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
                cli::nonZeroGoodColor(okTotal),
                static_cast<unsigned long>(okTotal),
                LOG_COLOR_RESET,
                cli::zeroGoodColor(failTotal),
                static_cast<unsigned long>(failTotal),
                LOG_COLOR_RESET,
                cli::successRateColor(pct),
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
                  cli::nonZeroGoodColor(stats[i].ok),
                  static_cast<unsigned long>(stats[i].ok),
                  LOG_COLOR_RESET,
                  cli::zeroGoodColor(stats[i].fail),
                  static_cast<unsigned long>(stats[i].fail),
                  LOG_COLOR_RESET);
  }
  const uint32_t successDelta = device.totalSuccess() - successBefore;
  const uint32_t failDelta = device.totalFailures() - failBefore;
  Serial.printf("  Health delta (tracked I2C): %ssuccess +%lu%s, %sfailures +%lu%s\n",
                cli::nonZeroGoodColor(successDelta),
                static_cast<unsigned long>(successDelta),
                LOG_COLOR_RESET,
                cli::zeroGoodColor(failDelta),
                static_cast<unsigned long>(failDelta),
                LOG_COLOR_RESET);
  if (failTotal > 0U && hilCommandStatus == INA3221::Err::OK) {
    hilCommandStatus = INA3221::Err::I2C_ERROR;
  }
}

INA3221::Status runOneOwnerSample(INA3221::JobResult& result) {
  INA3221::Status st = startOwnerSample();
  if (!st.inProgress()) return st;
  const bool previousAutoService = ownerAutoService;
  ownerAutoService = false;
  while (true) {
    INA3221::JobProgress progress{};
    st = device.getJobProgress(progress);
    if (!st.ok()) break;
    if (progress.resultPending) {
      st = device.takeJobResult(result);
      break;
    }
    if (progress.state != INA3221::JobTerminalState::ACTIVE) {
      st = INA3221::Status::Error(INA3221::Err::NO_RESULT,
                                  "Owner sample ended without result");
      break;
    }
    INA3221::PollContext context{};
    context.nowMs = ownerNowMs();
    context.deadlineMs = progress.deadlineMs;
    context.transferTimeoutMs = board::I2C_TIMEOUT_MS;
    context.maxTransfers = 1U;
    st = device.pollJob(context);
    if (!st.ok() && !st.inProgress()) {
      (void)device.getJobProgress(progress);
      if (!progress.resultPending) break;
    }
    yield();
    delay(1);
  }
  ownerDemoPhase = OwnerDemoPhase::IDLE;
  ownerAutoService = previousAutoService;
  if (!st.ok()) return st;
  lastOwnerResult = result;
  lastOwnerResultValid = true;
  if (!result.status.ok()) return result.status;
  if (!result.sampleValid) {
    return INA3221::Status::Error(INA3221::Err::NO_RESULT,
                                  "Owner sample result has no sample");
  }
  return INA3221::Status::Ok();
}

void runOwnerStress(int count) {
  uint32_t passed = 0;
  uint32_t failed = 0;
  uint16_t maximumTransfers = 0;
  const INA3221::Mode mode = device.deviceProfile().mode;
  const INA3221::JobKind kind =
      (mode == INA3221::Mode::SHUNT_CONT || mode == INA3221::Mode::BUS_CONT ||
       mode == INA3221::Mode::SHUNT_BUS_CONT)
          ? INA3221::JobKind::CONTINUOUS_SAMPLE
          : INA3221::JobKind::TRIGGERED_SAMPLE;
  INA3221::Status boundStatus = INA3221::INA3221::maximumJobTransfers(
      kind, device.deviceProfile(), maximumTransfers);
  if (!boundStatus.ok()) {
    printStatus(boundStatus);
    return;
  }
  const uint32_t startMs = millis();
  for (int i = 0; i < count; ++i) {
    INA3221::JobResult result{};
    const uint32_t expectedRequest = ownerRequestId == UINT32_MAX ? 1U : ownerRequestId + 1U;
    INA3221::Status st = runOneOwnerSample(result);
    const bool valid = st.ok() && result.requestId == expectedRequest &&
                       result.profileGeneration == device.profileGeneration() &&
                       result.sample.validChannels == device.deviceProfile().enabledChannels &&
                       result.transfers <= maximumTransfers;
    if (valid) {
      ++passed;
    } else {
      ++failed;
      if (verboseMode) {
        Serial.printf("  Owner iteration %d failed validation\n", i + 1);
        printStatus(st.ok() ? INA3221::Status::Error(
                                 INA3221::Err::PROFILE_MISMATCH,
                                 "Owner result invariant failed")
                           : st);
      }
      break;
    }
  }
  Serial.println("=== stress_owner summary ===");
  Serial.printf("  Target: %d  pass=%lu fail=%lu  max_transfers=%u  duration=%lu ms\n",
                count, static_cast<unsigned long>(passed),
                static_cast<unsigned long>(failed),
                static_cast<unsigned>(maximumTransfers),
                static_cast<unsigned long>(millis() - startMs));
  if (failed != 0U) {
    printStatus(INA3221::Status::Error(INA3221::Err::PROFILE_MISMATCH,
                                      "Owner stress validation failed",
                                      static_cast<int32_t>(failed)));
  }
}

void runFrequencyStress(int count) {
  const uint32_t initialFrequency = transport::frequencyHz();
  uint32_t passed = 0;
  uint32_t failed = 0;
  INA3221::Status last = INA3221::Status::Ok();
  for (int i = 0; i < count; ++i) {
    const uint32_t frequency = (i & 1) == 0 ? 100000U : 400000U;
    last = setExampleFrequency(frequency);
    uint16_t manufacturer = 0;
    uint16_t die = 0;
    if (last.ok()) last = device.readManufacturerId(manufacturer);
    if (last.ok()) last = device.readDieId(die);
    if (last.ok() && (manufacturer != INA3221::cmd::MANUFACTURER_ID_VALUE ||
                      die != INA3221::cmd::DIE_ID_VALUE)) {
      last = INA3221::Status::Error(INA3221::Err::DIE_ID_MISMATCH,
                                    "Frequency stress identity mismatch", die);
    }
    if (last.ok()) ++passed;
    else {
      ++failed;
      break;
    }
  }
  const INA3221::Status restore = setExampleFrequency(initialFrequency);
  if (!restore.ok()) {
    ++failed;
    last = restore;
  }
  Serial.println("=== stress_freq summary ===");
  Serial.printf("  Target: %d  pass=%lu fail=%lu  restored_hz=%lu\n",
                count, static_cast<unsigned long>(passed),
                static_cast<unsigned long>(failed),
                static_cast<unsigned long>(transport::frequencyHz()));
  if (failed != 0U) printStatus(last);
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

  if (cmd.startsWith("hilrun ")) {
    String args = cmd.substring(7);
    args.trim();
    const int tokenSplit = args.indexOf(' ');
    String token;
    String sequence;
    String inner;
    if (tokenSplit >= 0) {
      token = args.substring(0, tokenSplit);
      String tail = args.substring(tokenSplit + 1);
      tail.trim();
      const int sequenceSplit = tail.indexOf(' ');
      if (sequenceSplit >= 0) {
        sequence = tail.substring(0, sequenceSplit);
        inner = tail.substring(sequenceSplit + 1);
        inner.trim();
      }
    }
    Serial.printf("HIL_BEGIN token=%s seq=%s\n", token.c_str(), sequence.c_str());
    const uint32_t startMs = millis();
    if (token.length() == 0 || sequence.length() == 0 || inner.length() == 0 ||
        inner.startsWith("hilrun ")) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      Serial.println("  Status: INVALID_PARAM (malformed or nested hilrun)");
    } else {
      hilCommandStatus = INA3221::Err::OK;
      processCommand(inner);
    }
    Serial.printf("HIL_END token=%s seq=%s status=%s elapsed_ms=%lu\n",
                  token.c_str(), sequence.c_str(), errToStr(hilCommandStatus),
                  static_cast<unsigned long>(millis() - startMs));
    return;
  } else if (cmd.startsWith("hilmark ")) {
    String token = cmd.substring(8);
    token.trim();
    if (token.length() == 0) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: hilmark <token>");
    } else {
      Serial.printf("HILMARK %s\n", token.c_str());
    }
  } else if (cmd == "xfer_reset") {
    transport::resetTransferStats();
    Serial.println("XFER_RESET read=0 write=0 total=0");
  } else if (cmd == "xfer_stats") {
    const transport::TransferStats stats = transport::transferStats();
    Serial.printf("XFER_STATS read=%lu write=%lu total=%lu\n",
                  static_cast<unsigned long>(stats.read),
                  static_cast<unsigned long>(stats.write),
                  static_cast<unsigned long>(stats.read + stats.write));
  } else if (cmd.startsWith("xfer_assert ")) {
    uint32_t expectedRead = 0;
    uint32_t expectedWrite = 0;
    uint32_t expectedTotal = 0;
    if (!parseThreeU32(cmd.substring(12), expectedRead, expectedWrite,
                       expectedTotal)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: xfer_assert <read> <write> <total>");
      return;
    }
    const transport::TransferStats stats = transport::transferStats();
    const uint32_t total = stats.read + stats.write;
    if (stats.read == expectedRead && stats.write == expectedWrite &&
        total == expectedTotal) {
      Serial.printf("XFER_ASSERT PASS read=%lu write=%lu total=%lu\n",
                    static_cast<unsigned long>(stats.read),
                    static_cast<unsigned long>(stats.write),
                    static_cast<unsigned long>(total));
    } else {
      hilCommandStatus = INA3221::Err::PROFILE_MISMATCH;
      Serial.printf("XFER_ASSERT FAIL expected_read=%lu expected_write=%lu "
                    "expected_total=%lu read=%lu write=%lu total=%lu\n",
                    static_cast<unsigned long>(expectedRead),
                    static_cast<unsigned long>(expectedWrite),
                    static_cast<unsigned long>(expectedTotal),
                    static_cast<unsigned long>(stats.read),
                    static_cast<unsigned long>(stats.write),
                    static_cast<unsigned long>(total));
      printStatus(INA3221::Status::Error(
          INA3221::Err::PROFILE_MISMATCH, "Transfer count mismatch",
          static_cast<int32_t>(total)));
    }
  } else if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "version" || cmd == "ver") {
    printVersionInfo();
  } else if (cmd == "scan") {
    i2c_scanner::scanDefault();
    scanIna3221Addresses();
  } else if (cmd == "scanina") {
    scanIna3221Addresses();
  } else if (cmd == "job" || cmd == "job progress") {
    printOwnerJobProgress();
  } else if (cmd == "job init") {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound; use init"));
      return;
    }
    const INA3221::DeviceProfile profile = device.deviceProfile();
    printStatus(trackOwnerStart(device.startInitialize(
        nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::INITIALIZE, profile))));
  } else if (cmd == "job apply") {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    const INA3221::DeviceProfile profile = device.deviceProfile();
    printStatus(trackOwnerStart(device.startApplyProfile(
        profile, nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::APPLY_PROFILE, profile))));
  } else if (cmd == "job reconcile") {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    printStatus(trackOwnerStart(device.startReconcile(
        nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::RECONCILE,
                         device.deviceProfile()))));
  } else if (cmd == "job sample") {
    printStatus(trackOwnerStart(startOwnerSample()));
  } else if (cmd == "job continuous" || cmd.startsWith("job continuous ")) {
    bool consumeAlerts = false;
    if (cmd.length() > 14U && !parseBool01(cmd.substring(15), consumeAlerts)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: job continuous [0|1]");
      return;
    }
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    printStatus(trackOwnerStart(device.startContinuousSample(
        nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::CONTINUOUS_SAMPLE,
                         device.deviceProfile()),
        consumeAlerts)));
  } else if (cmd == "job powerdown") {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    printStatus(trackOwnerStart(device.startPowerDown(
        nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::POWER_DOWN,
                         device.deviceProfile()))));
  } else if (cmd == "job cancel") {
    // Both operations are cache-only. Consume and print the terminal record
    // inside the command frame so automation never depends on loop timing.
    const INA3221::Status st = device.cancelJob();
    printStatus(st);
    if (st.code == INA3221::Err::CANCELLED) serviceOwnerJob();
  } else if (cmd == "job auto") {
    Serial.printf("  Owner auto service: %s\n", ownerAutoService ? "ON" : "OFF");
  } else if (cmd.startsWith("job auto ")) {
    bool enabled = false;
    if (!parseBool01(cmd.substring(9), enabled)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: job auto <0|1>");
      return;
    }
    ownerAutoService = enabled;
    Serial.printf("  Owner auto service: %s\n", ownerAutoService ? "ON" : "OFF");
  } else if (cmd.startsWith("job step ")) {
    uint32_t budget = 0;
    if (!parseU32(cmd.substring(9), budget) || budget > 255U) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: job step <0..255>");
      return;
    }
    printStatus(stepOwnerJob(static_cast<uint8_t>(budget)));
    printOwnerJobProgress();
  } else if (cmd == "job result") {
    if (lastOwnerResultValid) printOwnerResult(lastOwnerResult);
    else printStatus(INA3221::Status::Error(INA3221::Err::NO_RESULT,
                                            "No cached owner result"));
  } else if (cmd == "job lastsample") {
    INA3221::SampleBatch sample{};
    const INA3221::Status st = device.peekLastSample(sample);
    if (st.ok()) printOwnerSample(sample);
    else printStatus(st);
  } else if (cmd == "job alerts" || cmd == "job alerts take") {
    INA3221::AlertSnapshot alerts{};
    const INA3221::Status st = cmd == "job alerts take"
                                  ? device.takeAlertEvents(alerts)
                                  : device.peekAlertEvents(alerts);
    if (st.ok()) printAlertSnapshot(alerts, cmd == "job alerts take"
                                                ? "Taken Alert Evidence"
                                                : "Retained Alert Evidence");
    else printStatus(st);
  } else if (cmd == "probe") {
    LOGI("Probing device (no health tracking)...");
    auto st = device.probe();
    printStatus(st);
  } else if (cmd == "drv") {
    printDriverHealth();
  } else if (cmd == "diag") {
    printFullDiagnostics();
  } else if (cmd == "verify") {
    Serial.println("  Note: Mask/Enable verification consumes CVRF/latched flags; evidence is retained.");
    const INA3221::Status st = verifyManagedRegisters();
    printVerificationEvidence();
    printStatus(st);
  } else if (cmd == "mismatch") {
    printVerificationEvidence();
  } else if (cmd == "recover") {
    LOGI("Attempting recovery...");
    auto st = device.recover();
    printStatus(st);
    printDriverHealth();
  } else if (cmd == "addr") {
    Serial.printf("  Selected INA3221 address: 0x%02X (%s)\n",
                  selectedAddress, addressStrap(selectedAddress));
    if (device.isBound()) {
      Serial.printf("  Active driver address: 0x%02X\n",
                    device.deviceProfile().i2cAddress);
    } else {
      Serial.println("  Active driver address: none (unbound)");
    }
  } else if (cmd.startsWith("addr ")) {
    uint8_t address = 0;
    if (!parseAddress(cmd.substring(5), address)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Invalid address; use 0x40-0x43");
      return;
    }
    selectedAddress = address;
    Serial.printf("  Selected INA3221 address: 0x%02X (%s); run init to apply\n",
                  selectedAddress, addressStrap(selectedAddress));
  } else if (cmd == "init" || cmd.startsWith("init ")) {
    uint8_t address = selectedAddress;
    if (cmd.length() > 4U && !parseAddress(cmd.substring(5), address)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Invalid address; use init [0x40-0x43]");
      return;
    }
    printStatus(initializeDeviceAt(address, true));
  } else if (cmd == "end") {
    const INA3221::Status idle = requireIdleOwnerJob();
    if (!idle.ok()) {
      printStatus(idle);
      return;
    }
    retainCurrentProfile();
    device.unbind();
    ownerDemoPhase = OwnerDemoPhase::IDLE;
    startupSamplePending = false;
    Serial.println("  Driver unbound; physical device state and I2C bus are unchanged");
  } else if (cmd == "freq") {
    Serial.printf("  I2C frequency: %lu Hz\n",
                  static_cast<unsigned long>(transport::frequencyHz()));
  } else if (cmd.startsWith("freq ")) {
    uint32_t frequency = 0;
    if (!parseU32(cmd.substring(5), frequency)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: freq <10000..400000>");
      return;
    }
    const INA3221::Status st = setExampleFrequency(frequency);
    printStatus(st);
    Serial.printf("  I2C frequency: %lu Hz\n",
                  static_cast<unsigned long>(transport::frequencyHz()));
  } else if (cmd == "verbose") {
    LOGI("Verbose mode: %s%s%s", cli::enabledColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
  } else if (cmd.startsWith("verbose ")) {
    bool val = false;
    if (!parseBool01(cmd.substring(8), val)) {
      LOGW("Invalid verbose value (0|1)");
      return;
    }
    verboseMode = val;
    LOGI("Verbose mode: %s%s%s", cli::enabledColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
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
      LOGI("Conversion ready: %s%s%s", cli::yesNoColor(ready), ready ? "YES" : "NO", LOG_COLOR_RESET);
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
    } else if (token == "pda") {
      mode = INA3221::Mode::POWER_DOWN_ALT;
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
      LOGW("Invalid mode (pd/pda/strig/btrig/sbtrig/sc/bc/sbc)");
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
  } else if (cmd == "direction") {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    const INA3221::DeviceProfile& profile = device.deviceProfile();
    Serial.printf("  Current direction: CH1=%s CH2=%s CH3=%s\n",
                  directionToStr(profile.shunts[0].direction),
                  directionToStr(profile.shunts[1].direction),
                  directionToStr(profile.shunts[2].direction));
  } else if (cmd.startsWith("direction ")) {
    String args = cmd.substring(10);
    args.trim();
    const int split = args.indexOf(' ');
    const int ch = split < 0 ? -1 : parseChannel(args.substring(0, split));
    bool inverted = false;
    if (ch < 0 || !parseBool01(args.substring(split + 1), inverted)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Usage: direction <1|2|3> <0|1>");
      return;
    }
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    INA3221::DeviceProfile profile = device.deviceProfile();
    profile.shunts[ch].direction = inverted
        ? INA3221::CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT
        : INA3221::CurrentDirection::POSITIVE_SHUNT_IS_POSITIVE_CURRENT;
    const INA3221::Status st = device.startApplyProfile(
        profile, nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::APPLY_PROFILE, profile));
    if (st.inProgress()) {
      ownerDemoPhase = OwnerDemoPhase::ACTIVE;
      ownerAutoService = true;
      startupSamplePending = false;
    }
    printStatus(st);
  } else if (cmd == "profile") {
    printDesiredProfile();
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
  } else if (cmd == "alertsnap" || cmd == "alertsnap take") {
    INA3221::AlertSnapshot snapshot{};
    const INA3221::Status st = cmd == "alertsnap take"
                                  ? device.takeAlertEvents(snapshot)
                                  : device.peekAlertEvents(snapshot);
    if (st.ok()) printAlertSnapshot(snapshot, cmd == "alertsnap take"
                                                ? "Taken Alert Evidence"
                                                : "Retained Alert Evidence");
    else printStatus(st);
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
    LOGI("Online: %s%s%s", cli::yesNoColor(online), online ? "YES" : "NO", LOG_COLOR_RESET);
  } else if (cmd == "selftest") {
    runSelfTest();
  } else if (cmd == "stress_owner") {
    runOwnerStress(10);
  } else if (cmd.startsWith("stress_owner ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(13), count) || count <= 0 || count > 10000) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Invalid count (1-10000)");
      return;
    }
    runOwnerStress(static_cast<int>(count));
  } else if (cmd == "stress_freq") {
    runFrequencyStress(10);
  } else if (cmd.startsWith("stress_freq ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(12), count) || count <= 0 || count > 10000) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      LOGW("Invalid count (1-10000)");
      return;
    }
    runFrequencyStress(static_cast<int>(count));
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
    hilCommandStatus = INA3221::Err::INVALID_PARAM;
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

  i2c_scanner::scanDefault();

  const INA3221::TransportConfig transportConfig = makeOwnerTransport();
  const INA3221::DeviceProfile profile = makeOwnerProfile();
  retainedProfile = profile;
  retainedProfileValid = true;

  // bind() validates and stores contracts without touching I2C. Initialization
  // is then advanced by serviceOwnerJob(), one transport callback per loop.
  auto st = device.bind(transportConfig, profile);
  if (!st.ok()) {
    LOGE("Failed to bind device contracts");
    printStatus(st);
    return;
  }
  ++ownerRequestId;
  st = device.startInitialize(
      ownerRequestId,
      ownerDeadlineFor(INA3221::JobKind::INITIALIZE, profile));
  if (!st.inProgress()) {
    LOGE("Failed to start staged initialization");
    printStatus(st);
    device.unbind();
    return;
  }
  ownerDemoPhase = OwnerDemoPhase::ACTIVE;
  ownerAutoService = true;
  startupSamplePending = true;
  LOGI("Staged initialization started (budget=1 transfer/loop)");

  Serial.println("\nType 'help' for commands");
  printPrompt();
}

void loop() {
  serviceOwnerJob();
  if (ownerDemoPhase == OwnerDemoPhase::IDLE && device.isInitialized()) {
    (void)device.tickStatus(millis());
  }

  static String inputBuffer;
  static bool inputOverflow = false;
  static constexpr size_t kMaxInputLen = 128;
  size_t bytesProcessed = 0;
  while (Serial.available() && bytesProcessed < kMaxInputLen + 2U) {
    ++bytesProcessed;
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (inputOverflow) {
        inputBuffer = "";
        inputOverflow = false;
        LOGW("Input line exceeds 128 bytes; command rejected");
        printPrompt();
        break;
      }
      if (inputBuffer.length() > 0U) {
        processCommand(inputBuffer);
        inputBuffer = "";
        printPrompt();
        break;  // Service the owner once between queued commands.
      }
      break;
    } else if (inputBuffer.length() < kMaxInputLen) {
      inputBuffer += c;
    } else {
      inputOverflow = true;
    }
  }
}
