/// @file main.cpp
/// @brief Native ESP-IDF INA3221 bringup CLI.

#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

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

INA3221::INA3221 device;
bool verboseMode = false;
uint8_t activeAddress = DEFAULT_ADDR;

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
  out("  Status: %s (code=%u, detail=%ld)\n",
      errToStr(st.code),
      static_cast<unsigned>(st.code),
      static_cast<long>(st.detail));
  if (st.msg != nullptr && st.msg[0] != '\0') {
    out("  Message: %s\n", st.msg);
  }
}

INA3221::Config makeConfig(uint8_t address) {
  INA3221::Config cfg;
  cfg.i2cWrite = ina3221IdfI2cWrite;
  cfg.i2cWriteRead = ina3221IdfI2cWriteRead;
  cfg.i2cUser = &ina3221IdfTransportContext();
  cfg.nowMs = ina3221IdfNowMs;
  cfg.cooperativeYield = ina3221IdfYield;
  cfg.timeUser = &ina3221IdfTransportContext();
  cfg.i2cAddress = address;
  cfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  cfg.offlineThreshold = 5;
  cfg.shuntResistance[0] = 0.1f;
  cfg.shuntResistance[1] = 0.1f;
  cfg.shuntResistance[2] = 0.1f;
  return cfg;
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
  out("  Last OK: %lu\n", static_cast<unsigned long>(device.lastOkMs()));
  out("  Last error: %lu\n", static_cast<unsigned long>(device.lastErrorMs()));
  const INA3221::Status last = device.lastError();
  if (!last.ok()) {
    out("  Last error code: %s\n", errToStr(last.code));
    if (last.msg != nullptr && last.msg[0] != '\0') {
      out("  Last error msg: %s\n", last.msg);
    }
  }
}

void printHelpItem(const char* cmd, const char* desc) {
  out("  %-32s %s\n", cmd, desc);
}

void printHelp() {
  out("\n=== INA3221 CLI Help ===\n");
  printHelpItem("help / ?", "Show this help");
  printHelpItem("version / ver", "Print firmware and library version info");
  printHelpItem("scan", "Scan I2C bus");
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
  printHelpItem("mode [pd|strig|btrig|sbtrig|sc|bc|sbc]", "Set/show mode");
  printHelpItem("avg [0..7]", "Set/show averaging");
  printHelpItem("vbusct [0..7]", "Set/show bus conversion time");
  printHelpItem("vshct [0..7]", "Set/show shunt conversion time");
  printHelpItem("chen [<1|2|3> <0|1>]", "Show/set channel enable");
  printHelpItem("rshunt [<1|2|3> <ohms>]", "Show/set shunt resistance");
  printHelpItem("config", "Dump config register");
  printHelpItem("config write <hex>", "Write full config register");
  printHelpItem("reset", "Software reset");
  printHelpItem("reg <addr>", "Read 16-bit register");
  printHelpItem("wreg <addr> <val>", "Write 16-bit register");
  printHelpItem("alerts", "Read alert flags");
  printHelpItem("mask", "Read/decode Mask/Enable register");
  printHelpItem("crit [<1|2|3> [raw]]", "Show/set critical alert limit");
  printHelpItem("warn [<1|2|3> [raw]]", "Show/set warning alert limit");
  printHelpItem("sumlim [raw]", "Get/set shunt sum limit");
  printHelpItem("pvhi [raw]", "Get/set power-valid upper limit");
  printHelpItem("pvlo [raw]", "Get/set power-valid lower limit");
  printHelpItem("sumch [<1|2|3> <0|1>]", "Show/set summation channels");
  printHelpItem("latch [<warn> <crit>]", "Show/set alert latches");
  printHelpItem("drv", "Show driver state and health");
  printHelpItem("probe", "Probe device without health tracking");
  printHelpItem("recover", "Manual recovery attempt");
  printHelpItem("online", "Show online state");
  printHelpItem("cfg / settings", "Print cached configuration snapshot");
  printHelpItem("verbose [0|1]", "Enable/disable verbose output");
  printHelpItem("stress [N]", "Run N measurement cycles");
  printHelpItem("stress_mix [N]", "Run N mixed-operation cycles");
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
  }
  if (!stressStats.lastError.ok()) {
    out("  Last error: %s\n", errToStr(stressStats.lastError.code));
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

  out("=== INA3221 selftest (safe commands) ===\n");
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
  INA3221::ChannelMeasurement m;
  st = device.readChannel(INA3221::Channel::CH1, m);
  report("readChannel(CH1)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setMode(INA3221::Mode::SHUNT_BUS_CONT);
  report("setMode(SHUNT_BUS_CONT)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setAveraging(INA3221::Averaging::AVG_1);
  report("setAveraging(1)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.recover();
  report("recover", st.ok(), st.ok() ? "" : errToStr(st.code));
  report("isOnline", device.isOnline(), "");
  out("Selftest result: pass=%lu fail=%lu skip=%lu\n",
      static_cast<unsigned long>(pass),
      static_cast<unsigned long>(fail),
      static_cast<unsigned long>(skip));
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
  if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (std::strcmp(cmd, "version") == 0 || std::strcmp(cmd, "ver") == 0) {
    printVersionInfo();
  } else if (std::strcmp(cmd, "scan") == 0) {
    scanBus();
  } else if (std::strcmp(cmd, "probe") == 0) {
    info("Probing device without health tracking");
    printStatus(device.probe());
  } else if (std::strcmp(cmd, "drv") == 0) {
    printDriverHealth();
  } else if (std::strcmp(cmd, "recover") == 0) {
    info("Attempting recovery");
    printStatus(device.recover());
    printDriverHealth();
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
      warn("Invalid mode (pd/strig/btrig/sbtrig/sc/bc/sbc)");
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
    warn("Unknown command: %s", cmd);
  }
}

void setupExample() {
  info("=== INA3221 Native ESP-IDF Bringup Example ===");
  if (!initBus(activeAddress)) {
    return;
  }
  scanBus();
  INA3221::Config cfg = makeConfig(activeAddress);
  INA3221::Status st = device.begin(cfg);
  if (!st.ok()) {
    error("Failed to initialize device");
    printStatus(st);
    return;
  }
  info("Device initialized successfully");
  printDriverHealth();
}

}  // namespace

extern "C" void app_main(void) {
  setupExample();
  out("\nType 'help' for commands\n");
  prompt();

  char line[MAX_LINE_LEN];
  while (true) {
    device.tick(nowMs());
    if (std::fgets(line, sizeof(line), stdin) == nullptr) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    processCommand(line);
    prompt();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
