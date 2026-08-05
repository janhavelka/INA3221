/// @file main.cpp
/// @brief Native ESP-IDF INA3221 bringup CLI.

#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <unistd.h>

#include "INA3221/INA3221.h"
#include "Ina3221IdfI2cTransport.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

static constexpr int I2C_SDA = 8;
static constexpr int I2C_SCL = 9;
static constexpr uint32_t I2C_FREQ_HZ = 400000;
static constexpr uint16_t I2C_TIMEOUT_MS = 50;
static constexpr uint8_t DEFAULT_ADDR = 0x40;
static constexpr size_t MAX_LINE_LEN = 128;
static constexpr uint8_t INA3221_ADDR_MIN = 0x40;
static constexpr uint8_t INA3221_ADDR_MAX = 0x43;
static constexpr uint32_t I2C_FREQ_MIN_HZ = 10000;
static constexpr uint32_t I2C_FREQ_MAX_HZ = 400000;

INA3221::INA3221 device;
bool verboseMode = false;
uint8_t activeAddress = DEFAULT_ADDR;
uint8_t selectedAddress = DEFAULT_ADDR;
INA3221::Err hilCommandStatus = INA3221::Err::OK;
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
// Leave enough bounded time for a human (or HIL) to inspect progress and issue
// manual job-step commands without expiring an otherwise healthy operation.
static constexpr uint64_t OWNER_JOB_TIMEOUT_MS = 5000U;

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

uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

uint64_t ownerNowMs() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000U;
}

void out(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::vprintf(fmt, args);
  va_end(args);
}

void info(const char* fmt, ...) {
  std::printf("[I] ");
  va_list args;
  va_start(args, fmt);
  std::vprintf(fmt, args);
  va_end(args);
  std::printf("\n");
}

void warn(const char* fmt, ...) {
  if (hilCommandStatus == INA3221::Err::OK) {
    hilCommandStatus = INA3221::Err::INVALID_PARAM;
  }
  std::printf("[W] ");
  va_list args;
  va_start(args, fmt);
  std::vprintf(fmt, args);
  va_end(args);
  std::printf("\n");
}

void error(const char* fmt, ...) {
  std::printf("[E] ");
  va_list args;
  va_start(args, fmt);
  std::vprintf(fmt, args);
  va_end(args);
  std::printf("\n");
}

void prompt() {
  std::printf("\nina3221> ");
  std::fflush(stdout);
}

char* ltrim(char* s) {
  while (*s != '\0' && std::isspace(static_cast<unsigned char>(*s))) {
    ++s;
  }
  return s;
}

void rtrim(char* s) {
  size_t len = std::strlen(s);
  while (len > 0 && std::isspace(static_cast<unsigned char>(s[len - 1]))) {
    s[--len] = '\0';
  }
}

char* trim(char* s) {
  char* start = ltrim(s);
  rtrim(start);
  return start;
}

bool startsWith(const char* text, const char* prefix) {
  return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

const char* argAfter(const char* text, const char* prefix) {
  return startsWith(text, prefix) ? text + std::strlen(prefix) : nullptr;
}

bool parseI32(const char* token, int32_t& outValue) {
  if (token == nullptr || *token == '\0') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long value = std::strtol(token, &end, 0);
  if (errno != 0 || end == token || *trim(end) != '\0') {
    return false;
  }
  outValue = static_cast<int32_t>(value);
  return true;
}

bool parseU32(const char* token, uint32_t& outValue) {
  if (token == nullptr || *token == '\0') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const unsigned long value = std::strtoul(token, &end, 0);
  if (errno != 0 || end == token || *trim(end) != '\0') {
    return false;
  }
  outValue = static_cast<uint32_t>(value);
  return true;
}

bool parseFloatArg(const char* token, float& outValue) {
  if (token == nullptr || *token == '\0') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const float value = std::strtof(token, &end);
  if (errno != 0 || end == token || *trim(end) != '\0') {
    return false;
  }
  outValue = value;
  return true;
}

bool parseBool01(const char* token, bool& outValue) {
  if (token == nullptr) {
    return false;
  }
  char tmp[MAX_LINE_LEN];
  std::snprintf(tmp, sizeof(tmp), "%s", token);
  char* value = trim(tmp);
  if (std::strcmp(value, "0") == 0) {
    outValue = false;
    return true;
  }
  if (std::strcmp(value, "1") == 0) {
    outValue = true;
    return true;
  }
  return false;
}

int parseChannel(const char* token) {
  int32_t ch = 0;
  if (!parseI32(token, ch) || ch < 1 || ch > 3) {
    return -1;
  }
  return static_cast<int>(ch - 1);
}

bool splitTwoArgs(const char* args, char* first, size_t firstLen, char* second, size_t secondLen) {
  if (args == nullptr) {
    return false;
  }
  char tmp[MAX_LINE_LEN];
  std::snprintf(tmp, sizeof(tmp), "%s", args);
  char* p = trim(tmp);
  char* space = p;
  while (*space != '\0' && !std::isspace(static_cast<unsigned char>(*space))) {
    ++space;
  }
  if (*space == '\0') {
    return false;
  }
  *space = '\0';
  char* rest = trim(space + 1);
  if (*p == '\0' || *rest == '\0') {
    return false;
  }
  std::snprintf(first, firstLen, "%s", p);
  std::snprintf(second, secondLen, "%s", rest);
  return true;
}

bool splitThreeArgs(const char* args, uint32_t& first, uint32_t& second,
                    uint32_t& third) {
  if (args == nullptr) return false;
  char copy[MAX_LINE_LEN];
  std::snprintf(copy, sizeof(copy), "%s", args);
  char* one = trim(copy);
  char* split1 = std::strchr(one, ' ');
  if (split1 == nullptr) return false;
  *split1 = '\0';
  char* two = trim(split1 + 1);
  char* split2 = std::strchr(two, ' ');
  if (split2 == nullptr) return false;
  *split2 = '\0';
  char* three = trim(split2 + 1);
  return parseU32(one, first) && parseU32(two, second) && parseU32(three, third);
}

bool isValidIna3221Address(uint8_t address) {
  return address >= INA3221_ADDR_MIN && address <= INA3221_ADDR_MAX;
}

bool parseAddress(const char* token, uint8_t& address) {
  uint32_t value = 0;
  if (!parseU32(token, value) || value > 0xFFU ||
      !isValidIna3221Address(static_cast<uint8_t>(value))) {
    return false;
  }
  address = static_cast<uint8_t>(value);
  return true;
}

const char* addressStrap(uint8_t address) {
  switch (address) {
    case 0x40: return "A0=GND";
    case 0x41: return "A0=VS";
    case 0x42: return "A0=SDA";
    case 0x43: return "A0=SCL";
    default: return "invalid";
  }
}

const char* errToStr(INA3221::Err err) {
  using INA3221::Err;
  switch (err) {
    case Err::OK: return "OK";
    case Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case Err::I2C_ERROR: return "I2C_ERROR";
    case Err::TIMEOUT: return "TIMEOUT";
    case Err::INVALID_PARAM: return "INVALID_PARAM";
    case Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case Err::MANUFACTURER_ID_MISMATCH: return "MANUFACTURER_ID_MISMATCH";
    case Err::DIE_ID_MISMATCH: return "DIE_ID_MISMATCH";
    case Err::CONVERSION_NOT_READY: return "CONVERSION_NOT_READY";
    case Err::BUSY: return "BUSY";
    case Err::IN_PROGRESS: return "IN_PROGRESS";
    case Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case Err::I2C_BUS: return "I2C_BUS";
    case Err::JOB_BUSY: return "JOB_BUSY";
    case Err::RESULT_PENDING: return "RESULT_PENDING";
    case Err::NO_RESULT: return "NO_RESULT";
    case Err::CANCELLED: return "CANCELLED";
    case Err::DEADLINE_EXPIRED: return "DEADLINE_EXPIRED";
    case Err::CONFIG_UNKNOWN: return "CONFIG_UNKNOWN";
    case Err::PROFILE_MISMATCH: return "PROFILE_MISMATCH";
    case Err::READ_ONLY_REGISTER: return "READ_ONLY_REGISTER";
    case Err::ARITHMETIC_OVERFLOW: return "ARITHMETIC_OVERFLOW";
    case Err::OUT_OF_RANGE: return "OUT_OF_RANGE";
    case Err::NO_ACTIVE_JOB: return "NO_ACTIVE_JOB";
    case Err::JOB_KIND_MISMATCH: return "JOB_KIND_MISMATCH";
    case Err::CONVERSION_BUSY: return "CONVERSION_BUSY";
    case Err::DEVICE_OFFLINE: return "DEVICE_OFFLINE";
    default: return "UNKNOWN";
  }
}

const char* stateToStr(INA3221::DriverState st) {
  using INA3221::DriverState;
  switch (st) {
    case DriverState::UNINIT: return "UNINIT";
    case DriverState::READY: return "READY";
    case DriverState::DEGRADED: return "DEGRADED";
    case DriverState::OFFLINE: return "OFFLINE";
    default: return "UNKNOWN";
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
             ? "INVERTED" : "NORMAL";
}

const char* modeToStr(INA3221::Mode mode) {
  using INA3221::Mode;
  switch (mode) {
    case Mode::POWER_DOWN: return "POWER_DOWN";
    case Mode::SHUNT_TRIG: return "SHUNT_TRIG";
    case Mode::BUS_TRIG: return "BUS_TRIG";
    case Mode::SHUNT_BUS_TRIG: return "SHUNT_BUS_TRIG";
    case Mode::POWER_DOWN_ALT: return "POWER_DOWN_ALT";
    case Mode::SHUNT_CONT: return "SHUNT_CONT";
    case Mode::BUS_CONT: return "BUS_CONT";
    case Mode::SHUNT_BUS_CONT: return "SHUNT_BUS_CONT";
    default: return "UNKNOWN";
  }
}

const char* avgToStr(INA3221::Averaging avg) {
  using INA3221::Averaging;
  switch (avg) {
    case Averaging::AVG_1: return "1";
    case Averaging::AVG_4: return "4";
    case Averaging::AVG_16: return "16";
    case Averaging::AVG_64: return "64";
    case Averaging::AVG_128: return "128";
    case Averaging::AVG_256: return "256";
    case Averaging::AVG_512: return "512";
    case Averaging::AVG_1024: return "1024";
    default: return "UNKNOWN";
  }
}

const char* ctToStr(INA3221::ConvTime ct) {
  using INA3221::ConvTime;
  switch (ct) {
    case ConvTime::CT_140US: return "140us";
    case ConvTime::CT_204US: return "204us";
    case ConvTime::CT_332US: return "332us";
    case ConvTime::CT_588US: return "588us";
    case ConvTime::CT_1100US: return "1100us";
    case ConvTime::CT_2116US: return "2116us";
    case ConvTime::CT_4156US: return "4156us";
    case ConvTime::CT_8244US: return "8244us";
    default: return "UNKNOWN";
  }
}

void printStatus(const INA3221::Status& st) {
  if (!st.ok() && hilCommandStatus == INA3221::Err::OK) {
    hilCommandStatus = st.code;
  }
  out("  Status: %s (code=%u, detail=%ld)\n",
      errToStr(st.code),
      static_cast<unsigned>(st.code),
      static_cast<long>(st.detail));
  if (st.msg != nullptr && st.msg[0] != '\0') {
    out("  Message: %s\n", st.msg);
  }
}

INA3221::TransportConfig makeOwnerTransport() {
  INA3221::TransportConfig transportConfig{};
  transportConfig.i2cWrite = ina3221IdfI2cWrite;
  transportConfig.i2cWriteRead = ina3221IdfI2cWriteRead;
  transportConfig.i2cUser = &ina3221IdfTransportContext();
  transportConfig.nowMs = ina3221IdfNowMs;
  transportConfig.cooperativeYield = ina3221IdfYield;
  transportConfig.timeUser = &ina3221IdfTransportContext();
  transportConfig.defaultTransferTimeoutMs = I2C_TIMEOUT_MS;
  transportConfig.offlineThreshold = 5U;
  return transportConfig;
}

INA3221::DeviceProfile makeOwnerProfile(uint8_t address) {
  INA3221::DeviceProfile profile{};
  profile.i2cAddress = address;
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

bool initBus(uint8_t address) {
  activeAddress = address;
  if (!ina3221IdfInitI2c(I2C_SDA, I2C_SCL, I2C_FREQ_HZ, I2C_TIMEOUT_MS, address)) {
    error("I2C init failed: %s", esp_err_to_name(ina3221IdfLastError()));
    return false;
  }
  info("I2C initialized SDA=%d SCL=%d addr=0x%02X", I2C_SDA, I2C_SCL, address);
  return true;
}

void printChannelMeasurement(int chNum, const INA3221::ChannelMeasurement& m) {
  out("  CH%d: Vshunt=%.3f mV  Vbus=%.3f V  I=%.3f mA  P=%.3f mW\n",
      chNum,
      static_cast<double>(m.shuntVoltage_mV),
      static_cast<double>(m.busVoltage_V),
      static_cast<double>(m.current_mA),
      static_cast<double>(m.power_mW));
}

void readAllChannels() {
  INA3221::ChannelMeasurement ch1, ch2, ch3;
  INA3221::ChannelMeasurement* p1 =
      device.getChannelEnable(INA3221::Channel::CH1) ? &ch1 : nullptr;
  INA3221::ChannelMeasurement* p2 =
      device.getChannelEnable(INA3221::Channel::CH2) ? &ch2 : nullptr;
  INA3221::ChannelMeasurement* p3 =
      device.getChannelEnable(INA3221::Channel::CH3) ? &ch3 : nullptr;
  INA3221::Status st = device.readBlocking(p1, p2, p3);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  if (p1 != nullptr) printChannelMeasurement(1, ch1);
  if (p2 != nullptr) printChannelMeasurement(2, ch2);
  if (p3 != nullptr) printChannelMeasurement(3, ch3);
}

void printDriverHealth() {
  const uint32_t currentMs = nowMs();
  const uint32_t totalOk = device.totalSuccess();
  const uint32_t totalFail = device.totalFailures();
  const uint32_t total = totalOk + totalFail;
  const float rate = total > 0U ? (100.0f * static_cast<float>(totalOk) / total) : 0.0f;
  out("=== Driver Health ===\n");
  out("  State: %s\n", stateToStr(device.state()));
  out("  Online: %s\n", device.isOnline() ? "YES" : "NO");
  out("  Consecutive failures: %u\n", static_cast<unsigned>(device.consecutiveFailures()));
  out("  Total success: %lu\n", static_cast<unsigned long>(totalOk));
  out("  Total failures: %lu\n", static_cast<unsigned long>(totalFail));
  out("  Success rate: %.1f%%\n", static_cast<double>(rate));
  const uint32_t lastOk = device.lastOkMs();
  if (device.totalSuccess() != 0U) {
    out("  Last OK: %lu ms ago (at %lu ms)\n",
        static_cast<unsigned long>(currentMs - lastOk),
        static_cast<unsigned long>(lastOk));
  } else {
    out("  Last OK: never\n");
  }
  const uint32_t lastErrorMs = device.lastErrorMs();
  const INA3221::Status last = device.lastError();
  if (!last.ok()) {
    out("  Last error: %lu ms ago (at %lu ms)\n",
        static_cast<unsigned long>(currentMs - lastErrorMs),
        static_cast<unsigned long>(lastErrorMs));
    out("  Last error code: %s\n", errToStr(last.code));
    out("  Last error detail: %ld\n", static_cast<long>(last.detail));
    if (last.msg != nullptr && last.msg[0] != '\0') {
      out("  Last error msg: %s\n", last.msg);
    }
  } else {
    out("  Last error: never\n");
  }
}

void printHelpItem(const char* cmd, const char* desc) {
  out("  %-32s %s\n", cmd, desc);
}

void printHelp() {
  out("\n=== INA3221 CLI Help ===\n");
  printHelpItem("help / ?", "Show this help");
  printHelpItem("version / ver", "Print firmware and library version info");
  printHelpItem("scan", "Scan bus and probe 0x40-0x43 for INA3221 IDs");
  printHelpItem("scanina", "Probe only valid INA3221 addresses and IDs");
  printHelpItem("read", "Read all enabled channels");
  printHelpItem("read N", "Read N measurements");
  printHelpItem("ch <1|2|3>", "Read single channel");
  printHelpItem("shunt <1|2|3>", "Read shunt voltage");
  printHelpItem("bus <1|2|3>", "Read bus voltage");
  printHelpItem("current <1|2|3>", "Read current");
  printHelpItem("power <1|2|3>", "Read power");
  printHelpItem("sum", "Read shunt-voltage sum");
  printHelpItem("shuntraw <1|2|3>", "Read raw shunt register");
  printHelpItem("busraw <1|2|3>", "Read raw bus register");
  printHelpItem("sumraw", "Read raw shunt sum register");
  printHelpItem("ids", "Read manufacturer and die IDs");
  printHelpItem("timing", "Show conversion timing");
  printHelpItem("start", "Start single-shot conversion");
  printHelpItem("start <strig|btrig|sbtrig>", "Start selected triggered conversion");
  printHelpItem("poll", "Read conversion-ready flag");
  printHelpItem("job / job progress", "Show cooperative owner-job progress");
  printHelpItem("job init", "Start identity/profile initialization");
  printHelpItem("job apply", "Apply and verify the active desired profile");
  printHelpItem("job reconcile", "Reapply and verify the active profile");
  printHelpItem("job sample", "Start sample using the active profile mode");
  printHelpItem("job continuous [0|1]", "Start continuous sample; optionally consume alerts");
  printHelpItem("job powerdown", "Start verified power-down operation");
  printHelpItem("job cancel", "Cancel active job without I2C");
  printHelpItem("job auto [0|1]", "Show/set automatic one-transfer polling");
  printHelpItem("job step <0..255>", "Poll once with an explicit transfer budget");
  printHelpItem("job result", "Show the last consumed terminal result");
  printHelpItem("job lastsample", "Show the last committed fixed-unit sample");
  printHelpItem("job alerts [take]", "Peek or consume retained alert evidence");
  printHelpItem("mode [pd|pda|strig|btrig|sbtrig|sc|bc|sbc]", "Set/show mode");
  printHelpItem("avg [0..7]", "Set/show averaging");
  printHelpItem("vbusct [0..7]", "Set/show bus conversion time");
  printHelpItem("vshct [0..7]", "Set/show shunt conversion time");
  printHelpItem("chen [<1|2|3> <0|1>]", "Show/set channel enable");
  printHelpItem("rshunt [<1|2|3> <ohms>]", "Show/set shunt resistance");
  printHelpItem("direction [<1|2|3> <0|1>]", "Show/set current sign");
  printHelpItem("profile", "Show complete desired profile and certainty");
  printHelpItem("addr [0x40..0x43]", "Show/select target INA3221 address");
  printHelpItem("init [0x40..0x43]", "Bind and initialize at selected/given address");
  printHelpItem("end", "Unbind driver without touching the bus");
  printHelpItem("freq [10000..400000]", "Show/set I2C frequency in Hz");
  printHelpItem("config", "Dump config register");
  printHelpItem("config write <hex>", "Write full config register");
  printHelpItem("reset", "Software reset");
  printHelpItem("reg <addr>", "Read 16-bit register");
  printHelpItem("wreg <addr> <val>", "Write 16-bit register");
  printHelpItem("alerts", "Read alert flags");
  printHelpItem("alertsnap [take]", "Peek/consume retained alert evidence");
  printHelpItem("mask", "Read/decode Mask/Enable register");
  printHelpItem("crit [<1|2|3> [raw]]", "Show/set critical alert limit");
  printHelpItem("warn [<1|2|3> [raw]]", "Show/set warning alert limit");
  printHelpItem("sumlim [raw]", "Get/set shunt sum limit");
  printHelpItem("pvhi [raw]", "Get/set power-valid upper limit");
  printHelpItem("pvlo [raw]", "Get/set power-valid lower limit");
  printHelpItem("sumch [<1|2|3> <0|1>]", "Show/set summation channels");
  printHelpItem("latch [<warn> <crit>]", "Show/set alert latches");
  printHelpItem("drv", "Show driver state and health");
  printHelpItem("diag", "Show cache-only diagnostics and mismatch evidence");
  printHelpItem("verify", "Read/compare all managed registers; consumes Mask flags");
  printHelpItem("mismatch", "Show cached register verification evidence");
  printHelpItem("probe", "Probe device without health tracking");
  printHelpItem("recover", "Manual recovery attempt");
  printHelpItem("online", "Show online state");
  printHelpItem("cfg / settings", "Print cached configuration snapshot");
  printHelpItem("verbose [0|1]", "Enable/disable verbose output");
  printHelpItem("stress [N]", "Run N measurement cycles");
  printHelpItem("stress_mix [N]", "Run N mixed-operation cycles");
  printHelpItem("stress_owner [N]", "Run N cooperative sample jobs");
  printHelpItem("stress_freq [N]", "Alternate 100/400 kHz with identity reads");
  printHelpItem("hilrun <token> <seq> <cmd>", "Run one framed HIL command");
  printHelpItem("hilmark <token>", "Print an automation marker");
  printHelpItem("xfer_reset", "Reset example transport counters");
  printHelpItem("xfer_stats", "Show example transport counters");
  printHelpItem("xfer_assert <r> <w> <t>", "Assert transfer counts");
  printHelpItem("selftest", "Run safe command self-test report");
  printHelpItem("convert shunt <raw>", "Convert shunt raw to mV");
  printHelpItem("convert bus <raw>", "Convert bus raw to V");
}

void printVersionInfo() {
  out("=== Version Info ===\n");
  out("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  out("  INA3221 library version: %s\n", INA3221::VERSION);
  out("  INA3221 version code: %d (major=%d minor=%d patch=%d)\n",
      INA3221::VERSION_INT,
      INA3221::VERSION_MAJOR,
      INA3221::VERSION_MINOR,
      INA3221::VERSION_PATCH);
}

void printOwnerSample(const INA3221::SampleBatch& sample) {
  out("Owner sample request=%lu profile=%lu enabled=0x%02X valid=0x%02X "
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
    out("  CH%u: shunt=%lduV bus=%ldmV current=%ldmA power=%ldmW quantities=0x%02X\n",
        static_cast<unsigned>(i + 1U),
        static_cast<long>(reading.shuntMicroVolts),
        static_cast<long>(reading.busMilliVolts),
        static_cast<long>(reading.currentMilliAmps),
        static_cast<long>(reading.powerMilliWatts),
        static_cast<unsigned>(reading.validQuantities));
  }
  if (sample.alertSnapshotValid) printAlertSnapshot(sample.alerts, "Sample Alert Evidence");
}

uint64_t ownerDeadlineFor(INA3221::JobKind kind,
                          const INA3221::DeviceProfile& profile,
                          INA3221::Mode sampleMode = INA3221::Mode::SHUNT_BUS_TRIG) {
  const uint64_t current = ownerNowMs();
  uint16_t transfers = 0;
  uint64_t durationMs = OWNER_JOB_TIMEOUT_MS;
  INA3221::Status st = INA3221::INA3221::maximumJobTransfers(kind, profile, transfers);
  if (st.ok()) durationMs = static_cast<uint64_t>(transfers) * I2C_TIMEOUT_MS + 250U;
  if (kind == INA3221::JobKind::TRIGGERED_SAMPLE) {
    uint64_t cycleUs = 0;
    st = INA3221::INA3221::maximumCycleTimeUs(profile, sampleMode, cycleUs);
    if (st.ok()) durationMs += (cycleUs + 999U) / 1000U + 100U;
  }
  if (durationMs < OWNER_JOB_TIMEOUT_MS) durationMs = OWNER_JOB_TIMEOUT_MS;
  return current + durationMs;
}

uint32_t nextOwnerRequestId() {
  ++ownerRequestId;
  if (ownerRequestId == 0U) ++ownerRequestId;
  return ownerRequestId;
}

INA3221::Status trackOwnerStart(const INA3221::Status& st) {
  if (st.inProgress()) {
    ownerDemoPhase = OwnerDemoPhase::ACTIVE;
    ownerAutoService = false;
    startupSamplePending = false;
  }
  return st;
}

void printAlertSnapshot(const INA3221::AlertSnapshot& alerts, const char* title) {
  out("=== %s ===\n", title);
  out("  Raw: 0x%04X events=0x%04X writable=0x%04X\n",
      alerts.raw, alerts.events, alerts.writableBits);
  out("  PowerValid=%u TimingControl=%u ConversionReady=%u EvidenceUncertain=%u\n",
      alerts.powerValid ? 1U : 0U, alerts.timingControl ? 1U : 0U,
      alerts.conversionReady ? 1U : 0U, alerts.evidenceUncertain ? 1U : 0U);
}

void printOwnerResult(const INA3221::JobResult& result) {
  out("=== Owner Job Result ===\n");
  out("  Kind: %s (%u) State: %s (%u)\n",
      jobKindToStr(result.kind), static_cast<unsigned>(result.kind),
      jobStateToStr(result.state), static_cast<unsigned>(result.state));
  out("  Request: %lu Transfers: %u Profile generation: %lu\n",
      static_cast<unsigned long>(result.requestId),
      static_cast<unsigned>(result.transfers),
      static_cast<unsigned long>(result.profileGeneration));
  out("  Hardware effect: %u Sample valid: %u\n",
      static_cast<unsigned>(result.hardwareEffect), result.sampleValid ? 1U : 0U);
  printStatus(result.status);
  if (result.mismatchValid) {
    const uint16_t delta = static_cast<uint16_t>(
        (result.mismatchActual ^ result.mismatchExpected) & result.mismatchMask);
    out("  Register mismatch: reg=0x%02X actual=0x%04X expected=0x%04X "
        "mask=0x%04X delta=0x%04X\n",
        result.mismatchRegister, result.mismatchActual, result.mismatchExpected,
        result.mismatchMask, delta);
  }
  if (result.sampleValid) printOwnerSample(result.sample);
}

INA3221::Status startOwnerSample() {
  const uint32_t requestId = nextOwnerRequestId();
  const INA3221::Mode mode = device.deviceProfile().mode;
  INA3221::Status st;
  if (mode == INA3221::Mode::SHUNT_TRIG || mode == INA3221::Mode::BUS_TRIG ||
      mode == INA3221::Mode::SHUNT_BUS_TRIG) {
    st = device.startTriggeredSample(
        mode, requestId,
        ownerDeadlineFor(INA3221::JobKind::TRIGGERED_SAMPLE,
                         device.deviceProfile(), mode));
  } else if (mode == INA3221::Mode::SHUNT_CONT || mode == INA3221::Mode::BUS_CONT ||
             mode == INA3221::Mode::SHUNT_BUS_CONT) {
    st = device.startContinuousSample(
        requestId,
        ownerDeadlineFor(INA3221::JobKind::CONTINUOUS_SAMPLE,
                         device.deviceProfile()), false);
  } else {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM,
                                  "Active profile is powered down");
  }
  if (st.inProgress()) ownerDemoPhase = OwnerDemoPhase::ACTIVE;
  return st;
}

void serviceOwnerJob() {
  if (ownerDemoPhase == OwnerDemoPhase::IDLE) return;

  INA3221::JobProgress progress{};
  (void)device.getJobProgress(progress);
  if (progress.state == INA3221::JobTerminalState::ACTIVE && ownerAutoService) {
    INA3221::PollContext context{};
    context.nowMs = ownerNowMs();
    context.deadlineMs = progress.deadlineMs;
    context.transferTimeoutMs = I2C_TIMEOUT_MS;
    context.maxTransfers = 1U;  // At most one synchronous callback per service.
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
  const uint64_t current = ownerNowMs();
  const uint64_t remaining = progress.deadlineMs > current
                                 ? progress.deadlineMs - current : 0U;
  out("=== Owner Job Progress ===\n");
  out("  Kind: %s (%u) Stage: %s (%u) State: %s (%u)\n",
      jobKindToStr(progress.kind), static_cast<unsigned>(progress.kind),
      jobStageToStr(progress.stage), static_cast<unsigned>(progress.stage),
      jobStateToStr(progress.state), static_cast<unsigned>(progress.state));
  out("  Request: %lu Transfers: %u Last poll: %u Result pending: %u\n",
      static_cast<unsigned long>(progress.requestId),
      static_cast<unsigned>(progress.totalTransfers),
      static_cast<unsigned>(progress.lastPollTransfers),
      progress.resultPending ? 1U : 0U);
  out("  Now: %llu Deadline: %llu Remaining: %llu ms Ready at: %llu\n",
      static_cast<unsigned long long>(current),
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
  context.transferTimeoutMs = I2C_TIMEOUT_MS;
  context.maxTransfers = maxTransfers;
  st = device.pollJob(context);
  ownerAutoService = false;
  serviceOwnerJob();
  return st;
}

void printConfig() {
  uint16_t config = 0;
  INA3221::Status st = device.readConfig(config);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  out("  Config: 0x%04X\n", config);
  out("  Mode: %s\n", modeToStr(device.getMode()));
  out("  Averaging: %s samples\n", avgToStr(device.getAveraging()));
  out("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  out("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  out("  CH1: %s  CH2: %s  CH3: %s\n",
      device.getChannelEnable(INA3221::Channel::CH1) ? "ON" : "OFF",
      device.getChannelEnable(INA3221::Channel::CH2) ? "ON" : "OFF",
      device.getChannelEnable(INA3221::Channel::CH3) ? "ON" : "OFF");
  out("  Rshunt: CH1=%.4f  CH2=%.4f  CH3=%.4f ohm\n",
      static_cast<double>(device.getShuntResistance(INA3221::Channel::CH1)),
      static_cast<double>(device.getShuntResistance(INA3221::Channel::CH2)),
      static_cast<double>(device.getShuntResistance(INA3221::Channel::CH3)));
  out("  Cycle time: %lu us\n", static_cast<unsigned long>(device.getCycleTimeUs()));
}

void printSettingsSnapshot() {
  INA3221::SettingsSnapshot snap;
  INA3221::Status st = device.getSettings(snap);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  out("=== Cached Settings ===\n");
  out("  Initialized: %s\n", snap.initialized ? "YES" : "NO");
  out("  State: %s\n", stateToStr(snap.state));
  out("  Address: 0x%02X\n", snap.i2cAddress);
  out("  I2C timeout: %lu ms\n", static_cast<unsigned long>(snap.i2cTimeoutMs));
  out("  Offline threshold: %u\n", static_cast<unsigned>(snap.offlineThreshold));
  out("  Hooks: nowMs=%s yield=%s\n",
      snap.hasNowMsHook ? "YES" : "NO",
      snap.hasCooperativeYieldHook ? "YES" : "NO");
  out("  Mode: %s\n", modeToStr(snap.mode));
  out("  Averaging: %s samples\n", avgToStr(snap.averaging));
  out("  VbusCT: %s\n", ctToStr(snap.vBusCt));
  out("  VshCT: %s\n", ctToStr(snap.vShCt));
  out("  Channels: CH1=%s  CH2=%s  CH3=%s\n",
      snap.ch1Enable ? "ON" : "OFF",
      snap.ch2Enable ? "ON" : "OFF",
      snap.ch3Enable ? "ON" : "OFF");
  out("  Rshunt: CH1=%.4f  CH2=%.4f  CH3=%.4f ohm\n",
      static_cast<double>(snap.shuntResistance[0]),
      static_cast<double>(snap.shuntResistance[1]),
      static_cast<double>(snap.shuntResistance[2]));
  out("  Conversion: started=%s ready=%s start=%lu ms\n",
      snap.conversionStarted ? "YES" : "NO",
      snap.conversionReady ? "YES" : "NO",
      static_cast<unsigned long>(snap.conversionStartMs));
  out("  Mask/Enable writable cache: 0x%04X\n", snap.maskEnableWritableCache);
  out("  Hardware config dirty: %s\n", snap.hardwareConfigDirty ? "YES" : "NO");
  if (snap.hardwareConfigDirty) {
    out("  Dirty reason: %s (code=%u detail=%ld)\n",
        errToStr(snap.hardwareConfigDirtyStatus.code),
        static_cast<unsigned>(snap.hardwareConfigDirtyStatus.code),
        static_cast<long>(snap.hardwareConfigDirtyStatus.detail));
    if (snap.hardwareConfigDirtyStatus.msg != nullptr &&
        snap.hardwareConfigDirtyStatus.msg[0] != '\0') {
      out("  Dirty message: %s\n", snap.hardwareConfigDirtyStatus.msg);
    }
  }
}

void printDesiredProfile() {
  if (!device.isBound() && !retainedProfileValid) {
    out("  No retained profile\n");
    return;
  }
  const INA3221::DeviceProfile& profile =
      device.isBound() ? device.deviceProfile() : retainedProfile;
  out("=== Desired Device Profile ===\n");
  out("  Bound: %s Initialized: %s Address: 0x%02X\n",
      device.isBound() ? "YES" : "NO", device.isInitialized() ? "YES" : "NO",
      profile.i2cAddress);
  out("  Generation: %lu Measurement state: %s Alert state: %s\n",
      static_cast<unsigned long>(device.profileGeneration()),
      appliedStateToStr(device.measurementConfigState()),
      appliedStateToStr(device.alertConfigState()));
  out("  Mode: %s AVG: %s VBUSCT: %s VSHCT: %s Channels=0x%02X\n",
      modeToStr(profile.mode), avgToStr(profile.averaging), ctToStr(profile.vBusCt),
      ctToStr(profile.vShCt), static_cast<unsigned>(profile.enabledChannels));
  for (uint8_t i = 0; i < 3U; ++i) {
    out("  CH%u calibration: %lu uohm (%.6f ohm), direction=%s\n",
        static_cast<unsigned>(i + 1U),
        static_cast<unsigned long>(profile.shunts[i].resistanceMicroOhms),
        static_cast<double>(profile.shunts[i].resistanceMicroOhms) / 1000000.0,
        directionToStr(profile.shunts[i].direction));
    out("  CH%u limits: critical=%ld uV warning=%ld uV\n",
        static_cast<unsigned>(i + 1U),
        static_cast<long>(profile.alerts.criticalLimitMicroVolts[i]),
        static_cast<long>(profile.alerts.warningLimitMicroVolts[i]));
  }
  out("  Sum channels=0x%02X Sum limit=%ld uV\n",
      static_cast<unsigned>(profile.alerts.summationChannels),
      static_cast<long>(profile.alerts.shuntSumLimitMicroVolts));
  out("  Power-valid window: %lu..%lu mV Latch: warning=%u critical=%u\n",
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
  out("=== Managed Register Verification Evidence ===\n");
  if (!lastVerification.valid) {
    out("  No verification captured; run 'verify'\n");
    return;
  }
  out("  Captured at: %lu ms Address: 0x%02X Profile generation: %lu\n",
      static_cast<unsigned long>(lastVerification.capturedAtMs),
      lastVerification.i2cAddress,
      static_cast<unsigned long>(lastVerification.profileGeneration));
  out("  Registers: %u Mismatches: %u Read failures: %u\n",
      static_cast<unsigned>(lastVerification.count),
      static_cast<unsigned>(lastVerification.mismatches),
      static_cast<unsigned>(lastVerification.readFailures));
  for (uint8_t i = 0; i < lastVerification.count; ++i) {
    const RegisterEvidence& item = lastVerification.registers[i];
    if (!item.readOk) {
      out("  REG 0x%02X %-11s READ_ERROR code=%s detail=%ld msg=%s\n",
          item.reg, registerName(item.reg), errToStr(item.status.code),
          static_cast<long>(item.status.detail), item.status.msg ? item.status.msg : "");
      continue;
    }
    const uint16_t delta = static_cast<uint16_t>((item.actual ^ item.expected) & item.compareMask);
    out("  REG 0x%02X %-11s actual=0x%04X expected=0x%04X mask=0x%04X delta=0x%04X %s\n",
        item.reg, registerName(item.reg), item.actual, item.expected,
        item.compareMask, delta, delta == 0U ? "MATCH" : "MISMATCH");
  }
  out("  VERIFY_RESULT: %s\n",
      lastVerification.mismatches == 0U && lastVerification.readFailures == 0U
          ? "PASS" : "FAIL");
}

INA3221::Status verifyManagedRegisters() {
  lastVerification = VerificationEvidence{};
  lastVerification.valid = true;
  lastVerification.capturedAtMs = nowMs();
  lastVerification.i2cAddress = device.isBound()
                                    ? device.deviceProfile().i2cAddress
                                    : selectedAddress;
  lastVerification.profileGeneration = device.profileGeneration();
  lastVerification.count = 11U;
  const INA3221::DeviceProfile verificationProfile =
      device.isBound() ? device.deviceProfile()
                       : (retainedProfileValid ? retainedProfile
                                               : makeOwnerProfile(selectedAddress));
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
    } else if (((item.actual ^ item.expected) & item.compareMask) != 0U) {
      if (firstMismatch == 0xFFU) firstMismatch = item.reg;
      ++lastVerification.mismatches;
    }
  }
  if (lastVerification.readFailures != 0U) {
    return firstReadError;
  }
  if (lastVerification.mismatches != 0U) {
    return INA3221::Status::Error(INA3221::Err::PROFILE_MISMATCH,
                                  "Managed register mismatch", firstMismatch);
  }
  return INA3221::Status::Ok();
}

void printFullDiagnostics() {
  out("=== Full INA3221 Diagnostics (cache-only) ===\n");
  out("  Selected address: 0x%02X Active address: 0x%02X I2C frequency: %lu Hz\n",
      selectedAddress, activeAddress,
      static_cast<unsigned long>(ina3221IdfTransportContext().frequencyHz));
  printDriverHealth();
  printDesiredProfile();
  printOwnerJobProgress();
  if (lastOwnerResultValid) printOwnerResult(lastOwnerResult);
  else out("  Last owner result: none\n");
  printVerificationEvidence();
  const Ina3221IdfTransferStats stats = ina3221IdfTransferStats();
  out("  Transport transfers: read=%lu write=%lu total=%lu\n",
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
  out("=== Mask/Enable Register ===\n");
  out("  Raw: 0x%04X\n", raw);
  out("  Sum channels: CH1=%s  CH2=%s  CH3=%s\n",
      (raw & INA3221::cmd::MASK_SCC1) ? "ON" : "OFF",
      (raw & INA3221::cmd::MASK_SCC2) ? "ON" : "OFF",
      (raw & INA3221::cmd::MASK_SCC3) ? "ON" : "OFF");
  out("  Latch: warning=%s  critical=%s\n",
      (raw & INA3221::cmd::MASK_WEN) ? "ON" : "OFF",
      (raw & INA3221::cmd::MASK_CEN) ? "ON" : "OFF");
  out("  Critical flags: CH1=%d CH2=%d CH3=%d\n",
      (raw & INA3221::cmd::MASK_CF1) != 0,
      (raw & INA3221::cmd::MASK_CF2) != 0,
      (raw & INA3221::cmd::MASK_CF3) != 0);
  out("  Warning flags: CH1=%d CH2=%d CH3=%d\n",
      (raw & INA3221::cmd::MASK_WF1) != 0,
      (raw & INA3221::cmd::MASK_WF2) != 0,
      (raw & INA3221::cmd::MASK_WF3) != 0);
  out("  SF=%d PVF=%d TCF=%d CVRF=%d\n",
      (raw & INA3221::cmd::MASK_SF) != 0,
      (raw & INA3221::cmd::MASK_PVF) != 0,
      (raw & INA3221::cmd::MASK_TCF) != 0,
      (raw & INA3221::cmd::MASK_CVRF) != 0);
}

void printTimingInfo() {
  out("=== Timing Info ===\n");
  out("  Conversion time: %lu us\n", static_cast<unsigned long>(device.getConversionTimeUs()));
  out("  Cycle time: %lu us\n", static_cast<unsigned long>(device.getCycleTimeUs()));
  out("  Averaging: %s samples\n", avgToStr(device.getAveraging()));
  out("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  out("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  out("  Shunt LSB: 0.04 mV\n");
  out("  Bus LSB: 8 mV\n");
}

void printLimit(bool critical, INA3221::Channel ch, int chNum) {
  int16_t raw = 0;
  INA3221::Status st = critical ? device.getCriticalAlertLimit(ch, raw)
                                : device.getWarningAlertLimit(ch, raw);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  out("  CH%d %s limit: %d (%.3f mV)\n",
      chNum,
      critical ? "critical" : "warning",
      raw,
      static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)));
}

void printAllLimits(bool critical) {
  printLimit(critical, INA3221::Channel::CH1, 1);
  printLimit(critical, INA3221::Channel::CH2, 2);
  printLimit(critical, INA3221::Channel::CH3, 3);
}

void printChannelEnable() {
  out("  Channels: CH1=%s CH2=%s CH3=%s\n",
      device.getChannelEnable(INA3221::Channel::CH1) ? "ON" : "OFF",
      device.getChannelEnable(INA3221::Channel::CH2) ? "ON" : "OFF",
      device.getChannelEnable(INA3221::Channel::CH3) ? "ON" : "OFF");
}

void printShuntResistance() {
  out("  Rshunt: CH1=%.4f CH2=%.4f CH3=%.4f ohm\n",
      static_cast<double>(device.getShuntResistance(INA3221::Channel::CH1)),
      static_cast<double>(device.getShuntResistance(INA3221::Channel::CH2)),
      static_cast<double>(device.getShuntResistance(INA3221::Channel::CH3)));
}

void scanBus() {
  out("I2C scan:\n");
  uint8_t count = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    INA3221::Status st = ina3221IdfProbeAddress(address, I2C_TIMEOUT_MS);
    if (st.ok()) {
      ++count;
      out("  0x%02X", address);
      uint8_t reg = INA3221::cmd::REG_MANUFACTURER_ID;
      uint8_t rx[2] = {0, 0};
      st = ina3221IdfI2cWriteReadAt(address, &reg, 1, rx, sizeof(rx), I2C_TIMEOUT_MS,
                                    &ina3221IdfTransportContext());
      if (st.ok()) {
        const uint16_t mfg = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
        out(" mfg=0x%04X", mfg);
      }
      reg = INA3221::cmd::REG_DIE_ID;
      st = ina3221IdfI2cWriteReadAt(address, &reg, 1, rx, sizeof(rx), I2C_TIMEOUT_MS,
                                    &ina3221IdfTransportContext());
      if (st.ok()) {
        const uint16_t die = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
        out(" die=0x%04X", die);
      }
      out("\n");
    }
  }
  out("Found %u device(s)\n", static_cast<unsigned>(count));
}

INA3221::Status readRegisterAt(uint8_t address, uint8_t reg, uint16_t& value) {
  uint8_t rx[2] = {0, 0};
  const INA3221::Status st = ina3221IdfI2cWriteReadAt(
      address, &reg, 1U, rx, sizeof(rx), I2C_TIMEOUT_MS,
      &ina3221IdfTransportContext());
  if (!st.ok()) return st;
  value = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8U) | rx[1]);
  return INA3221::Status::Ok();
}

void scanIna3221Addresses() {
  out("=== INA3221 Address Probe (raw, no driver-health tracking) ===\n");
  uint8_t healthy = 0;
  for (uint8_t address = INA3221_ADDR_MIN; address <= INA3221_ADDR_MAX; ++address) {
    out("  0x%02X (%s): ", address, addressStrap(address));
    INA3221::Status st = ina3221IdfProbeAddress(address, I2C_TIMEOUT_MS);
    if (!st.ok()) {
      out("NO_ACK code=%s detail=%ld\n", errToStr(st.code),
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
      out("ACK ID_READ_ERROR code=%s detail=%ld msg=%s\n", errToStr(failed.code),
          static_cast<long>(failed.detail), failed.msg ? failed.msg : "");
      continue;
    }
    const bool match = manufacturer == INA3221::cmd::MANUFACTURER_ID_VALUE &&
                       die == INA3221::cmd::DIE_ID_VALUE;
    out("ACK manufacturer=0x%04X die=0x%04X %s\n",
        manufacturer, die, match ? "HEALTHY_INA3221" : "ID_MISMATCH");
    if (match) ++healthy;
  }
  out("  Healthy INA3221 devices: %u\n", static_cast<unsigned>(healthy));
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
  const uint8_t previousAddress = activeAddress;
  const INA3221::DeviceProfile previousProfile = retainedProfile;
  INA3221::DeviceProfile profile = retainedProfileValid
                                       ? retainedProfile
                                       : makeOwnerProfile(address);
  profile.i2cAddress = address;
  st = ina3221IdfSetAddress(address);
  if (!st.ok()) {
    if (ina3221IdfTransportContext().dev == nullptr) {
      device.unbind();
      ownerDemoPhase = OwnerDemoPhase::IDLE;
    }
    return st;
  }
  device.unbind();
  ownerDemoPhase = OwnerDemoPhase::IDLE;
  activeAddress = address;
  selectedAddress = address;
  retainedProfile = profile;
  retainedProfileValid = true;
  st = device.bind(makeOwnerTransport(), profile);
  if (!st.ok()) {
    const INA3221::Status restore = ina3221IdfSetAddress(previousAddress);
    activeAddress = previousAddress;
    selectedAddress = previousAddress;
    retainedProfile = previousProfile;
    if (!restore.ok()) return restore;
    const INA3221::Status rebind = device.bind(makeOwnerTransport(), previousProfile);
    if (!rebind.ok()) return rebind;
    return st;
  }
  st = device.startInitialize(
      nextOwnerRequestId(),
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
  const uint32_t previous = ina3221IdfTransportContext().frequencyHz;
  st = ina3221IdfSetFrequency(frequencyHz);
  if (!st.ok()) {
    if (ina3221IdfTransportContext().dev == nullptr) {
      retainCurrentProfile();
      device.unbind();
      ownerDemoPhase = OwnerDemoPhase::IDLE;
    }
    return st;
  }
  if (!device.isInitialized()) return INA3221::Status::Ok();
  uint16_t manufacturer = 0;
  uint16_t die = 0;
  st = readRegisterAt(activeAddress, INA3221::cmd::REG_MANUFACTURER_ID, manufacturer);
  if (st.ok()) st = readRegisterAt(activeAddress, INA3221::cmd::REG_DIE_ID, die);
  if (st.ok() && manufacturer != INA3221::cmd::MANUFACTURER_ID_VALUE) {
    st = INA3221::Status::Error(INA3221::Err::MANUFACTURER_ID_MISMATCH,
                                "Manufacturer ID mismatch", manufacturer);
  }
  if (st.ok() && die != INA3221::cmd::DIE_ID_VALUE) {
    st = INA3221::Status::Error(INA3221::Err::DIE_ID_MISMATCH,
                                "Die ID mismatch", die);
  }
  if (!st.ok()) {
    const INA3221::Status restore = ina3221IdfSetFrequency(previous);
    if (!restore.ok()) {
      retainCurrentProfile();
      device.unbind();
      ownerDemoPhase = OwnerDemoPhase::IDLE;
      return INA3221::Status::Error(INA3221::Err::I2C_BUS,
                                    "Frequency verification and rollback failed",
                                    restore.detail);
    }
    return st;
  }
  return INA3221::Status::Ok();
}

void resetStressStats(int target) {
  stressStats.startMs = nowMs();
  stressStats.endMs = 0;
  stressStats.target = target;
  stressStats.attempts = 0;
  stressStats.success = 0;
  stressStats.errors = 0;
  stressStats.lastError = INA3221::Status::Ok();
  for (int ch = 0; ch < 3; ++ch) {
    ChannelStressStats& s = stressStats.channels[ch];
    s.enabled = device.getChannelEnable(static_cast<INA3221::Channel>(ch));
    s.hasSample = false;
    s.minVshuntMv = std::numeric_limits<float>::max();
    s.maxVshuntMv = std::numeric_limits<float>::lowest();
    s.minVbusV = std::numeric_limits<float>::max();
    s.maxVbusV = std::numeric_limits<float>::lowest();
    s.minCurrentMa = std::numeric_limits<float>::max();
    s.maxCurrentMa = std::numeric_limits<float>::lowest();
    s.minPowerMw = std::numeric_limits<float>::max();
    s.maxPowerMw = std::numeric_limits<float>::lowest();
    s.sumVshuntMv = 0.0;
    s.sumVbusV = 0.0;
    s.sumCurrentMa = 0.0;
    s.sumPowerMw = 0.0;
  }
}

void updateChannelStressStats(ChannelStressStats& s, const INA3221::ChannelMeasurement& m) {
  if (!s.hasSample) {
    s.minVshuntMv = s.maxVshuntMv = m.shuntVoltage_mV;
    s.minVbusV = s.maxVbusV = m.busVoltage_V;
    s.minCurrentMa = s.maxCurrentMa = m.current_mA;
    s.minPowerMw = s.maxPowerMw = m.power_mW;
    s.hasSample = true;
  } else {
    if (m.shuntVoltage_mV < s.minVshuntMv) s.minVshuntMv = m.shuntVoltage_mV;
    if (m.shuntVoltage_mV > s.maxVshuntMv) s.maxVshuntMv = m.shuntVoltage_mV;
    if (m.busVoltage_V < s.minVbusV) s.minVbusV = m.busVoltage_V;
    if (m.busVoltage_V > s.maxVbusV) s.maxVbusV = m.busVoltage_V;
    if (m.current_mA < s.minCurrentMa) s.minCurrentMa = m.current_mA;
    if (m.current_mA > s.maxCurrentMa) s.maxCurrentMa = m.current_mA;
    if (m.power_mW < s.minPowerMw) s.minPowerMw = m.power_mW;
    if (m.power_mW > s.maxPowerMw) s.maxPowerMw = m.power_mW;
  }
  s.sumVshuntMv += m.shuntVoltage_mV;
  s.sumVbusV += m.busVoltage_V;
  s.sumCurrentMa += m.current_mA;
  s.sumPowerMw += m.power_mW;
}

void updateStressStats(const INA3221::ChannelMeasurement* ch1,
                       const INA3221::ChannelMeasurement* ch2,
                       const INA3221::ChannelMeasurement* ch3) {
  const INA3221::ChannelMeasurement* samples[3] = {ch1, ch2, ch3};
  for (int ch = 0; ch < 3; ++ch) {
    if (samples[ch] != nullptr) {
      updateChannelStressStats(stressStats.channels[ch], *samples[ch]);
    }
  }
  ++stressStats.success;
}

void printStressSummary() {
  stressStats.endMs = nowMs();
  const uint32_t elapsed = stressStats.endMs - stressStats.startMs;
  out("=== Stress Summary ===\n");
  out("  Target: %d\n", stressStats.target);
  out("  Attempts: %d\n", stressStats.attempts);
  out("  Success: %d\n", stressStats.success);
  out("  Errors: %lu\n", static_cast<unsigned long>(stressStats.errors));
  out("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  if (elapsed > 0U) {
    out("  Rate: %.2f samples/s\n",
        static_cast<double>(1000.0f * static_cast<float>(stressStats.attempts) / elapsed));
  }
  for (int ch = 0; ch < 3; ++ch) {
    const ChannelStressStats& s = stressStats.channels[ch];
    if (!s.enabled) {
      out("  CH%d: disabled\n", ch + 1);
      continue;
    }
    if (!s.hasSample || stressStats.success <= 0) {
      out("  CH%d: no valid samples\n", ch + 1);
      continue;
    }
    out("  CH%d Vshunt mV: min=%.3f avg=%.3f max=%.3f\n",
        ch + 1,
        static_cast<double>(s.minVshuntMv),
        static_cast<double>(s.sumVshuntMv / stressStats.success),
        static_cast<double>(s.maxVshuntMv));
    out("  CH%d Vbus V: min=%.3f avg=%.3f max=%.3f\n",
        ch + 1,
        static_cast<double>(s.minVbusV),
        static_cast<double>(s.sumVbusV / stressStats.success),
        static_cast<double>(s.maxVbusV));
    out("  CH%d Current mA: min=%.3f avg=%.3f max=%.3f\n",
        ch + 1,
        static_cast<double>(s.minCurrentMa),
        static_cast<double>(s.sumCurrentMa / stressStats.success),
        static_cast<double>(s.maxCurrentMa));
    out("  CH%d Power mW: min=%.3f avg=%.3f max=%.3f\n",
        ch + 1,
        static_cast<double>(s.minPowerMw),
        static_cast<double>(s.sumPowerMw / stressStats.success),
        static_cast<double>(s.maxPowerMw));
  }
  if (!stressStats.lastError.ok()) {
    if (hilCommandStatus == INA3221::Err::OK) {
      hilCommandStatus = stressStats.lastError.code;
    }
    out("  Last error: %s\n", errToStr(stressStats.lastError.code));
    out("  Detail: %ld\n", static_cast<long>(stressStats.lastError.detail));
    if (stressStats.lastError.msg != nullptr && stressStats.lastError.msg[0] != '\0') {
      out("  Message: %s\n", stressStats.lastError.msg);
    }
  }
}

void runStress(int count) {
  resetStressStats(count);
  for (int i = 0; i < count; ++i) {
    INA3221::ChannelMeasurement ch1, ch2, ch3;
    INA3221::ChannelMeasurement* p1 =
        device.getChannelEnable(INA3221::Channel::CH1) ? &ch1 : nullptr;
    INA3221::ChannelMeasurement* p2 =
        device.getChannelEnable(INA3221::Channel::CH2) ? &ch2 : nullptr;
    INA3221::ChannelMeasurement* p3 =
        device.getChannelEnable(INA3221::Channel::CH3) ? &ch3 : nullptr;
    INA3221::Status st = device.readBlocking(p1, p2, p3);
    ++stressStats.attempts;
    if (st.ok()) {
      updateStressStats(p1, p2, p3);
      if (verboseMode) {
        out("  [%d]", i + 1);
        if (p1 != nullptr) out(" CH1=%.1fmA", static_cast<double>(ch1.current_mA));
        if (p2 != nullptr) out(" CH2=%.1fmA", static_cast<double>(ch2.current_mA));
        if (p3 != nullptr) out(" CH3=%.1fmA", static_cast<double>(ch3.current_mA));
        out("\n");
      }
    } else {
      ++stressStats.errors;
      stressStats.lastError = st;
      if (verboseMode) {
        printStatus(st);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  printStressSummary();
}

void runStressMix(int count) {
  struct OpStats {
    const char* name;
    uint32_t ok;
    uint32_t fail;
  };
  OpStats stats[] = {
      {"readBlocking", 0, 0},
      {"readConfig", 0, 0},
      {"mfgId", 0, 0},
      {"shuntCh1", 0, 0},
      {"busCh2", 0, 0},
      {"alerts", 0, 0},
  };
  const uint32_t successBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint32_t start = nowMs();
  for (int i = 0; i < count; ++i) {
    INA3221::ChannelMeasurement ch1, ch2, ch3;
    INA3221::ChannelMeasurement* p1 =
        device.getChannelEnable(INA3221::Channel::CH1) ? &ch1 : nullptr;
    INA3221::ChannelMeasurement* p2 =
        device.getChannelEnable(INA3221::Channel::CH2) ? &ch2 : nullptr;
    INA3221::ChannelMeasurement* p3 =
        device.getChannelEnable(INA3221::Channel::CH3) ? &ch3 : nullptr;
    INA3221::Status st = device.readBlocking(p1, p2, p3);
    st.ok() ? ++stats[0].ok : ++stats[0].fail;
    uint16_t cfg = 0;
    st = device.readConfig(cfg);
    st.ok() ? ++stats[1].ok : ++stats[1].fail;
    uint16_t mfg = 0;
    st = device.readManufacturerId(mfg);
    st.ok() ? ++stats[2].ok : ++stats[2].fail;
    float shunt = 0.0f;
    st = device.readShuntVoltage(INA3221::Channel::CH1, shunt);
    st.ok() ? ++stats[3].ok : ++stats[3].fail;
    float bus = 0.0f;
    st = device.readBusVoltage(INA3221::Channel::CH2, bus);
    st.ok() ? ++stats[4].ok : ++stats[4].fail;
    INA3221::AlertFlags flags;
    st = device.readAlertFlags(flags);
    st.ok() ? ++stats[5].ok : ++stats[5].fail;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  const uint32_t elapsed = nowMs() - start;
  uint32_t ok = 0;
  uint32_t fail = 0;
  out("=== stress_mix summary ===\n");
  for (const auto& stat : stats) {
    ok += stat.ok;
    fail += stat.fail;
    out("  %-12s ok=%lu fail=%lu\n",
        stat.name,
        static_cast<unsigned long>(stat.ok),
        static_cast<unsigned long>(stat.fail));
  }
  out("  Total: ok=%lu fail=%lu\n", static_cast<unsigned long>(ok), static_cast<unsigned long>(fail));
  out("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  out("  Health delta: success +%lu failures +%lu\n",
      static_cast<unsigned long>(device.totalSuccess() - successBefore),
      static_cast<unsigned long>(device.totalFailures() - failBefore));
  if (fail != 0U && hilCommandStatus == INA3221::Err::OK) {
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
    context.transferTimeoutMs = I2C_TIMEOUT_MS;
    context.maxTransfers = 1U;
    st = device.pollJob(context);
    if (!st.ok() && !st.inProgress()) {
      (void)device.getJobProgress(progress);
      if (!progress.resultPending) break;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
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
  INA3221::Status st = INA3221::INA3221::maximumJobTransfers(
      kind, device.deviceProfile(), maximumTransfers);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  const uint32_t start = nowMs();
  for (int i = 0; i < count; ++i) {
    INA3221::JobResult result{};
    const uint32_t expectedRequest = ownerRequestId == UINT32_MAX ? 1U : ownerRequestId + 1U;
    st = runOneOwnerSample(result);
    const bool valid = st.ok() && result.requestId == expectedRequest &&
                       result.profileGeneration == device.profileGeneration() &&
                       result.sample.validChannels == device.deviceProfile().enabledChannels &&
                       result.transfers <= maximumTransfers;
    if (valid) ++passed;
    else {
      ++failed;
      break;
    }
  }
  out("=== stress_owner summary ===\n");
  out("  Target: %d pass=%lu fail=%lu max_transfers=%u duration=%lu ms\n",
      count, static_cast<unsigned long>(passed), static_cast<unsigned long>(failed),
      static_cast<unsigned>(maximumTransfers),
      static_cast<unsigned long>(nowMs() - start));
  if (failed != 0U) {
    printStatus(st.ok() ? INA3221::Status::Error(
                             INA3221::Err::PROFILE_MISMATCH,
                             "Owner stress validation failed", failed)
                        : st);
  }
}

void runFrequencyStress(int count) {
  const uint32_t initialFrequency = ina3221IdfTransportContext().frequencyHz;
  uint32_t passed = 0;
  uint32_t failed = 0;
  INA3221::Status last = INA3221::Status::Ok();
  for (int i = 0; i < count; ++i) {
    last = setExampleFrequency((i & 1) == 0 ? 100000U : 400000U);
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
  out("=== stress_freq summary ===\n");
  out("  Target: %d pass=%lu fail=%lu restored_hz=%lu\n",
      count, static_cast<unsigned long>(passed), static_cast<unsigned long>(failed),
      static_cast<unsigned long>(ina3221IdfTransportContext().frequencyHz));
  if (failed != 0U) printStatus(last);
}

void runSelfTest() {
  uint32_t pass = 0;
  uint32_t fail = 0;
  uint32_t skip = 0;
  auto report = [&](const char* name, bool ok, const char* note) {
    out("  [%s] %s", ok ? "PASS" : "FAIL", name);
    if (note != nullptr && note[0] != '\0') {
      out(" - %s", note);
    }
    out("\n");
    ok ? ++pass : ++fail;
  };
  auto reportSkip = [&](const char* name, const char* note) {
    out("  [SKIP] %s - %s\n", name, note);
    ++skip;
  };

  out("=== INA3221 selftest (configuration-preserving diagnostics) ===\n");
  out("  Note: managed-register verification reads Mask/Enable and retains its read-clear alert evidence.\n");
  const INA3221::Status idle = requireIdleOwnerJob();
  if (!idle.ok()) {
    reportSkip("all checks", "owner job active");
    printStatus(idle);
    out("Selftest result: pass=0 fail=0 skip=1\n");
    return;
  }
  const INA3221::DeviceProfile profileBefore =
      device.isBound() ? device.deviceProfile() : INA3221::DeviceProfile{};
  const uint32_t generationBefore = device.profileGeneration();
  const uint32_t succBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint8_t consBefore = device.consecutiveFailures();
  INA3221::Status st = device.probe();
  if (st.code == INA3221::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    out("Selftest result: pass=%lu fail=%lu skip=%lu\n",
        static_cast<unsigned long>(pass),
        static_cast<unsigned long>(fail),
        static_cast<unsigned long>(skip));
    return;
  }
  report("probe responds", st.ok(), st.ok() ? "" : errToStr(st.code));
  report("probe no-health-side-effects",
         device.totalSuccess() == succBefore &&
             device.totalFailures() == failBefore &&
             device.consecutiveFailures() == consBefore,
         "");
  uint16_t cfg = 0;
  st = device.readConfig(cfg);
  report("readConfig", st.ok(), st.ok() ? "" : errToStr(st.code));
  uint16_t mfg = 0;
  st = device.readManufacturerId(mfg);
  report("readManufacturerId", st.ok() && mfg == 0x5449, st.ok() ? "" : errToStr(st.code));
  uint16_t die = 0;
  st = device.readDieId(die);
  report("readDieId", st.ok() && die == 0x3220, st.ok() ? "" : errToStr(st.code));
  const bool poweredDown = profileBefore.mode == INA3221::Mode::POWER_DOWN ||
                           profileBefore.mode == INA3221::Mode::POWER_DOWN_ALT;
  for (uint8_t i = 0; i < 3U; ++i) {
    const INA3221::ChannelMask bit = static_cast<INA3221::ChannelMask>(1U << i);
    char name[24];
    std::snprintf(name, sizeof(name), "readChannel(CH%u)", static_cast<unsigned>(i + 1U));
    if ((profileBefore.enabledChannels & bit) == 0U || poweredDown) {
      reportSkip(name, poweredDown ? "profile is powered down" : "channel disabled");
      continue;
    }
    INA3221::ChannelMeasurement measurement{};
    st = device.readChannel(static_cast<INA3221::Channel>(i), measurement);
    report(name, st.ok(), st.ok() ? "" : errToStr(st.code));
  }
  INA3221::AlertSnapshot alerts{};
  st = device.peekAlertEvents(alerts);
  report("peekAlertEvents(cache-only)", st.ok(), st.ok() ? "" : errToStr(st.code));
  INA3221::SampleBatch sample{};
  st = device.peekLastSample(sample);
  if (st.ok()) report("peekLastSample(cache-only)", true, "");
  else reportSkip("peekLastSample(cache-only)", "no completed sample retained");
  st = verifyManagedRegisters();
  report("verify 11 managed registers", st.ok(), st.ok() ? "" : errToStr(st.code));
  report("register mismatch count zero", lastVerification.mismatches == 0U, "");
  report("register read failure count zero", lastVerification.readFailures == 0U, "");
  int32_t decoded = 0;
  st = INA3221::INA3221::decodeShuntMicroVolts(0xFFF8U, decoded);
  report("fixed shunt decode", st.ok() && decoded == -40, "");
  st = INA3221::INA3221::decodeBusMilliVolts(0x0008U, decoded);
  report("fixed bus decode", st.ok() && decoded == 8, "");
  report("profile preserved",
         device.isBound() &&
             std::memcmp(&profileBefore, &device.deviceProfile(), sizeof(profileBefore)) == 0,
         "");
  report("profile generation preserved", generationBefore == device.profileGeneration(), "");
  report("driver online", device.isOnline(), "");
  report("consecutive failures zero", device.consecutiveFailures() == 0U, "");
  out("Selftest result: pass=%lu fail=%lu skip=%lu\n",
      static_cast<unsigned long>(pass),
      static_cast<unsigned long>(fail),
      static_cast<unsigned long>(skip));
  if (fail != 0U) {
    printStatus(INA3221::Status::Error(INA3221::Err::PROFILE_MISMATCH,
                                      "Selftest failed", fail));
  }
}

bool parseModeToken(const char* token, INA3221::Mode& mode, bool triggeredOnly) {
  if (std::strcmp(token, "strig") == 0) {
    mode = INA3221::Mode::SHUNT_TRIG;
    return true;
  }
  if (std::strcmp(token, "btrig") == 0) {
    mode = INA3221::Mode::BUS_TRIG;
    return true;
  }
  if (std::strcmp(token, "sbtrig") == 0) {
    mode = INA3221::Mode::SHUNT_BUS_TRIG;
    return true;
  }
  if (triggeredOnly) {
    return false;
  }
  if (std::strcmp(token, "pd") == 0) {
    mode = INA3221::Mode::POWER_DOWN;
    return true;
  }
  if (std::strcmp(token, "pda") == 0) {
    mode = INA3221::Mode::POWER_DOWN_ALT;
    return true;
  }
  if (std::strcmp(token, "sc") == 0) {
    mode = INA3221::Mode::SHUNT_CONT;
    return true;
  }
  if (std::strcmp(token, "bc") == 0) {
    mode = INA3221::Mode::BUS_CONT;
    return true;
  }
  if (std::strcmp(token, "sbc") == 0) {
    mode = INA3221::Mode::SHUNT_BUS_CONT;
    return true;
  }
  return false;
}

void handleChannelRead(const char* args, const char* kind) {
  const int ch = parseChannel(args);
  if (ch < 0) {
    warn("Invalid channel (1-3)");
    return;
  }
  const auto channel = static_cast<INA3221::Channel>(ch);
  if (std::strcmp(kind, "ch") == 0) {
    INA3221::ChannelMeasurement m;
    INA3221::Status st = device.readChannel(channel, m);
    st.ok() ? printChannelMeasurement(ch + 1, m) : printStatus(st);
  } else if (std::strcmp(kind, "shuntraw") == 0) {
    int16_t raw = 0;
    INA3221::Status st = device.readShuntRaw(channel, raw);
    st.ok() ? out("  CH%d shunt raw: %d (%.3f mV)\n",
                  ch + 1, raw,
                  static_cast<double>(INA3221::INA3221::shuntRawToMv(raw)))
            : printStatus(st);
  } else if (std::strcmp(kind, "shunt") == 0) {
    float mV = 0.0f;
    INA3221::Status st = device.readShuntVoltage(channel, mV);
    st.ok() ? out("  CH%d shunt: %.3f mV\n", ch + 1, static_cast<double>(mV))
            : printStatus(st);
  } else if (std::strcmp(kind, "busraw") == 0) {
    int16_t raw = 0;
    INA3221::Status st = device.readBusRaw(channel, raw);
    st.ok() ? out("  CH%d bus raw: %d (%.3f V)\n",
                  ch + 1, raw,
                  static_cast<double>(INA3221::INA3221::busRawToVolts(raw)))
            : printStatus(st);
  } else if (std::strcmp(kind, "bus") == 0) {
    float volts = 0.0f;
    INA3221::Status st = device.readBusVoltage(channel, volts);
    st.ok() ? out("  CH%d bus: %.3f V\n", ch + 1, static_cast<double>(volts))
            : printStatus(st);
  } else if (std::strcmp(kind, "current") == 0) {
    float mA = 0.0f;
    INA3221::Status st = device.readCurrent(channel, mA);
    st.ok() ? out("  CH%d current: %.3f mA\n", ch + 1, static_cast<double>(mA))
            : printStatus(st);
  } else if (std::strcmp(kind, "power") == 0) {
    float mW = 0.0f;
    INA3221::Status st = device.readPower(channel, mW);
    st.ok() ? out("  CH%d power: %.3f mW\n", ch + 1, static_cast<double>(mW))
            : printStatus(st);
  }
}

void handleLimitCommand(const char* args, bool critical) {
  if (args == nullptr || *args == '\0') {
    printAllLimits(critical);
    return;
  }
  char first[24];
  char second[48];
  if (!splitTwoArgs(args, first, sizeof(first), second, sizeof(second))) {
    const int ch = parseChannel(args);
    if (ch < 0) {
      warn("Invalid channel (1-3)");
      return;
    }
    printLimit(critical, static_cast<INA3221::Channel>(ch), ch + 1);
    return;
  }
  const int ch = parseChannel(first);
  int32_t raw = 0;
  if (ch < 0 || !parseI32(second, raw) || raw < -32768 || raw > 32767) {
    warn("Usage: %s <1|2|3> [int16]", critical ? "crit" : "warn");
    return;
  }
  INA3221::Status st =
      critical ? device.setCriticalAlertLimit(static_cast<INA3221::Channel>(ch),
                                              static_cast<int16_t>(raw))
               : device.setWarningAlertLimit(static_cast<INA3221::Channel>(ch),
                                             static_cast<int16_t>(raw));
  printStatus(st);
}

void processCommand(char* line) {
  char* cmd = trim(line);
  if (*cmd == '\0') {
    return;
  }

  const char* arg = nullptr;
  if ((arg = argAfter(cmd, "hilrun ")) != nullptr) {
    char frame[MAX_LINE_LEN];
    std::snprintf(frame, sizeof(frame), "%s", arg);
    char* token = trim(frame);
    char* tokenEnd = std::strchr(token, ' ');
    char* sequence = nullptr;
    char* inner = nullptr;
    if (tokenEnd != nullptr) {
      *tokenEnd = '\0';
      sequence = trim(tokenEnd + 1);
      char* sequenceEnd = std::strchr(sequence, ' ');
      if (sequenceEnd != nullptr) {
        *sequenceEnd = '\0';
        inner = trim(sequenceEnd + 1);
      }
    }
    out("HIL_BEGIN token=%s seq=%s\n", token,
        sequence != nullptr ? sequence : "");
    const uint32_t start = nowMs();
    if (*token == '\0' || sequence == nullptr || *sequence == '\0' ||
        inner == nullptr || *inner == '\0' || startsWith(inner, "hilrun ")) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      out("  Status: INVALID_PARAM (malformed or nested hilrun)\n");
    } else {
      hilCommandStatus = INA3221::Err::OK;
      processCommand(inner);
    }
    out("HIL_END token=%s seq=%s status=%s elapsed_ms=%lu\n", token,
        sequence != nullptr ? sequence : "", errToStr(hilCommandStatus),
        static_cast<unsigned long>(nowMs() - start));
  } else if ((arg = argAfter(cmd, "hilmark ")) != nullptr) {
    if (*trim(const_cast<char*>(arg)) == '\0') {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: hilmark <token>");
    } else {
      out("HILMARK %s\n", arg);
    }
  } else if (std::strcmp(cmd, "xfer_reset") == 0) {
    ina3221IdfResetTransferStats();
    out("XFER_RESET read=0 write=0 total=0\n");
  } else if (std::strcmp(cmd, "xfer_stats") == 0) {
    const Ina3221IdfTransferStats stats = ina3221IdfTransferStats();
    out("XFER_STATS read=%lu write=%lu total=%lu\n",
        static_cast<unsigned long>(stats.read),
        static_cast<unsigned long>(stats.write),
        static_cast<unsigned long>(stats.read + stats.write));
  } else if ((arg = argAfter(cmd, "xfer_assert ")) != nullptr) {
    uint32_t expectedRead = 0;
    uint32_t expectedWrite = 0;
    uint32_t expectedTotal = 0;
    if (!splitThreeArgs(arg, expectedRead, expectedWrite, expectedTotal)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: xfer_assert <read> <write> <total>");
      return;
    }
    const Ina3221IdfTransferStats stats = ina3221IdfTransferStats();
    const uint32_t total = stats.read + stats.write;
    if (stats.read == expectedRead && stats.write == expectedWrite && total == expectedTotal) {
      out("XFER_ASSERT PASS read=%lu write=%lu total=%lu\n",
          static_cast<unsigned long>(stats.read),
          static_cast<unsigned long>(stats.write), static_cast<unsigned long>(total));
    } else {
      hilCommandStatus = INA3221::Err::PROFILE_MISMATCH;
      out("XFER_ASSERT FAIL expected_read=%lu expected_write=%lu expected_total=%lu "
          "read=%lu write=%lu total=%lu\n",
          static_cast<unsigned long>(expectedRead),
          static_cast<unsigned long>(expectedWrite),
          static_cast<unsigned long>(expectedTotal),
          static_cast<unsigned long>(stats.read),
          static_cast<unsigned long>(stats.write), static_cast<unsigned long>(total));
      printStatus(INA3221::Status::Error(
          INA3221::Err::PROFILE_MISMATCH, "Transfer count mismatch",
          static_cast<int32_t>(total)));
    }
  } else if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (std::strcmp(cmd, "version") == 0 || std::strcmp(cmd, "ver") == 0) {
    printVersionInfo();
  } else if (std::strcmp(cmd, "scan") == 0) {
    scanBus();
    scanIna3221Addresses();
  } else if (std::strcmp(cmd, "scanina") == 0) {
    scanIna3221Addresses();
  } else if (std::strcmp(cmd, "job") == 0 || std::strcmp(cmd, "job progress") == 0) {
    printOwnerJobProgress();
  } else if (std::strcmp(cmd, "job init") == 0) {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound; use init"));
      return;
    }
    const INA3221::DeviceProfile profile = device.deviceProfile();
    printStatus(trackOwnerStart(device.startInitialize(
        nextOwnerRequestId(), ownerDeadlineFor(INA3221::JobKind::INITIALIZE, profile))));
  } else if (std::strcmp(cmd, "job apply") == 0) {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    const INA3221::DeviceProfile profile = device.deviceProfile();
    printStatus(trackOwnerStart(device.startApplyProfile(
        profile, nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::APPLY_PROFILE, profile))));
  } else if (std::strcmp(cmd, "job reconcile") == 0) {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    printStatus(trackOwnerStart(device.startReconcile(
        nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::RECONCILE, device.deviceProfile()))));
  } else if (std::strcmp(cmd, "job sample") == 0) {
    printStatus(trackOwnerStart(startOwnerSample()));
  } else if (std::strcmp(cmd, "job continuous") == 0 ||
             (arg = argAfter(cmd, "job continuous ")) != nullptr) {
    bool consumeAlerts = false;
    if (arg != nullptr && !parseBool01(arg, consumeAlerts)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: job continuous [0|1]");
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
                         device.deviceProfile()), consumeAlerts)));
  } else if (std::strcmp(cmd, "job powerdown") == 0) {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    printStatus(trackOwnerStart(device.startPowerDown(
        nextOwnerRequestId(),
        ownerDeadlineFor(INA3221::JobKind::POWER_DOWN, device.deviceProfile()))));
  } else if (std::strcmp(cmd, "job cancel") == 0) {
    // Both operations are cache-only; keep the terminal result inside HIL_END.
    const INA3221::Status st = device.cancelJob();
    printStatus(st);
    if (st.code == INA3221::Err::CANCELLED) serviceOwnerJob();
  } else if (std::strcmp(cmd, "job auto") == 0) {
    out("  Owner auto service: %s\n", ownerAutoService ? "ON" : "OFF");
  } else if ((arg = argAfter(cmd, "job auto ")) != nullptr) {
    bool enabled = false;
    if (!parseBool01(arg, enabled)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: job auto <0|1>");
      return;
    }
    ownerAutoService = enabled;
    out("  Owner auto service: %s\n", ownerAutoService ? "ON" : "OFF");
  } else if ((arg = argAfter(cmd, "job step ")) != nullptr) {
    uint32_t budget = 0;
    if (!parseU32(arg, budget) || budget > 255U) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: job step <0..255>");
      return;
    }
    printStatus(stepOwnerJob(static_cast<uint8_t>(budget)));
    printOwnerJobProgress();
  } else if (std::strcmp(cmd, "job result") == 0) {
    if (lastOwnerResultValid) printOwnerResult(lastOwnerResult);
    else printStatus(INA3221::Status::Error(INA3221::Err::NO_RESULT,
                                            "No cached owner result"));
  } else if (std::strcmp(cmd, "job lastsample") == 0) {
    INA3221::SampleBatch sample{};
    const INA3221::Status st = device.peekLastSample(sample);
    st.ok() ? printOwnerSample(sample) : printStatus(st);
  } else if (std::strcmp(cmd, "job alerts") == 0 ||
             std::strcmp(cmd, "job alerts take") == 0) {
    INA3221::AlertSnapshot alerts{};
    const bool take = std::strcmp(cmd, "job alerts take") == 0;
    const INA3221::Status st = take ? device.takeAlertEvents(alerts)
                                    : device.peekAlertEvents(alerts);
    st.ok() ? printAlertSnapshot(alerts, take ? "Taken Alert Evidence"
                                              : "Retained Alert Evidence")
            : printStatus(st);
  } else if (std::strcmp(cmd, "probe") == 0) {
    info("Probing device without health tracking");
    printStatus(device.probe());
  } else if (std::strcmp(cmd, "drv") == 0) {
    printDriverHealth();
  } else if (std::strcmp(cmd, "diag") == 0) {
    printFullDiagnostics();
  } else if (std::strcmp(cmd, "verify") == 0) {
    out("  Note: Mask/Enable verification consumes CVRF/latched flags; evidence is retained.\n");
    const INA3221::Status st = verifyManagedRegisters();
    printVerificationEvidence();
    printStatus(st);
  } else if (std::strcmp(cmd, "mismatch") == 0) {
    printVerificationEvidence();
  } else if (std::strcmp(cmd, "recover") == 0) {
    info("Attempting recovery");
    printStatus(device.recover());
    printDriverHealth();
  } else if (std::strcmp(cmd, "addr") == 0) {
    out("  Selected INA3221 address: 0x%02X (%s)\n",
        selectedAddress, addressStrap(selectedAddress));
    if (device.isBound()) out("  Active driver address: 0x%02X\n", device.deviceProfile().i2cAddress);
    else out("  Active driver address: none (unbound)\n");
  } else if ((arg = argAfter(cmd, "addr ")) != nullptr) {
    uint8_t address = 0;
    if (!parseAddress(arg, address)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Invalid address; use 0x40-0x43");
      return;
    }
    selectedAddress = address;
    out("  Selected INA3221 address: 0x%02X (%s); run init to apply\n",
        selectedAddress, addressStrap(selectedAddress));
  } else if (std::strcmp(cmd, "init") == 0 ||
             (arg = argAfter(cmd, "init ")) != nullptr) {
    uint8_t address = selectedAddress;
    if (arg != nullptr && !parseAddress(arg, address)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Invalid address; use init [0x40-0x43]");
      return;
    }
    printStatus(initializeDeviceAt(address, true));
  } else if (std::strcmp(cmd, "end") == 0) {
    const INA3221::Status idle = requireIdleOwnerJob();
    if (!idle.ok()) {
      printStatus(idle);
      return;
    }
    retainCurrentProfile();
    device.unbind();
    ownerDemoPhase = OwnerDemoPhase::IDLE;
    startupSamplePending = false;
    out("  Driver unbound; physical device state and I2C bus are unchanged\n");
  } else if (std::strcmp(cmd, "freq") == 0) {
    out("  I2C frequency: %lu Hz\n",
        static_cast<unsigned long>(ina3221IdfTransportContext().frequencyHz));
  } else if ((arg = argAfter(cmd, "freq ")) != nullptr) {
    uint32_t frequency = 0;
    if (!parseU32(arg, frequency)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: freq <10000..400000>");
      return;
    }
    const INA3221::Status st = setExampleFrequency(frequency);
    printStatus(st);
    out("  I2C frequency: %lu Hz\n",
        static_cast<unsigned long>(ina3221IdfTransportContext().frequencyHz));
  } else if (std::strcmp(cmd, "verbose") == 0) {
    info("Verbose mode: %s", verboseMode ? "ON" : "OFF");
  } else if ((arg = argAfter(cmd, "verbose ")) != nullptr) {
    bool enabled = false;
    if (!parseBool01(arg, enabled)) {
      warn("Invalid verbose value (0|1)");
      return;
    }
    verboseMode = enabled;
    info("Verbose mode: %s", verboseMode ? "ON" : "OFF");
  } else if (std::strcmp(cmd, "read") == 0) {
    readAllChannels();
  } else if ((arg = argAfter(cmd, "read ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 10000) {
      warn("Invalid count (1-10000)");
      return;
    }
    for (int32_t i = 0; i < count; ++i) {
      out("--- Reading %ld/%ld ---\n", static_cast<long>(i + 1), static_cast<long>(count));
      readAllChannels();
    }
  } else if ((arg = argAfter(cmd, "ch ")) != nullptr) {
    handleChannelRead(arg, "ch");
  } else if ((arg = argAfter(cmd, "shuntraw ")) != nullptr) {
    handleChannelRead(arg, "shuntraw");
  } else if ((arg = argAfter(cmd, "shunt ")) != nullptr) {
    handleChannelRead(arg, "shunt");
  } else if ((arg = argAfter(cmd, "busraw ")) != nullptr) {
    handleChannelRead(arg, "busraw");
  } else if ((arg = argAfter(cmd, "bus ")) != nullptr) {
    handleChannelRead(arg, "bus");
  } else if ((arg = argAfter(cmd, "current ")) != nullptr) {
    handleChannelRead(arg, "current");
  } else if ((arg = argAfter(cmd, "power ")) != nullptr) {
    handleChannelRead(arg, "power");
  } else if (std::strcmp(cmd, "sum") == 0) {
    float mV = 0.0f;
    INA3221::Status st = device.readShuntSumVoltage(mV);
    st.ok() ? out("  Shunt sum: %.3f mV\n", static_cast<double>(mV)) : printStatus(st);
  } else if (std::strcmp(cmd, "sumraw") == 0) {
    int16_t raw = 0;
    INA3221::Status st = device.readShuntSumRaw(raw);
    st.ok() ? out("  Shunt sum raw: %d\n", raw) : printStatus(st);
  } else if (std::strcmp(cmd, "timing") == 0) {
    printTimingInfo();
  } else if (std::strcmp(cmd, "start") == 0) {
    printStatus(device.startConversion());
  } else if ((arg = argAfter(cmd, "start ")) != nullptr) {
    INA3221::Mode mode = INA3221::Mode::SHUNT_BUS_TRIG;
    if (!parseModeToken(arg, mode, true)) {
      warn("Invalid mode (strig/btrig/sbtrig)");
      return;
    }
    printStatus(device.startConversion(mode));
  } else if (std::strcmp(cmd, "poll") == 0) {
    bool ready = false;
    INA3221::Status st = device.readConversionReady(ready);
    st.ok() ? info("Conversion ready: %s", ready ? "YES" : "NO") : printStatus(st);
  } else if (std::strcmp(cmd, "ids") == 0) {
    uint16_t mfg = 0;
    uint16_t die = 0;
    INA3221::Status s1 = device.readManufacturerId(mfg);
    INA3221::Status s2 = device.readDieId(die);
    if (s1.ok() && s2.ok()) {
      out("  Manufacturer ID: 0x%04X (%s)\n", mfg, (mfg == 0x5449) ? "OK - TI" : "UNEXPECTED");
      out("  Die ID: 0x%04X (%s)\n", die, (die == 0x3220) ? "OK - INA3221" : "UNEXPECTED");
    } else {
      if (!s1.ok()) printStatus(s1);
      if (!s2.ok()) printStatus(s2);
    }
  } else if (std::strcmp(cmd, "mode") == 0) {
    out("  Mode: %s\n", modeToStr(device.getMode()));
  } else if ((arg = argAfter(cmd, "mode ")) != nullptr) {
    INA3221::Mode mode = INA3221::Mode::SHUNT_BUS_CONT;
    if (!parseModeToken(arg, mode, false)) {
      warn("Invalid mode (pd/pda/strig/btrig/sbtrig/sc/bc/sbc)");
      return;
    }
    printStatus(device.setMode(mode));
  } else if (std::strcmp(cmd, "avg") == 0) {
    out("  Averaging: %s samples\n", avgToStr(device.getAveraging()));
  } else if ((arg = argAfter(cmd, "avg ")) != nullptr) {
    int32_t val = 0;
    if (!parseI32(arg, val) || val < 0 || val > 7) {
      warn("Invalid avg (0-7)");
      return;
    }
    printStatus(device.setAveraging(static_cast<INA3221::Averaging>(val)));
  } else if (std::strcmp(cmd, "vbusct") == 0) {
    out("  VbusCT: %s\n", ctToStr(device.getVBusConvTime()));
  } else if ((arg = argAfter(cmd, "vbusct ")) != nullptr) {
    int32_t val = 0;
    if (!parseI32(arg, val) || val < 0 || val > 7) {
      warn("Invalid conv time (0-7)");
      return;
    }
    printStatus(device.setVBusConvTime(static_cast<INA3221::ConvTime>(val)));
  } else if (std::strcmp(cmd, "vshct") == 0) {
    out("  VshCT: %s\n", ctToStr(device.getVShuntConvTime()));
  } else if ((arg = argAfter(cmd, "vshct ")) != nullptr) {
    int32_t val = 0;
    if (!parseI32(arg, val) || val < 0 || val > 7) {
      warn("Invalid conv time (0-7)");
      return;
    }
    printStatus(device.setVShuntConvTime(static_cast<INA3221::ConvTime>(val)));
  } else if (std::strcmp(cmd, "chen") == 0) {
    printChannelEnable();
  } else if ((arg = argAfter(cmd, "chen ")) != nullptr) {
    char a[24];
    char b[24];
    bool enabled = false;
    const int ch = splitTwoArgs(arg, a, sizeof(a), b, sizeof(b)) ? parseChannel(a) : -1;
    if (ch < 0 || !parseBool01(b, enabled)) {
      warn("Usage: chen <1|2|3> <0|1>");
      return;
    }
    printStatus(device.setChannelEnable(static_cast<INA3221::Channel>(ch), enabled));
  } else if (std::strcmp(cmd, "rshunt") == 0) {
    printShuntResistance();
  } else if ((arg = argAfter(cmd, "rshunt ")) != nullptr) {
    char a[24];
    char b[48];
    float ohms = 0.0f;
    const int ch = splitTwoArgs(arg, a, sizeof(a), b, sizeof(b)) ? parseChannel(a) : -1;
    if (ch < 0 || !parseFloatArg(b, ohms) || ohms <= 0.0f) {
      warn("Usage: rshunt <1|2|3> <ohms>");
      return;
    }
    printStatus(device.setShuntResistance(static_cast<INA3221::Channel>(ch), ohms));
  } else if (std::strcmp(cmd, "direction") == 0) {
    if (!device.isBound()) {
      printStatus(INA3221::Status::Error(INA3221::Err::NOT_INITIALIZED,
                                         "Driver is not bound"));
      return;
    }
    const INA3221::DeviceProfile& profile = device.deviceProfile();
    out("  Current direction: CH1=%s CH2=%s CH3=%s\n",
        directionToStr(profile.shunts[0].direction),
        directionToStr(profile.shunts[1].direction),
        directionToStr(profile.shunts[2].direction));
  } else if ((arg = argAfter(cmd, "direction ")) != nullptr) {
    char channelToken[24];
    char directionToken[24];
    bool inverted = false;
    const int ch = splitTwoArgs(arg, channelToken, sizeof(channelToken),
                                directionToken, sizeof(directionToken))
                       ? parseChannel(channelToken) : -1;
    if (ch < 0 || !parseBool01(directionToken, inverted)) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Usage: direction <1|2|3> <0|1>");
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
  } else if (std::strcmp(cmd, "profile") == 0) {
    printDesiredProfile();
  } else if ((arg = argAfter(cmd, "config write ")) != nullptr) {
    uint32_t value = 0;
    if (!parseU32(arg, value) || value > 0xFFFFU) {
      warn("Usage: config write <0..0xFFFF>");
      return;
    }
    INA3221::Status st = device.writeConfig(static_cast<uint16_t>(value));
    printStatus(st);
    if (st.ok()) printConfig();
  } else if (std::strcmp(cmd, "config") == 0) {
    printConfig();
  } else if (std::strcmp(cmd, "cfg") == 0 || std::strcmp(cmd, "settings") == 0) {
    printSettingsSnapshot();
  } else if (std::strcmp(cmd, "reset") == 0) {
    info("Performing software reset");
    printStatus(device.softReset());
  } else if ((arg = argAfter(cmd, "wreg ")) != nullptr) {
    char a[24];
    char b[48];
    uint32_t addr = 0;
    uint32_t value = 0;
    if (!splitTwoArgs(arg, a, sizeof(a), b, sizeof(b)) ||
        !parseU32(a, addr) ||
        !parseU32(b, value) ||
        addr > 0xFFU ||
        value > 0xFFFFU) {
      warn("Usage: wreg <addr> <val>");
      return;
    }
    printStatus(device.writeRegister16(static_cast<uint8_t>(addr), static_cast<uint16_t>(value)));
  } else if ((arg = argAfter(cmd, "reg ")) != nullptr) {
    uint32_t addr = 0;
    if (!parseU32(arg, addr) || addr > 0xFFU) {
      warn("Usage: reg <addr>");
      return;
    }
    uint16_t value = 0;
    INA3221::Status st = device.readRegister16(static_cast<uint8_t>(addr), value);
    st.ok() ? out("  Reg 0x%02lX = 0x%04X (%u)\n",
                  static_cast<unsigned long>(addr), value, value)
            : printStatus(st);
  } else if (std::strcmp(cmd, "alerts") == 0) {
    INA3221::AlertFlags flags;
    INA3221::Status st = device.readAlertFlags(flags);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    out("=== Alert Flags ===\n");
    out("  Critical: CH1=%d CH2=%d CH3=%d\n",
        flags.criticalCh1, flags.criticalCh2, flags.criticalCh3);
    out("  Warning: CH1=%d CH2=%d CH3=%d\n",
        flags.warningCh1, flags.warningCh2, flags.warningCh3);
    out("  Summation=%d PowerValid=%d TimingCtl=%d ConvReady=%d\n",
        flags.summation, flags.powerValid, flags.timingControl, flags.conversionReady);
  } else if (std::strcmp(cmd, "alertsnap") == 0 ||
             std::strcmp(cmd, "alertsnap take") == 0) {
    INA3221::AlertSnapshot snapshot{};
    const bool take = std::strcmp(cmd, "alertsnap take") == 0;
    const INA3221::Status st = take ? device.takeAlertEvents(snapshot)
                                    : device.peekAlertEvents(snapshot);
    st.ok() ? printAlertSnapshot(snapshot, take ? "Taken Alert Evidence"
                                                : "Retained Alert Evidence")
            : printStatus(st);
  } else if (std::strcmp(cmd, "mask") == 0) {
    printMaskEnable();
  } else if (std::strcmp(cmd, "crit") == 0) {
    handleLimitCommand(nullptr, true);
  } else if ((arg = argAfter(cmd, "crit ")) != nullptr) {
    handleLimitCommand(arg, true);
  } else if (std::strcmp(cmd, "warn") == 0) {
    handleLimitCommand(nullptr, false);
  } else if ((arg = argAfter(cmd, "warn ")) != nullptr) {
    handleLimitCommand(arg, false);
  } else if (std::strcmp(cmd, "sumlim") == 0) {
    int16_t raw = 0;
    INA3221::Status st = device.getShuntSumLimit(raw);
    st.ok() ? out("  Shunt sum limit: %d\n", raw) : printStatus(st);
  } else if ((arg = argAfter(cmd, "sumlim ")) != nullptr) {
    int32_t raw = 0;
    if (!parseI32(arg, raw) || raw < -32768 || raw > 32767) {
      warn("Invalid raw value (int16 range)");
      return;
    }
    printStatus(device.setShuntSumLimit(static_cast<int16_t>(raw)));
  } else if (std::strcmp(cmd, "pvhi") == 0) {
    int16_t raw = 0;
    INA3221::Status st = device.getPowerValidUpperLimit(raw);
    st.ok() ? out("  Power valid upper limit: %d (%.3f V)\n",
                  raw,
                  static_cast<double>(INA3221::INA3221::busRawToVolts(raw)))
            : printStatus(st);
  } else if ((arg = argAfter(cmd, "pvhi ")) != nullptr) {
    int32_t raw = 0;
    if (!parseI32(arg, raw) || raw < -32768 || raw > 32767) {
      warn("Invalid raw value (int16 range)");
      return;
    }
    printStatus(device.setPowerValidUpperLimit(static_cast<int16_t>(raw)));
  } else if (std::strcmp(cmd, "pvlo") == 0) {
    int16_t raw = 0;
    INA3221::Status st = device.getPowerValidLowerLimit(raw);
    st.ok() ? out("  Power valid lower limit: %d (%.3f V)\n",
                  raw,
                  static_cast<double>(INA3221::INA3221::busRawToVolts(raw)))
            : printStatus(st);
  } else if ((arg = argAfter(cmd, "pvlo ")) != nullptr) {
    int32_t raw = 0;
    if (!parseI32(arg, raw) || raw < -32768 || raw > 32767) {
      warn("Invalid raw value (int16 range)");
      return;
    }
    printStatus(device.setPowerValidLowerLimit(static_cast<int16_t>(raw)));
  } else if (std::strcmp(cmd, "sumch") == 0) {
    printMaskEnable();
  } else if ((arg = argAfter(cmd, "sumch ")) != nullptr) {
    char a[24];
    char b[24];
    bool enabled = false;
    const int ch = splitTwoArgs(arg, a, sizeof(a), b, sizeof(b)) ? parseChannel(a) : -1;
    if (ch < 0 || !parseBool01(b, enabled)) {
      warn("Usage: sumch <1|2|3> <0|1>");
      return;
    }
    uint16_t mask = 0;
    INA3221::Status st = device.readRegister16(INA3221::cmd::REG_MASK_ENABLE, mask);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    bool ch1 = (mask & INA3221::cmd::MASK_SCC1) != 0;
    bool ch2 = (mask & INA3221::cmd::MASK_SCC2) != 0;
    bool ch3 = (mask & INA3221::cmd::MASK_SCC3) != 0;
    if (ch == 0) ch1 = enabled;
    if (ch == 1) ch2 = enabled;
    if (ch == 2) ch3 = enabled;
    printStatus(device.setSummationChannels(ch1, ch2, ch3));
  } else if (std::strcmp(cmd, "latch") == 0) {
    printMaskEnable();
  } else if ((arg = argAfter(cmd, "latch ")) != nullptr) {
    char a[24];
    char b[24];
    bool warnLatch = false;
    bool critLatch = false;
    if (!splitTwoArgs(arg, a, sizeof(a), b, sizeof(b)) ||
        !parseBool01(a, warnLatch) ||
        !parseBool01(b, critLatch)) {
      warn("Usage: latch <warn 0|1> <crit 0|1>");
      return;
    }
    printStatus(device.setAlertLatchEnable(warnLatch, critLatch));
  } else if (std::strcmp(cmd, "online") == 0) {
    info("Online: %s", device.isOnline() ? "YES" : "NO");
  } else if (std::strcmp(cmd, "selftest") == 0) {
    runSelfTest();
  } else if (std::strcmp(cmd, "stress_owner") == 0) {
    runOwnerStress(10);
  } else if ((arg = argAfter(cmd, "stress_owner ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 10000) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Invalid count (1-10000)");
      return;
    }
    runOwnerStress(static_cast<int>(count));
  } else if (std::strcmp(cmd, "stress_freq") == 0) {
    runFrequencyStress(10);
  } else if ((arg = argAfter(cmd, "stress_freq ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 10000) {
      hilCommandStatus = INA3221::Err::INVALID_PARAM;
      warn("Invalid count (1-10000)");
      return;
    }
    runFrequencyStress(static_cast<int>(count));
  } else if (std::strcmp(cmd, "stress_mix") == 0) {
    runStressMix(50);
  } else if ((arg = argAfter(cmd, "stress_mix ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 100000) {
      warn("Invalid count (1-100000)");
      return;
    }
    runStressMix(static_cast<int>(count));
  } else if (std::strcmp(cmd, "stress") == 0) {
    runStress(10);
  } else if ((arg = argAfter(cmd, "stress ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 100000) {
      warn("Invalid count (1-100000)");
      return;
    }
    runStress(static_cast<int>(count));
  } else if ((arg = argAfter(cmd, "convert shunt ")) != nullptr) {
    int32_t raw = 0;
    if (!parseI32(arg, raw) || raw < -32768 || raw > 32767) {
      warn("Invalid raw value (int16 range)");
      return;
    }
    out("  Shunt raw %ld = %.3f mV\n",
        static_cast<long>(raw),
        static_cast<double>(INA3221::INA3221::shuntRawToMv(static_cast<int16_t>(raw))));
  } else if ((arg = argAfter(cmd, "convert bus ")) != nullptr) {
    int32_t raw = 0;
    if (!parseI32(arg, raw) || raw < -32768 || raw > 32767) {
      warn("Invalid raw value (int16 range)");
      return;
    }
    out("  Bus raw %ld = %.3f V\n",
        static_cast<long>(raw),
        static_cast<double>(INA3221::INA3221::busRawToVolts(static_cast<int16_t>(raw))));
  } else {
    hilCommandStatus = INA3221::Err::INVALID_PARAM;
    warn("Unknown command: %s", cmd);
  }
}

void setupExample() {
  info("=== INA3221 Native ESP-IDF Bringup Example ===");
  if (!initBus(activeAddress)) {
    return;
  }
  scanBus();
  const INA3221::TransportConfig transportConfig = makeOwnerTransport();
  const INA3221::DeviceProfile profile = makeOwnerProfile(activeAddress);
  retainedProfile = profile;
  retainedProfileValid = true;
  selectedAddress = activeAddress;
  INA3221::Status st = device.bind(transportConfig, profile);
  if (!st.ok()) {
    error("Failed to bind device contracts");
    printStatus(st);
    return;
  }
  ++ownerRequestId;
  const uint64_t now = ownerNowMs();
  const uint64_t initializeDeadline =
      ownerDeadlineFor(INA3221::JobKind::INITIALIZE, profile);
  st = device.startInitialize(ownerRequestId, initializeDeadline);
  if (!st.inProgress()) {
    error("Failed to start staged initialization");
    printStatus(st);
    device.unbind();
    return;
  }
  ownerDemoPhase = OwnerDemoPhase::ACTIVE;
  ownerAutoService = true;
  startupSamplePending = true;
  info("Staged initialization started (budget=1 transfer/poll)");

  // Finish the bring-up demonstration before entering the shell. The outer
  // wait is bounded; every service call still admits at most one I2C callback.
  const uint64_t sampleDeadline = ownerDeadlineFor(
      INA3221::JobKind::TRIGGERED_SAMPLE, profile, profile.mode);
  const uint64_t sampleBudget = sampleDeadline > now
                                    ? sampleDeadline - now
                                    : OWNER_JOB_TIMEOUT_MS;
  const uint64_t demoDeadline = initializeDeadline + sampleBudget + 250U;
  while (ownerDemoPhase != OwnerDemoPhase::IDLE && ownerNowMs() < demoDeadline) {
    serviceOwnerJob();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  if (ownerDemoPhase != OwnerDemoPhase::IDLE) {
    (void)device.cancelJob();
    serviceOwnerJob();  // Cache-only take of the cancellation result.
  }
}

}  // namespace

extern "C" void app_main(void) {
  setupExample();
  const int stdinFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (stdinFlags >= 0) {
    (void)fcntl(STDIN_FILENO, F_SETFL, stdinFlags | O_NONBLOCK);
  }
  out("\nType 'help' for commands\n");
  prompt();

  char line[MAX_LINE_LEN]{};
  size_t lineLength = 0;
  bool lineOverflow = false;
  while (true) {
    serviceOwnerJob();
    if (ownerDemoPhase == OwnerDemoPhase::IDLE && device.isInitialized()) {
      (void)device.tickStatus(nowMs());
    }
    bool commandHandled = false;
    for (size_t bytes = 0; bytes < MAX_LINE_LEN + 1U; ++bytes) {
      char c = '\0';
      const ssize_t received = read(STDIN_FILENO, &c, 1U);
      if (received <= 0) break;
      if (c == '\n' || c == '\r') {
        const bool hadLine = lineOverflow || lineLength > 0U;
        if (lineOverflow) {
          warn("Input line exceeds 127 bytes; command rejected");
        } else if (lineLength > 0U) {
          line[lineLength] = '\0';
          processCommand(line);
        }
        lineLength = 0;
        lineOverflow = false;
        if (hadLine) prompt();
        commandHandled = hadLine;
        break;  // Service the owner once between queued commands.
      }
      if (lineOverflow) continue;
      if (lineLength + 1U < sizeof(line)) {
        line[lineLength++] = c;
      } else {
        lineOverflow = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(commandHandled ? 1 : 10));
  }
}
