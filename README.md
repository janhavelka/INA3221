# INA3221 Driver Library

Production-grade INA3221 triple-channel power monitor I2C driver for
ESP32-S2 / ESP32-S3 (Arduino framework, PlatformIO, and ESP-IDF component use).

Library version: `v1.2.0`

## Features

- Injected I2C transport (no Wire dependency in library code)
- Framework-neutral core (`include/` and `src/` do not include Arduino or ESP-IDF driver headers)
- Health monitoring with READY / DEGRADED / OFFLINE states
- Triple-channel shunt and bus voltage measurement
- Current and power calculation from configurable shunt resistance
- Shunt-voltage summation register readout
- Tracked single-shot and continuous conversion modes
- Configurable averaging (1–1024 samples), conversion time (140 µs–8.244 ms)
- Alert support: critical/warning limits, power-valid window, summation alert
- Manufacturer ID (0x5449) and Die ID (0x3220) verification

## Installation

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
  https://github.com/janhavelka/INA3221.git#v1.2.0
```

### Manual

Copy `include/INA3221/` and `src/` into your project.

### ESP-IDF

The repository root is an ESP-IDF component. Add it through `EXTRA_COMPONENT_DIRS`
or component manager metadata, then provide `Config::i2cWrite`,
`Config::i2cWriteRead`, `Config::nowMs`, and optionally
`Config::cooperativeYield` from your application-owned adapter. The native
example in `examples/esp_idf/basic` uses ESP-IDF `driver/i2c_master.h` glue and
implements the same bring-up CLI command surface natively with `app_main`,
`esp_timer`, FreeRTOS waits, and fixed C buffers.

## Release 1.2.0 Highlights

- Adds ESP-IDF component metadata and root CMake support for component use.
- Adds the native ESP-IDF `examples/esp_idf/basic` CLI using `driver/i2c_master.h`, `app_main`, `esp_timer`, FreeRTOS waits, and fixed C buffers.
- Preserves Arduino and ESP-IDF user-visible CLI parity for scan/probe, three-channel measurement, conversion control, alert limits, raw register diagnostics, stress, and self-test workflows.
- Keeps the driver core framework-neutral; I2C, timing, and cooperative-yield behavior remain callback-injected by the application.
- Includes hardware validation of the ESP-IDF port before release.

## Quick Start

```cpp
#include <Wire.h>
#include "INA3221/INA3221.h"

// Transport callbacks
INA3221::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                         uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;
  wire->beginTransmission(addr);
  wire->write(data, len);
  switch (wire->endTransmission(true)) {
    case 0: return INA3221::Status::Ok();
    case 2: return INA3221::Status::Error(INA3221::Err::I2C_NACK_ADDR, "Address NACK");
    case 3: return INA3221::Status::Error(INA3221::Err::I2C_NACK_DATA, "Data NACK");
    case 5: return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT, "I2C timeout");
    case 4: return INA3221::Status::Error(INA3221::Err::I2C_BUS, "I2C bus error");
    default: return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "Write failed");
  }
}

INA3221::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                             uint8_t* rx, size_t rxLen,
                             uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;
  wire->beginTransmission(addr);
  wire->write(tx, txLen);
  uint8_t result = wire->endTransmission(false);
  if (result != 0) {
    return INA3221::Status::Error(
      result == 2 ? INA3221::Err::I2C_NACK_ADDR :
      result == 3 ? INA3221::Err::I2C_NACK_DATA :
      result == 5 ? INA3221::Err::I2C_TIMEOUT :
      result == 4 ? INA3221::Err::I2C_BUS :
                    INA3221::Err::I2C_ERROR,
      "Write phase failed");
  }
  if (wire->requestFrom(addr, rxLen) != rxLen) {
    return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "Read failed");
  }
  for (size_t i = 0; i < rxLen; ++i) {
    rx[i] = wire->read();
  }
  return INA3221::Status::Ok();
}

INA3221::INA3221 device;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  INA3221::Config cfg;
  cfg.i2cWrite = i2cWrite;
  cfg.i2cWriteRead = i2cWriteRead;
  cfg.i2cUser = &Wire;
  cfg.nowMs = [](void*) { return millis(); };
  cfg.cooperativeYield = [](void*) { yield(); };
  cfg.i2cAddress = 0x40;
  cfg.shuntResistance[0] = 0.1f;  // Channel 1: 100 mΩ
  cfg.shuntResistance[1] = 0.1f;  // Channel 2: 100 mΩ
  cfg.shuntResistance[2] = 0.1f;  // Channel 3: 100 mΩ

  auto status = device.begin(cfg);
  if (!status.ok()) {
    Serial.printf("Init failed: %s\n", status.msg);
    return;
  }

  Serial.println("INA3221 initialized!");
}

void loop() {
  device.tick(millis());

  INA3221::ChannelMeasurement m;
  if (device.readChannel(INA3221::Channel::CH1, m).ok()) {
    Serial.printf("CH1: %.1f mV shunt, %.3f V bus, %.1f mA, %.1f mW\n",
                  m.shuntVoltage_mV, m.busVoltage_V, m.current_mA, m.power_mW);
  }

  delay(1000);
}
```

## API Overview

### Lifecycle And Diagnostics

| Method | Description |
|--------|-------------|
| `begin(config)` | Initialize with injected transport and verify manufacturer / die ID |
| `tick(nowMs)` | Process bounded conversion polling work |
| `end()` | Best-effort power the monitor down and clear cached conversion state |
| `isInitialized()` | True after successful `begin()` until `end()` |
| `getConfig()` | Return the driver's cached configuration snapshot |
| `getSettings(snap)` | Populate a `SettingsSnapshot` with cached config, conversion, Mask/Enable, and health state without I2C |
| `probe()` | Check device presence without updating health counters |
| `recover()` | Re-validate IDs, clear conversion state, and re-apply cached config / mask settings |

### Measurement And Conversion API

| Method | Description |
|--------|-------------|
| `readShuntRaw()` / `readBusRaw()` | Read raw per-channel voltage registers |
| `readShuntVoltage()` / `readBusVoltage()` | Read scaled shunt mV and bus V |
| `readCurrent()` / `readPower()` | Calculate current and power from configured shunt resistance |
| `readChannel()` | Read shunt, bus, current, and power for one channel |
| `readShuntSumRaw()` / `readShuntSumVoltage()` | Read shunt-voltage summation register |
| `startConversion()` | Trigger a single-shot conversion in the configured triggered mode |
| `startConversion(mode)` | Trigger a single-shot conversion using `SHUNT_TRIG`, `BUS_TRIG`, or `SHUNT_BUS_TRIG` |
| `readConversionReady(ready)` | Read CVRF with Status propagation |
| `conversionReady()` | Convenience bool wrapper that returns false on errors |
| `readBlocking()` | Bounded helper that starts/waits/reads without using `delay()` |

### Raw Access And Compatibility Aliases

| Method | Description |
|--------|-------------|
| `readRegister16(reg, value)` | Read a tracked 16-bit register; valid addresses are `0x00`-`0x11`, `0xFE`, `0xFF` |
| `writeRegister16(reg, value)` | Write a tracked 16-bit register; invalid addresses are rejected before I2C |
| `setVbusConvTime()` / `getVbusConvTime()` | Cross-library naming aliases for the bus conversion-time API |
| `setVshuntConvTime()` / `getVshuntConvTime()` | Cross-library naming aliases for the shunt conversion-time API |

## Configuration

| Field | Default | Description |
|-------|---------|-------------|
| `i2cAddress` | `0x40` | I2C address (0x40–0x43 via A0 pin) |
| `ch1Enable` / `ch2Enable` / `ch3Enable` | `true` | Enable per-channel measurement |
| `averaging` | `AVG_1` | Number of averages (1, 4, 16, 64, 128, 256, 512, 1024) |
| `vBusCt` | `CT_1100US` | Bus voltage conversion time |
| `vShCt` | `CT_1100US` | Shunt voltage conversion time |
| `mode` | `SHUNT_BUS_CONT` | Operating mode (power-down, triggered, continuous) |
| `shuntResistance[3]` | `{0.1, 0.1, 0.1}` | Per-channel shunt resistance in ohms |
| `offlineThreshold` | `5` | Consecutive I2C failures before OFFLINE state |

### Operating Modes

| Mode | Description |
|------|-------------|
| `POWER_DOWN` | Lowest power, no conversions |
| `SHUNT_TRIG` | Single-shot shunt voltage only |
| `BUS_TRIG` | Single-shot bus voltage only |
| `SHUNT_BUS_TRIG` | Single-shot shunt + bus |
| `SHUNT_CONT` | Continuous shunt voltage only |
| `BUS_CONT` | Continuous bus voltage only |
| `SHUNT_BUS_CONT` | Continuous shunt + bus (default) |

Writing a triggered mode to the Configuration register starts a hardware single-shot conversion. The driver tracks that side effect: `begin()` with a triggered mode, `setMode()` to a triggered mode, `writeConfig()` with triggered mode bits, and `startConversion()` all mark the conversion in progress. Measurement APIs return `CONVERSION_NOT_READY` until the configured conversion/averaging time has elapsed and CVRF is observed.

`readConversionReady()` and `readAlertFlags()` read the Mask/Enable register. Per INA3221 register semantics, that read clears CVRF and latched alert flags. The driver caches a ready triggered conversion before CVRF is cleared so subsequent measurement reads are still allowed.

Configuration setters update the cached `Config` only after their I2C write succeeds. On a failed write, the previous cached mode, conversion settings, channel enables, and conversion state are restored.
Alert-limit setters mask reserved bits before writing device registers. The
Mask/Enable writable-bit cache is preserved across configuration writes, and
`writeConfig()` with the reset bit set synchronizes cached settings back to the
device defaults.

### Conversion Times

`CT_140US`, `CT_204US`, `CT_332US`, `CT_588US`, `CT_1100US` (default),
`CT_2116US`, `CT_4156US`, `CT_8244US`

### Averaging

`AVG_1` (default), `AVG_4`, `AVG_16`, `AVG_64`, `AVG_128`, `AVG_256`,
`AVG_512`, `AVG_1024`

## Behavioral Contracts

1. **Threading**: Single-threaded. Call `tick()` and all API from the same context.
2. **Timing**: `tick()` does bounded work only (checks CVRF flag). All waits use deadline math, never `delay()`.
3. **Resource ownership**: I2C bus is NOT owned by the library. Transport callbacks are injected via `Config`.
4. **Framework boundary**: Core code does not call `Wire`, `Serial`, `delay()`, `yield()`, `millis()`, or ESP-IDF peripheral APIs directly. Arduino examples and native ESP-IDF examples provide those hooks externally.
5. **Memory**: All allocation happens in `begin()`. Zero heap allocation in steady state.
6. **Error handling**: Every fallible API returns `Status`. Check with `status.ok()`.
7. **Health**: `OFFLINE` is latched. Normal public I2C operations return `BUSY` with `Driver is offline; call recover()` without touching the bus until `recover()` succeeds.

## Examples

- `examples/01_basic_bringup_cli/` - interactive CLI for all INA3221 features
- `examples/esp_idf/basic/` - native ESP-IDF entry point with the full bring-up
  CLI command surface implemented without Arduino compatibility facades.
- Startup and `scan` diagnostics identify INA3221 devices on `0x40`-`0x43` by reading Manufacturer ID `0x5449` and Die ID `0x3220`, including the corresponding A0 strap label.
- CLI diagnostics include `cfg` / `settings` for cached settings, `mask` for decoded Mask/Enable state, and `reg <addr>` / `wreg <addr> <val>` for tracked raw register access. Bare `chen`, `rshunt`, `crit`, `warn`, `sumch`, and `latch` show current settings; adding arguments updates those settings.
- `stress` reports per-channel measurement statistics. `stress_mix` reports high-level operation counts plus `Health delta (tracked I2C)`, which is the driver's tracked transport success/failure counter delta and can be larger than the high-level operation count.
- CLI numeric and `0|1` arguments are parsed strictly; malformed input is rejected instead of silently becoming zero. Raw register writes are intended for diagnostics and can desync cached config until `recover()` or `begin()` reapplies it.

### Example Helpers (`examples/common/`)

Not part of the library. These simulate project-level glue and keep examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Arduino example pin definitions and I2C init |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Arduino `Wire` transport adapter |
| `I2cScanner.h` | Arduino I2C bus scanner with table output and INA3221 identity recognition on `0x40`-`0x43` |
| `BusDiag.h` | Bus diagnostics wrapper |
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## Validation

```bash
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python scripts/generate_version.py check
pio test -e native
pio run -e esp32s3dev
pio run -e esp32s2dev
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

## Documentation

- `CHANGELOG.md` — full release history
- `docs/IDF_PORT.md` — ESP-IDF portability guidance
- `docs/IDF_PORT_IMPLEMENTATION.md` — ESP-IDF implementation notes and validation status
- `INA3221_triple_power_monitor_implementation_manual.md` — implementation reference

## License

MIT License. See `LICENSE`.
